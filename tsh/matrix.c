#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <time.h>
#include <sys/socket.h> // Include for socket functions

// These would normally be in a separate header file (matrix.h)
#define TILE_SIZE 2
#define KEY_BUF 32
#define VAL_BUF 32
#define MAX_RETRIES 3   // Maximum number of retries for tuple operations
#define TIMEOUT_SEC 5   // Increased timeout for larger matrices
                        //tune this
// Global variables
int N;                  // Matrix size
jmp_buf worker_env;     // For timeout longjmp
static pid_t *worker_pids = NULL;
static int num_workers = 0;

// Structure to store tuple data with a sequence number for redundancy handling
typedef struct {
    char key[KEY_BUF];
    char value[VAL_BUF];
    int  seq_num;
} tuple_t;

// Placeholder functions for tuple space operations with retry and timeout
extern int  ConnectPORT(int port);
extern void TshPUT(const char *key, size_t n, const char *val);
extern void TshGET(const char *key, char *val);
extern void TshREAD(const char *key, char *val);

// Function to print a matrix
void printMatrix(int matrix[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%4d ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// Function to initialize a matrix and publish it to tuple space
void initializeMatrix(char name, int matrix[N][N], int initValue) {
    char key[KEY_BUF];
    char val[VAL_BUF];
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            matrix[i][j] = initValue + i + j;
            snprintf(val, sizeof val, "%d", matrix[i][j]);
            snprintf(key, sizeof key, "%c_%d_%d", name, i, j);
            TshPUT(key, strlen(val) + 1, val);
        }
    }
}

// Function for the master process to collect results and print matrix C
void master_collect(void) {
    char key[KEY_BUF];
    char val[VAL_BUF];
    int (*C)[N] = malloc(N * sizeof(int[N])); // Dynamically allocate C
    if (!C) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(C, 0, N * N * sizeof(int));

    int received_count = 0;
    int expected_count = N * N;
    tuple_t received_tuples[expected_count]; // Store received tuples
    int next_seq_num = 0; // Declare next_seq_num here

    while (received_count < expected_count) {
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                snprintf(key, sizeof key, "C_%d_%d", i, j);
                TshGET(key, val);

                // Check for redundant tuples
                int found = 0;
                for (int k = 0; k < received_count; ++k) {
                    if (strcmp(received_tuples[k].key, key) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    strcpy(received_tuples[received_count].key, key);
                    strcpy(received_tuples[received_count].value, val);
                    received_tuples[received_count].seq_num = next_seq_num++;
                    received_count++;
                    C[i][j] = atoi(val);
                }
            }
        }
    }

    printf("Result C:\n");
    printMatrix(C);
    free(C); // Free dynamically allocated memory
}

// Signal handler for SIGALRM in worker processes
void alarm_handler(int sig) {
    (void)sig;
    // jump back to retry point
    longjmp(worker_env, 1);
}

// Function for worker processes to compute assigned blocks
void worker_compute(int worker_id, int num_workers, int block_size) {
    char key[KEY_BUF];
    char valA[VAL_BUF], valB[VAL_BUF], valC[VAL_BUF];
    int num_blocks = N / block_size;
    int total_blocks = num_blocks * num_blocks;
    int worker_block_index; // Declare worker_block_index here

    signal(SIGALRM, alarm_handler);

    for (worker_block_index = worker_id; worker_block_index < total_blocks; worker_block_index += num_workers) {
        int brow = worker_block_index / num_blocks;
        int bcol = worker_block_index % num_blocks;
        int row0 = brow * block_size;
        int col0 = bcol * block_size;

        for (int i = row0; i < row0 + block_size && i < N; ++i) {
            for (int j = col0; j < col0 + block_size && j < N; ++j) {
                int sum = 0;
                for (int k = 0; k < N; ++k) {
                    // READ A[i][k]
                    snprintf(key, sizeof key, "A_%d_%d", i, k);
                    if (setjmp(worker_env) == 0) {
                        alarm(TIMEOUT_SEC);
                        TshREAD(key, valA);
                        alarm(0);
                    } else {
                        // retry on timeout
                        TshREAD(key, valA);
                    }

                    // READ B[k][j]
                    snprintf(key, sizeof key, "B_%d_%d", k, j);
                    if (setjmp(worker_env) == 0) {
                        alarm(TIMEOUT_SEC);
                        TshREAD(key, valB);
                        alarm(0);
                    } else {
                        TshREAD(key, valB);
                    }

                    sum += atoi(valA) * atoi(valB);
                }

                // PUT C[i][j]
                snprintf(valC, sizeof valC, "%d", sum);
                snprintf(key, sizeof key, "C_%d_%d", i, j);
                if (setjmp(worker_env) == 0) {
                    alarm(TIMEOUT_SEC);
                    TshPUT(key, strlen(valC) + 1, valC);
                    alarm(0);
                } else {
                    TshPUT(key, strlen(valC) + 1, valC);
                }
            }
        }
        printf("Worker %d: Finished block (%d, %d)\n", getpid(), brow, bcol);
    }
    printf("Worker %d finished all assigned blocks.\n", getpid());
}

