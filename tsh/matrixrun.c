#include "matrix.h"
// These would normally be in a separate header file (matrix.h)
#define TILE_SIZE 2
#define KEY_BUF 32
#define VAL_BUF 32

// Global variable for matrix size
int N;

// Placeholder functions for tuple space operations.  These would be
// implemented by your TSH library.  For this example, we'll make
// simple placeholder versions.

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
void master_collect(int num_workers) {
    char key[KEY_BUF];
    char val[VAL_BUF];
    int C[N][N];
    memset(C, 0, sizeof C);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            snprintf(key, sizeof key, "C_%d_%d", i, j);
            TshGET(key, val);
            C[i][j] = atoi(val);
        }
    }
    printf("Result C:\n");
    printMatrix(C);
}

// Function for worker processes to compute assigned rows
void worker_compute(int worker_id, int num_workers, int block_size) {
    char key[KEY_BUF];
    char valA[VAL_BUF], valB[VAL_BUF];
    char valC[VAL_BUF];
    int num_blocks = N / block_size;
    int block_row_start, block_col_start;
    int worker_block_index;
    int total_blocks = num_blocks * num_blocks;

    for (worker_block_index = worker_id; worker_block_index < total_blocks; worker_block_index += num_workers) {
        int block_row = worker_block_index / num_blocks;
        int block_col = worker_block_index % num_blocks;

        block_row_start = block_row * block_size;
        block_col_start = block_col * block_size;

        for (int i = block_row_start; i < block_row_start + block_size; ++i) {
            for (int j = block_col_start; j < block_col_start + block_size; ++j) {
                int sum = 0;
                for (int k = 0; k < N; ++k) {
                    snprintf(key, sizeof key, "A_%d_%d", i, k);
                    TshREAD(key, valA);
                    snprintf(key, sizeof key, "B_%d_%d", k, j);
                    TshREAD(key, valB);
                    sum += atoi(valA) * atoi(valB);
                }
                snprintf(valC, sizeof valC, "%d", sum);
                int len = strlen(valC) + 1;
                snprintf(key, sizeof key, "C_%d_%d", i, j);
                TshPUT(key, len, valC);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_workers> <port> <matrix_size>\n", argv[0]);
        return 1;
    }
    int num_workers = atoi(argv[1]);
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

    int A[N][N], B[N][N];
    initializeMatrix('A', A, 1);
    initializeMatrix('B', B, 2);
    printf("Matrix A (size %d):\n", N);
    printMatrix(A);
    printf("Matrix B (size %d):\n", N);
    printMatrix(B);

    int block_size = TILE_SIZE;

    pid_t pids[num_workers];
    for (int w = 0; w < num_workers; ++w) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            worker_compute(w, num_workers, block_size);
            exit(0);
        }
        pids[w] = pid;
    }

    for (int w = 0; w < num_workers; ++w) {
        waitpid(pids[w], NULL, 0);
    }

    master_collect(num_workers);
    return 0;
}