// Chaos monkey: randomly kill a worker every 10 seconds
void chaos_monkey_loop(void) {
    srand(time(NULL) ^ getpid());
    while (1) {
        sleep(1 + rand() % 10);
        int idx = rand() % num_workers;
        pid_t victim = worker_pids[idx];
        if (victim > 1) {
            kill(victim, SIGKILL);
            fprintf(stderr, "[chaos] killed worker %d (PID %d)\n", idx, victim);
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_workers> <port> <matrix_size>\n", argv[0]);
        return 1;
    }

    struct timespec t0, t1;

    // 1) mark start
    clock_gettime(CLOCK_MONOTONIC, &t0);
    num_workers = atoi(argv[1]);
    int port = atoi(argv[2]);
    N = atoi(argv[3]);

    if (num_workers < 1 || num_workers > N * N) {
        fprintf(stderr, "num_workers must be between 1 and %d\n", N * N);
        return 1;
    }
    if (N < 1) {
        fprintf(stderr, "Matrix size must be at least 1\n");
        return 1;
    }

    ConnectPORT(port);

    // Allocate matrices dynamically using malloc
    int (*A)[N] = malloc(N * sizeof(int[N]));
    int (*B)[N] = malloc(N * sizeof(int[N]));
    if (!A || !B) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    initializeMatrix('A', A, 1);
    initializeMatrix('B', B, 2);
    printf("Matrix A (size %d):\n", N);
    printMatrix(A);
    printf("Matrix B (size %d):\n", N);
    printMatrix(B);

    int block_size = TILE_SIZE;

    pid_t pids[num_workers];
    for (int w = 0; w < num_workers; ++w) {
        pids[w] = fork();
        if (pids[w] < 0) {
            perror("fork failed");
            // Terminate already started children? Cleanup?
            for (int k = 0; k < w; ++k) {
                kill(pids[k], SIGTERM);
            }
            exit(EXIT_FAILURE);
        }
        if (pids[w] == 0) {
            // Declare jmp_buf here, inside the worker process's scope.
            jmp_buf worker_env;
            worker_compute(w, num_workers, block_size);
            exit(EXIT_SUCCESS);
        }
        // Parent process
        printf("Worker %d created with PID %d\n", w, pids[w]);
    }
     // Initialize the global worker_pids array
    worker_pids = malloc(num_workers * sizeof(pid_t));
     if (!worker_pids) {
        perror("malloc");
        exit(EXIT_FAILURE);
     }
    for(int i = 0; i < num_workers; i++){
       worker_pids[i] = pids[i];
    }

    // fork chaos‐monkey
    pid_t chaos_pid = fork();
    if (chaos_pid < 0) {
        perror("fork chaos");
        exit(1);
    }
    if (chaos_pid == 0) {
        chaos_monkey_loop();
        // never returns
    }

    // parent watches and respawns workers
    int to_wait = num_workers;
    while (to_wait > 0) {
        int status;
        pid_t dead = wait(&status);
        if (dead <= 0) continue;

        // find index
        int idx = -1;
        for (int i = 0; i < num_workers; ++i) {
            if (pids[i] == dead) {
                idx = i;
                break;
            }
        }

        fprintf(stderr, "[parent] worker PID %d (idx=%d) died, respawning\n", dead, idx);
        pid_t c = fork();
        if (c < 0) {
            perror("fork");
            exit(1);
        }
        if (c == 0) {
            ConnectPORT(port);
            worker_compute(idx, num_workers, block_size);
            exit(0);
        }
        pids[idx] = c;
        to_wait--;
    }

    // stop chaos monkey
    kill(chaos_pid, SIGTERM);

    // collect and print result
    master_collect();


    // … code you want to time …

    // 2) mark end
    clock_gettime(CLOCK_MONOTONIC, &t1);

    // 3) compute elapsed, handling possible nanosecond underflow
    time_t sec  = t1.tv_sec  - t0.tv_sec;
    long   nsec = t1.tv_nsec - t0.tv_nsec;
    if (nsec < 0) {
        sec  -= 1;
        nsec += 1000000000L;
    }

    double elapsed = (double)sec + (double)nsec / 1e9;
    printf("Total elapsed time: %.3f seconds\n", elapsed);

    free(A);
    free(B);
    free(worker_pids);
    return 0;
}
