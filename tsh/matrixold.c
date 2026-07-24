
#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

// Include matrix.h if it declares Tsh* functions or common types
// If not, declare the functions from tshlib.c you use:
// #include "matrix.h" // Or declare functions below

// --- Function Declarations from tshlib (if not in matrix.h) ---
extern int ConnectPORT(int port); // Needs the port number set globally
extern int TshPUT(const char *key, size_t len, const char *value);
extern int TshREAD(const char *key, char *value); // Assume reads into buffer
extern int TshGET(const char *key, char *value);  // Assume gets into buffer
// --- End Declarations ---

// Constants
#define TILE_SIZE 2         // Example tile size (used as block_size)
#define KEY_BUF 64          // Buffer size for keys
#define VAL_BUF 64          // Buffer size for values
#define MAX_RETRIES 3       // Maximum number of retries for tuple operations
#define TIMEOUT_SEC 5       // Timeout in seconds for alarm
#define RETRY_DELAY_SEC 2   // Seconds part of the increased retry delay
#define RETRY_DELAY_NSEC 0  // Nanoseconds part of the increased retry delay

// Global variable for matrix size
int N;

// Global flag for signal handling (safer than longjmp from handler)
volatile sig_atomic_t timeout_occurred = 0;

// Signal handler for SIGALRM
void alarm_handler(int sig) {
    (void)sig; // Suppress unused variable warning
    timeout_occurred = 1;
    // Avoid unsafe functions like printf here.
}

// Function to print a matrix
void printMatrix(int matrix[N][N]) {
    printf("Matrix (size %dx%d):\n", N, N);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%4d ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// Function to initialize a matrix and publish it to tuple space
// Exits immediately on non-timeout errors. Uses increased delay for timeout retries.
void initializeMatrix(char name, int matrix[N][N], int initValue) {
    char key[KEY_BUF];
    char val[VAL_BUF];
    printf("Initializing matrix %c...\n", name);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            matrix[i][j] = initValue + i + j; // Simple initialization
            snprintf(val, sizeof val, "%d", matrix[i][j]);
            snprintf(key, sizeof key, "%c_%d_%d", name, i, j);

            int retries = 0;
            int result = -1;
            while (retries <= MAX_RETRIES) {
                timeout_occurred = 0;
                alarm(TIMEOUT_SEC);
                // TshPUT now establishes its own connection internally via tshlib
                result = TshPUT(key, strlen(val) + 1, val);
                alarm(0);

                if (result > 0 || result >= 0) break; // Success (Check exact success code if known, >=0 is safer)

                // Failure occurred
                if (timeout_occurred) {
                     if (retries < MAX_RETRIES) {
                         printf("Timeout during init TshPUT for %s (retry %d/%d). Waiting %d sec...\n",
                                key, retries + 1, MAX_RETRIES + 1, RETRY_DELAY_SEC);
                         struct timespec delay = {RETRY_DELAY_SEC, RETRY_DELAY_NSEC};
                         nanosleep(&delay, NULL);
                     }
                } else {
                     // Treat non-timeout error during critical init as fatal immediately
                     fprintf(stderr, "Error during init TshPUT for %s (not timeout, result=%d). Aborting.\n", key, result);
                     exit(EXIT_FAILURE); // Exit immediately
                }
                 retries++;
            } // end retry while

            if (result < 0) {
                 fprintf(stderr, "Error: Failed to TshPUT initial value for %s after %d retries. Aborting.\n", key, MAX_RETRIES + 1);
                 exit(EXIT_FAILURE); // Critical init failure
            }
        }
    }
    printf("Matrix %c initialized and published.\n", name);
}


// --- Modified Robust Tuple Space Operations for Worker ---

// Wrapper for TshREAD with timeout and retries. Exits worker on non-timeout errors.
int robust_TshREAD(const char *key, char *value, const char* context_msg) {
    int retries = 0;
    int result = -1;
    pid_t pid = getpid();

    while (retries <= MAX_RETRIES) {
        timeout_occurred = 0;
        alarm(TIMEOUT_SEC);
        result = TshREAD(key, value); // Assume value buffer is large enough
        alarm(0); // Clear alarm quickly

        if (result > 0 || result >= 0) { // Success (Check actual success code if known, >=0 is safer)
            return 0; // Indicate success to caller
        }

        // Failure occurred (result likely 0 or -1 from tshlib)
        if (timeout_occurred) {
            // It timed out - worth retrying with long delay
            printf("Worker %d: Timeout during TshREAD for %s (%s) (attempt %d/%d). Retrying after delay...\n",
                   pid, key, context_msg, retries + 1, MAX_RETRIES + 1);
            if (retries < MAX_RETRIES) {
                struct timespec delay = {RETRY_DELAY_SEC, RETRY_DELAY_NSEC}; // Use increased delay
                nanosleep(&delay, NULL);
            }
        } else {
            // *** Non-timeout error from TshREAD ***
            // Assume connection issue caused by tshlib model. Indicate failure.
            fprintf(stderr, "Worker %d: Error during TshREAD for %s (%s) (not timeout, result=%d). Assuming connection issue. Worker exiting.\n",
                   pid, key, context_msg, result);
            return -1; // Indicate non-recoverable failure
        }
        retries++;
    }

    // Only reached if all retries (due to timeout) failed
    fprintf(stderr, "Worker %d: Failed TshREAD for %s (%s) after %d timeout retries. Worker exiting.\n",
            pid, key, context_msg, MAX_RETRIES + 1);
    return -1; // Return failure
}

// Wrapper for TshPUT with timeout and retries. Exits worker on non-timeout errors.
int robust_TshPUT(const char *key, size_t len, const char *value, const char* context_msg) {
    int retries = 0;
    int result = -1;
    pid_t pid = getpid();

    while (retries <= MAX_RETRIES) {
        timeout_occurred = 0;
        alarm(TIMEOUT_SEC);
        result = TshPUT(key, len, value);
        alarm(0);

        if (result > 0 || result >= 0) { // Success (Check actual success code if known, >=0 is safer)
            return 0; // Indicate success to caller
        }

        // Failure occurred (result likely 0 or -1 from tshlib)
        if (timeout_occurred) {
            // It timed out - worth retrying with long delay
            printf("Worker %d: Timeout during TshPUT for %s (%s) (attempt %d/%d). Retrying after delay...\n",
                   pid, key, context_msg, retries + 1, MAX_RETRIES + 1);
            if (retries < MAX_RETRIES) {
                 struct timespec delay = {RETRY_DELAY_SEC, RETRY_DELAY_NSEC}; // Use increased delay
                 nanosleep(&delay, NULL);
            }
        } else {
            // *** Non-timeout error from TshPUT ***
             // Assume connection issue caused by tshlib model. Indicate failure.
             fprintf(stderr, "Worker %d: Error during TshPUT for %s (%s) (not timeout, result=%d). Assuming connection issue. Worker exiting.\n",
                   pid, key, context_msg, result);
             return -1; // Indicate non-recoverable failure
        }
        retries++;
    }

     // Only reached if all retries (due to timeout) failed
     fprintf(stderr, "Worker %d: Failed TshPUT for %s (%s) after %d timeout retries. Worker exiting.\n",
            pid, key, context_msg, MAX_RETRIES + 1);
    return -1; // Return failure
}


// Function for worker processes to compute assigned blocks
void worker_compute(int worker_id, int num_workers, int block_size) {
    char key[KEY_BUF];
    char valA_str[VAL_BUF], valB_str[VAL_BUF];
    char valC_str[VAL_BUF];
    int num_blocks = N / block_size;
    if (N % block_size != 0) {
        // Handled by loop bounds `i < N` and `j < N`
    }
    if (num_blocks == 0 && N > 0) num_blocks = 1;

    int block_row_start, block_col_start;
    int worker_block_index;
    int total_blocks = num_blocks * num_blocks;

    // Set up the signal handler for SIGALRM.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sa.sa_flags = SA_RESTART; // Important for nanosleep
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Worker: Failed to set signal handler");
        exit(EXIT_FAILURE);
    }

    printf("Worker %d started. Processing blocks assigned with offset %d and stride %d.\n", getpid(), worker_id, num_workers);

    // Iterate through the blocks assigned to this worker
    for (worker_block_index = worker_id; worker_block_index < total_blocks; worker_block_index += num_workers) {
        int block_row = worker_block_index / num_blocks;
        int block_col = worker_block_index % num_blocks;

        block_row_start = block_row * block_size;
        block_col_start = block_col * block_size;

        printf("Worker %d: Computing block (%d, %d) starting at C[%d][%d]\n",
               getpid(), block_row, block_col, block_row_start, block_col_start);

        // Compute elements within the assigned block
        for (int i = block_row_start; i < block_row_start + block_size && i < N; ++i) {
            for (int j = block_col_start; j < block_col_start + block_size && j < N; ++j) {
                int sum = 0;
                for (int k = 0; k < N; ++k) {
                    int valA_int, valB_int;

                    // --- Read A[i][k] with Modified Robustness ---
                    snprintf(key, sizeof key, "A_%d_%d", i, k);
                    if (robust_TshREAD(key, valA_str, "Read A") < 0) {
                        // robust_TshREAD now prints the error and returns -1 on non-recoverable failure
                        exit(EXIT_FAILURE); // Worker exits
                    }
                    valA_int = atoi(valA_str);

                    // --- Read B[k][j] with Modified Robustness ---
                    snprintf(key, sizeof key, "B_%d_%d", k, j);
                     if (robust_TshREAD(key, valB_str, "Read B") < 0) {
                        exit(EXIT_FAILURE); // Worker exits
                    }
                    valB_int = atoi(valB_str);

                    sum += valA_int * valB_int;
                } // end loop k

                // --- Publish C[i][j] with Modified Robustness ---
                snprintf(valC_str, sizeof valC_str, "%d", sum);
                int len = strlen(valC_str) + 1;
                snprintf(key, sizeof key, "C_%d_%d", i, j);

                 if (robust_TshPUT(key, len, valC_str, "Put C") < 0) {
                     exit(EXIT_FAILURE); // Worker exits
                 }

            } // end loop j
        } // end loop i
         printf("Worker %d: Finished block (%d, %d)\n", getpid(), block_row, block_col);
    } // end loop worker_block_index

    printf("Worker %d finished all assigned blocks successfully.\n", getpid());
    // Worker returns normally here, main calls exit(EXIT_SUCCESS)
}


// --- Robust Tuple Space Operations for Master ---

// Wrapper for TshGET with timeout and retries
// Returns 0 on success, -1 on persistent failure/timeout, -2 on non-timeout error
int robust_TshGET(const char *key, char *value, const char* context_msg) {
    int retries = 0;
    int result = -1;

    while (retries <= MAX_RETRIES) {
        timeout_occurred = 0;
        // Use a slightly longer timeout for GET as it might wait for workers
        alarm(TIMEOUT_SEC + 2); // e.g., TIMEOUT_SEC + 2 seconds
        result = TshGET(key, value); // Assume value buffer is large enough
        alarm(0); // Clear alarm quickly

        if (result > 0 || result >= 0) { // Success (Check actual success code if known, >=0 is safer)
            return 0; // Return success
        }

        // Failure occurred
        if (timeout_occurred) {
             printf("Master: Timeout during TshGET for %s (%s) (attempt %d/%d). Retrying...\n",
                   key, context_msg, retries + 1, MAX_RETRIES + 1);
             if (retries < MAX_RETRIES) {
                 struct timespec delay = {RETRY_DELAY_SEC, RETRY_DELAY_NSEC}; // Use increased delay
                 nanosleep(&delay, NULL); // Wait before retrying
             }
        } else {
            // Non-timeout error during GET (e.g., connection reset within TshGET)
            fprintf(stderr, "Master: Error during TshGET for %s (%s) (not timeout, result=%d).\n",
                   key, context_msg, result);
            return -2; // Indicate non-timeout error
        }
        retries++;
    }

    fprintf(stderr, "Master: Failed TshGET for %s (%s) after %d retries (likely timeout).\n",
            key, context_msg, MAX_RETRIES + 1);
    return -1; // Return persistent timeout/failure
}


// Function for the master process to collect results and print matrix C
// Returns true if all results collected successfully AND workers were ok, false otherwise.
bool master_collect(int num_workers, bool workers_ok) {
    char key[KEY_BUF];
    char val[VAL_BUF];
    int C[N][N]; // Result matrix
    bool processed[N][N]; // Track received elements
    memset(C, 0, sizeof C); // Initialize result matrix
    memset(processed, false, sizeof processed); // Initialize tracking matrix

    int received_count = 0;
    int expected_count = N * N;
    bool collection_success = true; // Track if collection itself encountered errors

    printf("Master started collecting results. Expecting %d elements.\n", expected_count);

    // Set up signal handler for master's own timeouts
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Master: Failed to set signal handler");
        return false; // Cannot proceed without reliable timeouts
    }

    // Loop through trying to get all results
    int attempts = 0;
    // Increased max_attempts heuristic slightly due to longer delays
    int max_attempts = expected_count * (MAX_RETRIES + 2) + num_workers;

    while (received_count < expected_count && attempts < max_attempts) {
        bool got_one_this_pass = false;
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                if (!processed[i][j]) {
                    snprintf(key, sizeof key, "C_%d_%d", i, j);
                    int get_result = robust_TshGET(key, val, "Collect C");

                    if (get_result == 0) { // Success getting tuple
                        if (!processed[i][j]) {
                             C[i][j] = atoi(val);
                             processed[i][j] = true;
                             received_count++;
                             got_one_this_pass = true;
                        }
                    } else if (get_result == -1) { // Persistent timeout/failure on GET
                        fprintf(stderr, "Master: Warning - Persistent failure getting %s. Result may be incomplete.\n", key);
                        collection_success = false;
                        // If workers failed, this might be expected, but still a collection problem.
                    } else { // Non-timeout error on GET (-2)
                        fprintf(stderr, "Master: Unrecoverable error on TshGET for %s. Aborting collection.\n", key);
                        collection_success = false;
                        goto collection_end; // Break out completely
                    }
                } // end if !processed
            } // end loop j
        } // end loop i

        attempts++;
        // If a full pass yielded nothing and we know workers had issues, maybe stop early?
        if (!got_one_this_pass && !workers_ok && received_count < expected_count) {
            printf("Master: No new results found and workers reported issues. Assuming collection is incomplete.\n");
            collection_success = false;
            break; // Stop trying if it seems hopeless
        }
        // Optional small sleep if no progress and still expecting tuples
        if (!got_one_this_pass && received_count < expected_count) {
           struct timespec brief_delay = {0, 100 * 1000000}; // 100ms
           nanosleep(&brief_delay, NULL);
        }

    } // end while received_count < expected_count

collection_end:
    if (received_count == expected_count) {
        printf("Master collected all %d results.\n", expected_count);
        printf("Result C:\n");
        printMatrix(C);
        // Final success requires both collection and workers completing ok.
        if (!collection_success) {
             fprintf(stderr, "Master: Warning - collection encountered errors getting tuples, results might be unreliable.\n");
        }
         if (!workers_ok) {
             fprintf(stderr, "Master: Warning - workers did not finish successfully, results might be incomplete or incorrect.\n");
        }
        return collection_success && workers_ok;
    } else {
        fprintf(stderr, "Master: Failed to collect all results (%d/%d received).\n", received_count, expected_count);
         if (workers_ok) {
            fprintf(stderr, "Master: Workers finished OK, but results missing. Possible Tuple Space issue or logic error.\n");
         } else {
             fprintf(stderr, "Master: Worker failures likely caused missing results.\n");
         }
        return false;
    }
}

// Main function
int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_workers> <port> <matrix_size>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int num_workers = atoi(argv[1]);
    int port = atoi(argv[2]);
    N = atoi(argv[3]); // Assign to global N

    if (num_workers < 1) {
        fprintf(stderr, "Error: Number of workers must be at least 1.\n");
        return EXIT_FAILURE;
    }
    if (N < 1) {
        fprintf(stderr, "Error: Matrix size must be at least 1.\n");
        return EXIT_FAILURE;
    }
    if (N % TILE_SIZE != 0 && N > TILE_SIZE) {
         printf("Warning: Matrix size %d is not a multiple of TILE_SIZE %d. Edges handled by loop bounds.\n", N, TILE_SIZE);
    }

    // Allocate matrices on the stack (careful with large N)
    if (N > 1000) {
         fprintf(stderr, "Warning: Matrix size %d is large, stack allocation might fail. Consider dynamic allocation.\n", N);
    }
    int A[N][N], B[N][N]; // Use Variable Length Arrays (C99 feature)

    // Set the port number for the tshlib (required by the provided tshlib.c)
    // We don't need to check the return of ConnectPORT itself here, as
    // the actual connection happens in TshPUT/GET/READ now.
    ConnectPORT(port);
    printf("Using Tuple Space on port %d (connection established per operation by tshlib)...\n", port);

    // Initialize matrices and publish them to tuple space
    initializeMatrix('A', A, 1); // Exits on failure
    initializeMatrix('B', B, 2); // Exits on failure

    // Optionally print local matrices for verification
    printf("Local Matrix A:\n"); printMatrix(A);
    printf("Local Matrix B:\n"); printMatrix(B);

    int block_size = TILE_SIZE;

    printf("Forking %d worker processes...\n", num_workers);
    pid_t pids[num_workers];
    memset(pids, 0, sizeof(pids));
    int workers_started = 0;
    for (int w = 0; w < num_workers; ++w) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
             // Terminate already started children before exiting
             fprintf(stderr, "Terminating %d already started workers due to fork failure.\n", workers_started);
             for (int k = 0; k < workers_started; ++k) {
                 if (pids[k] > 0) kill(pids[k], SIGTERM); // Send SIGTERM first
             }
             sleep(1); // Give them a moment to exit
             for (int k = 0; k < workers_started; ++k) {
                  if (pids[k] > 0) { // Check if still exists
                     // Check status non-blockingly before sending SIGKILL
                     if (waitpid(pids[k], NULL, WNOHANG) == 0) {
                         kill(pids[k], SIGKILL); // Force kill if still running
                     }
                  }
             }
            exit(EXIT_FAILURE);
        }
        if (pid == 0) { // Child process (Worker)
            // Worker process sets its own port number if needed (handled by global portNum)
            // ConnectPORT(port); // Not needed if using global or parent set it

            worker_compute(w, num_workers, block_size);

            // If worker_compute returns without exiting, it means success for this worker
            exit(EXIT_SUCCESS);
        }
        // Parent process
        pids[w] = pid;
        workers_started++;
        printf("Worker %d created with PID %d\n", w, pid);
    }

    // Master waits for all workers to complete (or exit)
    printf("Master waiting for workers to finish...\n");
    int status;
    bool all_workers_ok = true;
    int failed_worker_count = 0;
    for (int w = 0; w < num_workers; ++w) {
        pid_t finished_pid = waitpid(pids[w], &status, 0);
        if (finished_pid < 0) {
             perror("waitpid error");
             all_workers_ok = false;
             failed_worker_count++; // Count this as a failure
             continue;
        }

         if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            printf("Worker with PID %d finished with exit status %d.\n", finished_pid, exit_status);
            if (exit_status != EXIT_SUCCESS) {
                all_workers_ok = false;
                failed_worker_count++;
            }
        } else if (WIFSIGNALED(status)) {
            int term_sig = WTERMSIG(status);
            printf("Worker with PID %d terminated by signal %d (%s).\n", finished_pid, term_sig, strsignal(term_sig));
            all_workers_ok = false;
            failed_worker_count++;
        } else {
            printf("Worker with PID %d finished with unknown status (status=%d).\n", finished_pid, status);
            all_workers_ok = false;
            failed_worker_count++;
        }
    }

    if (failed_worker_count > 0) {
        printf("Master: %d worker(s) did not finish successfully.\n", failed_worker_count);
    }
    printf("All workers have completed processing (status collected).\n");

    // Master collects the results from tuple space
    bool collection_successful = master_collect(num_workers, all_workers_ok);

    // Final verdict
    if (collection_successful) {
         printf("Program finished successfully.\n");
         return EXIT_SUCCESS;
    } else {
         fprintf(stderr, "Program finished with errors or incomplete results.\n");
         return EXIT_FAILURE;
    }
}





#include "matrix.h"
    // For nanosleep (optional delay)

// --- Assumed from matrix.h or tsh library ---
extern int ConnectPORT(int port);
extern int TshPUT(const char *key, size_t len, const char *value);
extern int TshREAD(const char *key, char *value); // Assume reads into buffer
extern int TshGET(const char *key, char *value);  // Assume gets into buffer
// --- End Assumed ---

// Constants
#define TILE_SIZE 2         // Example tile size (used as block_size)
#define KEY_BUF 64          // Buffer size for keys
#define VAL_BUF 64          // Buffer size for values
#define MAX_RETRIES 3       // Maximum number of retries for tuple operations
#define TIMEOUT_SEC 5       // Timeout in seconds for alarm (increased slightly)
#define RETRY_DELAY_MS 500  // Delay between retries in milliseconds

// Global variable for matrix size
int N;

// Global flag for signal handling (safer than longjmp from handler)
volatile sig_atomic_t timeout_occurred = 0;

// Signal handler for SIGALRM
void alarm_handler(int sig) {
    (void)sig; // Suppress unused variable warning
    timeout_occurred = 1;
    // Avoid unsafe functions like printf here.
}

// Function to print a matrix
void printMatrix(int matrix[N][N]) {
    printf("Matrix (size %dx%d):\n", N, N);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%4d ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// Function to initialize a matrix and publish it to tuple space
// (Added basic retry logic here too, though init is often critical)
void initializeMatrix(char name, int matrix[N][N], int initValue) {
    char key[KEY_BUF];
    char val[VAL_BUF];
    printf("Initializing matrix %c...\n", name);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            matrix[i][j] = initValue + i + j; // Simple initialization
            snprintf(val, sizeof val, "%d", matrix[i][j]);
            snprintf(key, sizeof key, "%c_%d_%d", name, i, j);

            int retries = 0;
            int result = -1;
            while (retries <= MAX_RETRIES) {
                timeout_occurred = 0;
                alarm(TIMEOUT_SEC);
                result = TshPUT(key, strlen(val) + 1, val);
                alarm(0);

                if (result >= 0) break; // Success

                if (timeout_occurred) {
                     // printf("Timeout during init TshPUT for %s (retry %d)\n", key, retries); // Debug only
                     if (retries < MAX_RETRIES) {
                         struct timespec delay = {0, RETRY_DELAY_MS * 1000000}; // ms to ns
                         nanosleep(&delay, NULL);
                     }
                } else {
                     fprintf(stderr, "Error during init TshPUT for %s (not timeout)\n", key);
                     // Decide if non-timeout errors should be retried
                     break; // Don't retry non-timeout errors for init?
                }
                 retries++;
            } // end retry while

            if (result < 0) {
                 fprintf(stderr, "Error: Failed to TshPUT initial value for %s after %d retries. Aborting.\n", key, MAX_RETRIES);
                 // Maybe try to clean up already PUT tuples? Complex.
                 exit(EXIT_FAILURE); // Critical init failure
            }
        }
    }
    printf("Matrix %c initialized and published.\n", name);
}


// --- Robust Tuple Space Operations for Worker ---

// Wrapper for TshREAD with timeout and retries
int robust_TshREAD(const char *key, char *value, const char* context_msg) {
    int retries = 0;
    int result = -1;
    pid_t pid = getpid(); // Get PID once

    while (retries <= MAX_RETRIES) {
        timeout_occurred = 0;
        alarm(TIMEOUT_SEC);
        result = TshREAD(key, value); // Assume value buffer is large enough
        alarm(0); // Clear alarm quickly

        if (result >= 0) { // Success
            return 0; // Return success
        }

        // Failure occurred
        if (timeout_occurred) {
            printf("Worker %d: Timeout during TshREAD for %s (%s) (attempt %d/%d). Retrying...\n",
                   pid, key, context_msg, retries + 1, MAX_RETRIES + 1);
            if (retries < MAX_RETRIES) {
                struct timespec delay = {0, RETRY_DELAY_MS * 1000000};
                nanosleep(&delay, NULL); // Wait before retrying
            }
        } else {
            fprintf(stderr, "Worker %d: Error during TshREAD for %s (%s) (not timeout). Retrying...\n",
                   pid, key, context_msg);
             // Optionally add delay here too, or handle specific errors differently
             if (retries < MAX_RETRIES) {
                struct timespec delay = {0, RETRY_DELAY_MS * 1000000};
                nanosleep(&delay, NULL);
             }
        }
        retries++;
    }

    fprintf(stderr, "Worker %d: Failed TshREAD for %s (%s) after %d retries. Worker exiting.\n",
            pid, key, context_msg, MAX_RETRIES + 1);
    return -1; // Return failure
}

// Wrapper for TshPUT with timeout and retries
int robust_TshPUT(const char *key, size_t len, const char *value, const char* context_msg) {
    int retries = 0;
    int result = -1;
    pid_t pid = getpid();

    while (retries <= MAX_RETRIES) {
        timeout_occurred = 0;
        alarm(TIMEOUT_SEC);
        result = TshPUT(key, len, value);
        alarm(0);

        if (result >= 0) { // Success
            return 0; // Return success
        }

        // Failure occurred
        if (timeout_occurred) {
            printf("Worker %d: Timeout during TshPUT for %s (%s) (attempt %d/%d). Retrying...\n",
                   pid, key, context_msg, retries + 1, MAX_RETRIES + 1);
            if (retries < MAX_RETRIES) {
                 struct timespec delay = {0, RETRY_DELAY_MS * 1000000};
                 nanosleep(&delay, NULL);
            }
        } else {
             fprintf(stderr, "Worker %d: Error during TshPUT for %s (%s) (not timeout). Retrying...\n",
                   pid, key, context_msg);
            if (retries < MAX_RETRIES) {
                 struct timespec delay = {0, RETRY_DELAY_MS * 1000000};
                 nanosleep(&delay, NULL);
            }
        }
        retries++;
    }

     fprintf(stderr, "Worker %d: Failed TshPUT for %s (%s) after %d retries. Worker exiting.\n",
            pid, key, context_msg, MAX_RETRIES + 1);
    return -1; // Return failure
}


// Function for worker processes to compute assigned blocks
void worker_compute(int worker_id, int num_workers, int block_size) {
    char key[KEY_BUF];
    char valA_str[VAL_BUF], valB_str[VAL_BUF];
    char valC_str[VAL_BUF];
    int num_blocks = N / block_size;
    // Ensure num_blocks is at least 1, handle non-perfect division if needed
    if (N % block_size != 0) {
         // This is handled by loop bounds `i < N` and `j < N` below
    }
    if (num_blocks == 0 && N > 0) num_blocks = 1; // Handle N < block_size case

    int block_row_start, block_col_start;
    int worker_block_index;
    int total_blocks = num_blocks * num_blocks; // Number of C blocks to compute

    // Set up the signal handler for SIGALRM. Check for errors.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    // Important: Use SA_RESTART to avoid interrupting syscalls like nanosleep unnecessarily
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Worker: Failed to set signal handler");
        exit(EXIT_FAILURE);
    }

    printf("Worker %d started. Processing blocks assigned with offset %d and stride %d.\n", getpid(), worker_id, num_workers);

    // Iterate through the blocks assigned to this worker
    for (worker_block_index = worker_id; worker_block_index < total_blocks; worker_block_index += num_workers) {
        int block_row = worker_block_index / num_blocks;
        int block_col = worker_block_index % num_blocks;

        block_row_start = block_row * block_size;
        block_col_start = block_col * block_size;

        printf("Worker %d: Computing block (%d, %d) starting at C[%d][%d]\n",
               getpid(), block_row, block_col, block_row_start, block_col_start);

        // Compute elements within the assigned block
        for (int i = block_row_start; i < block_row_start + block_size && i < N; ++i) {
            for (int j = block_col_start; j < block_col_start + block_size && j < N; ++j) {
                int sum = 0;
                for (int k = 0; k < N; ++k) {
                    int valA_int, valB_int;

                    // --- Read A[i][k] with Timeout/Retry ---
                    snprintf(key, sizeof key, "A_%d_%d", i, k);
                    if (robust_TshREAD(key, valA_str, "Read A") < 0) {
                        exit(EXIT_FAILURE); // Worker exits on persistent failure
                    }
                    valA_int = atoi(valA_str);

                    // --- Read B[k][j] with Timeout/Retry ---
                    snprintf(key, sizeof key, "B_%d_%d", k, j);
                     if (robust_TshREAD(key, valB_str, "Read B") < 0) {
                        exit(EXIT_FAILURE); // Worker exits on persistent failure
                    }
                    valB_int = atoi(valB_str);

                    sum += valA_int * valB_int;
                } // end loop k

                // --- Publish C[i][j] with Timeout/Retry ---
                snprintf(valC_str, sizeof valC_str, "%d", sum);
                int len = strlen(valC_str) + 1;
                snprintf(key, sizeof key, "C_%d_%d", i, j);

                 if (robust_TshPUT(key, len, valC_str, "Put C") < 0) {
                     exit(EXIT_FAILURE); // Worker exits on persistent failure
                 }

            } // end loop j
        } // end loop i
         printf("Worker %d: Finished block (%d, %d)\n", getpid(), block_row, block_col);
    } // end loop worker_block_index

    printf("Worker %d finished all assigned blocks successfully.\n", getpid());
}


// --- Robust Tuple Space Operations for Master ---

// Wrapper for TshGET with timeout and retries
// Returns 0 on success, -1 on persistent failure/timeout, -2 on non-timeout error
int robust_TshGET(const char *key, char *value, const char* context_msg) {
    int retries = 0;
    int result = -1;

    while (retries <= MAX_RETRIES) {
        timeout_occurred = 0;
        // Use a slightly longer timeout for GET as it might wait for workers
        alarm(TIMEOUT_SEC + 2); // e.g., TIMEOUT_SEC + 2 seconds
        result = TshGET(key, value); // Assume value buffer is large enough
        alarm(0); // Clear alarm quickly

        if (result >= 0) { // Success
            return 0; // Return success
        }

        // Failure occurred
        if (timeout_occurred) {
             printf("Master: Timeout during TshGET for %s (%s) (attempt %d/%d). Retrying...\n",
                   key, context_msg, retries + 1, MAX_RETRIES + 1);
             if (retries < MAX_RETRIES) {
                 struct timespec delay = {0, RETRY_DELAY_MS * 1000000};
                 nanosleep(&delay, NULL); // Wait before retrying
             }
        } else {
             // TshGET might return -1 if tuple not found (depending on implementation)
             // Or a different error for connection issues. Needs clarification from tsh library.
             // Assuming any error other than timeout means we should probably stop retrying here.
            fprintf(stderr, "Master: Error during TshGET for %s (%s) (not timeout, result=%d).\n",
                   key, context_msg, result);
             return -2; // Indicate non-timeout error
        }
        retries++;
    }

    fprintf(stderr, "Master: Failed TshGET for %s (%s) after %d retries (likely timeout).\n",
            key, context_msg, MAX_RETRIES + 1);
    return -1; // Return persistent timeout/failure
}


// Function for the master process to collect results and print matrix C
// Returns true if all results collected successfully, false otherwise.
bool master_collect(int num_workers, bool workers_ok) {
    char key[KEY_BUF];
    char val[VAL_BUF];
    int C[N][N]; // Result matrix
    bool processed[N][N]; // Track received elements
    memset(C, 0, sizeof C); // Initialize result matrix
    memset(processed, false, sizeof processed); // Initialize tracking matrix

    int received_count = 0;
    int expected_count = N * N;
    bool collection_success = true; // Track if collection encountered errors

    printf("Master started collecting results. Expecting %d elements.\n", expected_count);

    // Set up signal handler for master's own timeouts
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Master: Failed to set signal handler");
        return false; // Cannot proceed without reliable timeouts
    }

    // Loop through trying to get all results
    // This loop structure is basic; could be improved with non-blocking gets if available
    int attempts = 0;
    int max_attempts = expected_count * (MAX_RETRIES + 1) + num_workers; // Heuristic limit

    while (received_count < expected_count && attempts < max_attempts) {
        bool got_one_this_pass = false;
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                if (!processed[i][j]) {
                    snprintf(key, sizeof key, "C_%d_%d", i, j);
                    int get_result = robust_TshGET(key, val, "Collect C");

                    if (get_result == 0) { // Success
                        if (!processed[i][j]) { // Avoid race condition if processed flags were updated elsewhere
                             C[i][j] = atoi(val);
                             processed[i][j] = true; // Mark as processed
                             received_count++;
                             got_one_this_pass = true;
                             // printf("Master: Received C[%d][%d] = %d (%d/%d)\n", i, j, C[i][j], received_count, expected_count);
                        }
                    } else if (get_result == -1) { // Persistent timeout/failure on GET
                        fprintf(stderr, "Master: Warning - Persistent failure getting %s. Result may be incomplete.\n", key);
                        collection_success = false;
                        // If we know workers failed, this is expected. If not, it's a problem.
                        if (!workers_ok) {
                           // Don't necessarily stop, maybe other workers produced it?
                           // But mark the overall result as potentially bad.
                        } else {
                           // All workers supposedly finished OK, but we can't get a result? Tuple space issue?
                        }
                        // We might want to break the outer loop here if a result is truly unobtainable
                        // attempts = max_attempts; // Force exit after this pass?
                    } else { // Non-timeout error on GET (-2)
                        fprintf(stderr, "Master: Unrecoverable error on TshGET for %s. Aborting collection.\n", key);
                        collection_success = false;
                        goto collection_end; // Break out completely
                    }
                } // end if !processed
            } // end loop j
        } // end loop i

        attempts++;
        // If a full pass yielded nothing and workers have issues, maybe stop early?
        if (!got_one_this_pass && !workers_ok && received_count < expected_count) {
            printf("Master: No new results found and workers reported issues. Assuming collection is incomplete.\n");
            collection_success = false;
           // break; // Optional: stop trying if it seems hopeless
        }
         // Optional small sleep to prevent overly aggressive busy-waiting if TS is slow
         // if (!got_one_this_pass) {
         //    struct timespec delay = {0, 100 * 1000000}; // 100ms
         //    nanosleep(&delay, NULL);
         // }

    } // end while received_count < expected_count

collection_end:
    if (received_count == expected_count) {
        printf("Master collected all %d results.\n", expected_count);
        printf("Result C:\n");
        printMatrix(C);
        // Even if all results received, if workers failed or collection had errors, it's not truly success
        return collection_success && workers_ok;
    } else {
        fprintf(stderr, "Master: Failed to collect all results (%d/%d received).\n", received_count, expected_count);
         if (workers_ok) {
            fprintf(stderr, "Master: Workers finished OK, but results missing. Possible Tuple Space issue or logic error.\n");
         } else {
             fprintf(stderr, "Master: Worker failures likely caused missing results.\n");
         }
        return false;
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_workers> <port> <matrix_size>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int num_workers = atoi(argv[1]);
    int port = atoi(argv[2]);
    N = atoi(argv[3]); // Assign to global N

    if (num_workers < 1) {
        fprintf(stderr, "Error: Number of workers must be at least 1.\n");
        return EXIT_FAILURE;
    }
    if (N < 1) {
        fprintf(stderr, "Error: Matrix size must be at least 1.\n");
        return EXIT_FAILURE;
    }
    if (N % TILE_SIZE != 0 && N > TILE_SIZE) {
         printf("Warning: Matrix size %d is not a multiple of TILE_SIZE %d. Edges handled by loop bounds.\n", N, TILE_SIZE);
    }

    // Allocate matrices on the stack (careful with large N)
    if (N > 1000) { // Example threshold
         fprintf(stderr, "Warning: Matrix size %d is large, stack allocation might fail. Consider dynamic allocation.\n", N);
    }
    int A[N][N], B[N][N]; // Use Variable Length Arrays (C99 feature)

    printf("Connecting to Tuple Space on port %d...\n", port);
    if (ConnectPORT(port) < 0) { // Assuming ConnectPORT returns < 0 on error
         fprintf(stderr, "Error: Failed to connect to Tuple Space on port %d.\n", port);
         return EXIT_FAILURE;
    }


    // Initialize matrices and publish them to tuple space
    initializeMatrix('A', A, 1); // Exits on failure
    initializeMatrix('B', B, 2); // Exits on failure

    // Optionally print local matrices for verification (can be large)
    printf("Local Matrix A:\n"); printMatrix(A);
    printf("Local Matrix B:\n"); printMatrix(B);

    int block_size = TILE_SIZE;

    printf("Forking %d worker processes...\n", num_workers);
    pid_t pids[num_workers];
    memset(pids, 0, sizeof(pids)); // Initialize PIDs array
    int workers_started = 0;
    for (int w = 0; w < num_workers; ++w) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
             // Terminate already started children before exiting
             fprintf(stderr, "Terminating %d already started workers due to fork failure.\n", workers_started);
             for (int k = 0; k < workers_started; ++k) {
                 if (pids[k] > 0) kill(pids[k], SIGTERM); // Send SIGTERM first
                 // Could add SIGKILL after a delay if needed
             }
             // Wait briefly for them to exit?
             sleep(1);
             for (int k = 0; k < workers_started; ++k) {
                  if (pids[k] > 0) kill(pids[k], SIGKILL); // Force kill
             }
            exit(EXIT_FAILURE);
        }
        if (pid == 0) { // Child process (Worker)
            // Child connects separately IF REQUIRED by the library design
            // if (ConnectPORT(port) < 0) { exit(EXIT_FAILURE); }

            worker_compute(w, num_workers, block_size);

            // If worker_compute returns, it means success
            exit(EXIT_SUCCESS); // Worker exits cleanly
        }
        // Parent process
        pids[w] = pid;
        workers_started++;
        printf("Worker %d created with PID %d\n", w, pid);
    }

    // Master waits for all workers to complete (or exit)
    printf("Master waiting for workers to finish...\n");
    int status;
    bool all_workers_ok = true;
    int failed_worker_count = 0;
    for (int w = 0; w < num_workers; ++w) {
        pid_t finished_pid = waitpid(pids[w], &status, 0);
        if (finished_pid < 0) {
             perror("waitpid error");
             all_workers_ok = false; // Can't determine status, assume failure
             continue;
        }

         if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            printf("Worker with PID %d finished with exit status %d.\n", finished_pid, exit_status);
            if (exit_status != EXIT_SUCCESS) {
                all_workers_ok = false;
                failed_worker_count++;
            }
        } else if (WIFSIGNALED(status)) {
            int term_sig = WTERMSIG(status);
            printf("Worker with PID %d terminated by signal %d (%s).\n", finished_pid, term_sig, strsignal(term_sig));
            all_workers_ok = false; // Terminated by signal is not success
            failed_worker_count++;
        } else {
            printf("Worker with PID %d finished with unknown status (status=%d).\n", finished_pid, status);
            all_workers_ok = false; // Unknown status is not success
            failed_worker_count++;
        }
    }

    if (failed_worker_count > 0) {
        printf("Master: %d worker(s) did not finish successfully.\n", failed_worker_count);
    }
    printf("All workers have completed processing (status collected).\n");

    // Master collects the results from tuple space
    bool collection_successful = master_collect(num_workers, all_workers_ok);

    // Final verdict
    if (collection_successful) {
         printf("Program finished successfully.\n");
         return EXIT_SUCCESS;
    } else {
         fprintf(stderr, "Program finished with errors or incomplete results.\n");
         return EXIT_FAILURE;
    }
}













// // // For bool type
// // #include "matrix.h"
// // // --- Assumed Tuple Space API ---
// // // These functions would be defined in a separate library (e.g., tsh.c)
// // // and declared in a header (e.g., tsh.h)


// // // --- End Assumed API ---

// // // Constants
// // #define TILE_SIZE 2     // Example tile size (used as block_size)
// // #define KEY_BUF 64      // Increased buffer size slightly for safety
// // #define VAL_BUF 64      // Increased buffer size slightly for safety
// // #define MAX_RETRIES 3   // Maximum number of retries (though current logic does 1 retry)
// // #define TIMEOUT_SEC 2   // Timeout in seconds for alarm

// // // Global variable for matrix size
// // int N;

// // // Declare jmp_buf globally for accessibility by signal handler
// // jmp_buf worker_env;

// // // Function to print a matrix
// // void printMatrix(int matrix[N][N]) {
// //     printf("Matrix (size %dx%d):\n", N, N);
// //     for (int i = 0; i < N; i++) {
// //         for (int j = 0; j < N; j++) {
// //             printf("%4d ", matrix[i][j]);
// //         }
// //         printf("\n");
// //     }
// //     printf("\n");
// // }

// // // Function to initialize a matrix and publish it to tuple space
// // void initializeMatrix(char name, int matrix[N][N], int initValue) {
// //     char key[KEY_BUF];
// //     char val[VAL_BUF];
// //     printf("Initializing matrix %c...\n", name);
// //     for (int i = 0; i < N; ++i) {
// //         for (int j = 0; j < N; ++j) {
// //             matrix[i][j] = initValue + i + j; // Simple initialization
// //             snprintf(val, sizeof val, "%d", matrix[i][j]);
// //             snprintf(key, sizeof key, "%c_%d_%d", name, i, j);
// //             // Initial PUT doesn't need timeout handling in this example setup
// //             if (TshPUT(key, strlen(val) + 1, val) < 0) {
// //                  fprintf(stderr, "Error: Failed to TshPUT initial value for %s\n", key);
// //                  // Decide how to handle critical init failure (e.g., exit)
// //                  exit(EXIT_FAILURE);
// //             }
// //         }
// //     }
// //     printf("Matrix %c initialized and published.\n", name);
// // }

// // // Signal handler for SIGALRM in worker processes
// // void alarm_handler(int sig) {
// //     (void)sig; // Suppress unused variable warning
// //     // NOTE: printf in signal handlers is generally unsafe, but used here for demo
// //     // In production, use safer methods like setting a volatile flag.
// //     printf("Worker %d: Timeout occurred during tuple operation. Retrying...\n", getpid());
// //     longjmp(worker_env, 1); // Jump back to the setjmp point, return value 1
// // }

// // // Function for worker processes to compute assigned blocks
// // void worker_compute(int worker_id, int num_workers, int block_size) {
// //     char key[KEY_BUF];
// //     char valA[VAL_BUF], valB[VAL_BUF];
// //     char valC_str[VAL_BUF];
// //     int num_blocks = N / block_size;
// //     // Ensure num_blocks is at least 1, handle non-perfect division if needed
// //     if (N % block_size != 0) {
// //          fprintf(stderr, "Warning: Matrix size %d is not perfectly divisible by block_size %d.\n", N, block_size);
// //          // Adjust or exit based on requirements. Here we proceed but might miss edges.
// //     }
// //     if (num_blocks == 0) num_blocks = 1; // Handle N < block_size case

// //     int block_row_start, block_col_start;
// //     int worker_block_index;
// //     int total_blocks = num_blocks * num_blocks; // Number of C blocks to compute

// //     // Set up the signal handler for SIGALRM. Check for errors.
// //     if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
// //         perror("Worker: Failed to set signal handler");
// //         exit(EXIT_FAILURE);
// //     }

// //     printf("Worker %d started. Processing blocks assigned with offset %d and stride %d.\n", getpid(), worker_id, num_workers);

// //     // Iterate through the blocks assigned to this worker
// //     for (worker_block_index = worker_id; worker_block_index < total_blocks; worker_block_index += num_workers) {
// //         int block_row = worker_block_index / num_blocks;
// //         int block_col = worker_block_index % num_blocks;

// //         block_row_start = block_row * block_size;
// //         block_col_start = block_col * block_size;

// //         printf("Worker %d: Computing block (%d, %d) starting at C[%d][%d]\n",
// //                getpid(), block_row, block_col, block_row_start, block_col_start);

// //         // Compute elements within the assigned block
// //         for (int i = block_row_start; i < block_row_start + block_size && i < N; ++i) {
// //             for (int j = block_col_start; j < block_col_start + block_size && j < N; ++j) {
// //                 int sum = 0;
// //                 for (int k = 0; k < N; ++k) {
// //                     int valA_int, valB_int;

// //                     // --- Read A[i][k] with Timeout ---
// //                     snprintf(key, sizeof key, "A_%d_%d", i, k);
// //                     if (setjmp(worker_env) == 0) { // Normal execution path
// //                         alarm(TIMEOUT_SEC);        // Set alarm
// //                         if (TshREAD(key, valA) < 0) {
// //                              alarm(0); // Clear alarm on error too
// //                              fprintf(stderr, "Worker %d: Failed to read %s\n", getpid(), key);
// //                              // Handle read failure (e.g., skip, retry differently, exit)
// //                              // For now, let's assume value is 0 on error
// //                              valA_int = 0;
// //                              // Consider exiting if reads consistently fail
// //                         } else {
// //                            alarm(0);                  // Clear alarm on success
// //                            valA_int = atoi(valA);
// //                         }
// //                     } else { // Arrived here via longjmp (timeout)
// //                         // alarm(0); // Alarm is already cancelled by signal delivery or longjmp
// //                         printf("Worker %d: Retrying read for %s\n", getpid(), key);
// //                         if (TshREAD(key, valA) < 0) { // Re-issue the read
// //                              fprintf(stderr, "Worker %d: Failed to read %s even after retry\n", getpid(), key);
// //                              valA_int = 0; // Assume 0 on retry failure
// //                         } else {
// //                            printf("Worker %d: Retry read for %s successful\n", getpid(), key);
// //                            valA_int = atoi(valA);
// //                         }
// //                     }

// //                     // --- Read B[k][j] with Timeout ---
// //                     snprintf(key, sizeof key, "B_%d_%d", k, j);
// //                      if (setjmp(worker_env) == 0) { // Normal execution path
// //                         alarm(TIMEOUT_SEC);        // Set alarm
// //                         if (TshREAD(key, valB) < 0) {
// //                              alarm(0);
// //                              fprintf(stderr, "Worker %d: Failed to read %s\n", getpid(), key);
// //                              valB_int = 0;
// //                         } else {
// //                            alarm(0);                  // Clear alarm on success
// //                            valB_int = atoi(valB);
// //                         }
// //                     } else { // Arrived here via longjmp (timeout)
// //                         // alarm(0);
// //                         printf("Worker %d: Retrying read for %s\n", getpid(), key);
// //                         if (TshREAD(key, valB) < 0) { // Re-issue the read
// //                              fprintf(stderr, "Worker %d: Failed to read %s even after retry\n", getpid(), key);
// //                              valB_int = 0;
// //                         } else {
// //                              printf("Worker %d: Retry read for %s successful\n", getpid(), key);
// //                              valB_int = atoi(valB);
// //                         }
// //                     }

// //                     sum += valA_int * valB_int;
// //                 } // end loop k

// //                 // --- Publish C[i][j] with Timeout ---
// //                 snprintf(valC_str, sizeof valC_str, "%d", sum);
// //                 int len = strlen(valC_str) + 1;
// //                 snprintf(key, sizeof key, "C_%d_%d", i, j);

// //                  if (setjmp(worker_env) == 0) { // Normal execution path
// //                     alarm(TIMEOUT_SEC);         // Set alarm
// //                     if (TshPUT(key, len, valC_str) < 0) {
// //                         alarm(0);
// //                         fprintf(stderr, "Worker %d: Failed to put %s\n", getpid(), key);
// //                         // Handle PUT failure (less critical than READ maybe, but result is lost)
// //                     } else {
// //                        alarm(0);                  // Clear alarm on success
// //                     }
// //                 } else { // Arrived here via longjmp (timeout)
// //                     // alarm(0);
// //                     printf("Worker %d: Retrying put for %s\n", getpid(), key);
// //                     if (TshPUT(key, len, valC_str) < 0) { // Re-issue the put
// //                          fprintf(stderr, "Worker %d: Failed to put %s even after retry\n", getpid(), key);
// //                     } else {
// //                         printf("Worker %d: Retry put for %s successful\n", getpid(), key);
// //                     }
// //                 }
// //             } // end loop j
// //         } // end loop i
// //          printf("Worker %d: Finished block (%d, %d)\n", getpid(), block_row, block_col);
// //     } // end loop worker_block_index

// //     printf("Worker %d finished all assigned blocks.\n", getpid());
// // }


// // // Function for the master process to collect results and print matrix C
// // void master_collect(int num_workers) {
// //     char key[KEY_BUF];
// //     char val[VAL_BUF];
// //     int C[N][N]; // Result matrix
// //     bool processed[N][N]; // Track received elements to handle redundancy
// //     memset(C, 0, sizeof C); // Initialize result matrix
// //     memset(processed, false, sizeof processed); // Initialize tracking matrix

// //     int received_count = 0;
// //     int expected_count = N * N;

// //     printf("Master started collecting results. Expecting %d elements.\n", expected_count);

// //     // Continue until all unique N*N results are collected
// //     while (received_count < expected_count) {
// //         // Iterate through all possible result indices
// //         for (int i = 0; i < N; ++i) {
// //             for (int j = 0; j < N; ++j) {
// //                 // Check if this result C[i][j] has already been processed
// //                 if (!processed[i][j]) {
// //                     snprintf(key, sizeof key, "C_%d_%d", i, j);

// //                     // Attempt to GET the result tuple. This will block if not available.
// //                     // NOTE: If a worker fails permanently before PUTting this tuple,
// //                     // the master could potentially block here indefinitely without
// //                     // a timeout mechanism on TshGET itself.
// //                     if (TshGET(key, val) >= 0) {
// //                         // Successfully retrieved a tuple for C[i][j]

// //                         // Double-check if it was processed by another iteration
// //                         // between the check and the TshGET (unlikely but safe)
// //                         if (!processed[i][j]) {
// //                             C[i][j] = atoi(val);
// //                             processed[i][j] = true; // Mark as processed
// //                             received_count++;
// //                             // Optional: Print progress
// //                             // printf("Master: Received C[%d][%d] = %d (%d/%d)\n", i, j, C[i][j], received_count, expected_count);
// //                         } else {
// //                              // This case should be rare if TshGET is atomic and blocking
// //                              printf("Master: Warning - Received tuple %s which was already marked processed.\n", key);
// //                         }
// //                     } else {
// //                         // TshGET failed for some reason other than not being present (e.g., connection issue)
// //                         fprintf(stderr, "Master: Error on TshGET for %s. Retrying or skipping...\n", key);
// //                          // Depending on the error, might need specific handling.
// //                          // For now, we just loop and will try again later.
// //                     }
// //                 } // end if !processed
// //             } // end loop j
// //         } // end loop i

// //         // Add a small sleep if desired to prevent busy-waiting if looping quickly
// //         // without finding many new tuples (depends on TshGET behavior).
// //         // usleep(10000); // e.g., sleep 10ms
// //     } // end while received_count < expected_count

// //     printf("Master collected all %d results.\n", expected_count);
// //     printf("Result C:\n");
// //     printMatrix(C);
// // }

// // int main(int argc, char **argv) {
// //     if (argc != 4) {
// //         fprintf(stderr, "Usage: %s <num_workers> <port> <matrix_size>\n", argv[0]);
// //         return 1;
// //     }
// //     int num_workers = atoi(argv[1]);
// //     int port = atoi(argv[2]);
// //     N = atoi(argv[3]); // Assign to global N

// //     if (num_workers < 1) {
// //         fprintf(stderr, "Error: Number of workers must be at least 1.\n");
// //         return 1;
// //     }
// //     if (N < 1) {
// //         fprintf(stderr, "Error: Matrix size must be at least 1.\n");
// //         return 1;
// //     }
// //      if (N % TILE_SIZE != 0 && N > TILE_SIZE) {
// //          fprintf(stderr, "Warning: Matrix size %d is not a multiple of TILE_SIZE %d. Computation might not cover the entire matrix depending on block logic.\n", N, TILE_SIZE);
// //          // Adjust worker loop bounds if necessary for non-multiples. The current code handles this with `&& i < N` etc.
// //     }


// //     // Allocate matrices on the stack (careful with large N)
// //     // For very large N, dynamic allocation (malloc) would be needed.
// //     if (N > 1000) { // Example threshold
// //          fprintf(stderr, "Warning: Matrix size %d is large, stack allocation might fail.\n", N);
// //     }
// //     int A[N][N], B[N][N]; // Use Variable Length Arrays (C99 feature)

// //     printf("Connecting to Tuple Space on port %d...\n", port);
// //     ConnectPORT(port); // Connect to the tuple space server

// //     // Initialize matrices and publish them to tuple space
// //     initializeMatrix('A', A, 1);
// //     initializeMatrix('B', B, 2);

// //     // Optionally print local matrices for verification (can be large)
// //     printf("Local Matrix A:\n"); printMatrix(A);
// //     printf("Local Matrix B:\n"); printMatrix(B);

// //     int block_size = TILE_SIZE;

// //     printf("Forking %d worker processes...\n", num_workers);
// //     pid_t pids[num_workers];
// //     for (int w = 0; w < num_workers; ++w) {
// //         pid_t pid = fork();
// //         if (pid < 0) {
// //             perror("fork failed");
// //              // Terminate already started children? Cleanup?
// //             // For simplicity, just exit. In robust code, send SIGTERM to children.
// //              for (int k=0; k<w; ++k) { kill(pids[k], SIGTERM); }
// //             exit(EXIT_FAILURE);
// //         }
// //         if (pid == 0) { // Child process (Worker)
// //             // Each worker connects separately (if required by the TS library)
// //             // ConnectPORT(port); // Uncomment if connection is per-process
// //             worker_compute(w, num_workers, block_size);
// //             exit(EXIT_SUCCESS); // Worker exits cleanly
// //         }
// //         // Parent process
// //         pids[w] = pid;
// //         printf("Worker %d created with PID %d\n", w, pid);
// //     }

// //     // Master waits for all workers to complete (or exit)
// //     printf("Master waiting for workers to finish...\n");
// //     int status;
// //     for (int w = 0; w < num_workers; ++w) {
// //         waitpid(pids[w], &status, 0);
// //          if (WIFEXITED(status)) {
// //             printf("Worker with PID %d finished with exit status %d.\n", pids[w], WEXITSTATUS(status));
// //         } else if (WIFSIGNALED(status)) {
// //             printf("Worker with PID %d terminated by signal %d.\n", pids[w], WTERMSIG(status));
// //         } else {
// //             printf("Worker with PID %d finished with unknown status.\n", pids[w]);
// //         }
// //     }
// //     printf("All workers have completed.\n");

// //     // Master collects the results from tuple space
// //     master_collect(num_workers);

// //     printf("Program finished.\n");
// //     return 0;
// // }


// // // #include "matrix.h"
// // // // These would normally be in a separate header file (matrix.h)
// // // #define TILE_SIZE 2
// // // #define KEY_BUF 32
// // // #define VAL_BUF 32

// // // // Global variable for matrix size
// // // int N;

// // // // Placeholder functions for tuple space operations.  These would be
// // // // implemented by your TSH library.  For this example, we'll make
// // // // simple placeholder versions.


// // // // Function to print a matrix
// // // void printMatrix(int matrix[N][N]) {
// // //     for (int i = 0; i < N; i++) {
// // //         for (int j = 0; j < N; j++) {
// // //             printf("%4d ", matrix[i][j]);
// // //         }
// // //         printf("\n");
// // //     }
// // //     printf("\n");
// // // }

// // // // Function to initialize a matrix and publish it to tuple space
// // // void initializeMatrix(char name, int matrix[N][N], int initValue) {
// // //     char key[KEY_BUF];
// // //     char val[VAL_BUF];
// // //     for (int i = 0; i < N; ++i) {
// // //         for (int j = 0; j < N; ++j) {
// // //             matrix[i][j] = initValue + i + j;
// // //             snprintf(val, sizeof val, "%d", matrix[i][j]);
// // //             snprintf(key, sizeof key, "%c_%d_%d", name, i, j);
// // //             TshPUT(key, strlen(val) + 1, val);
// // //         }
// // //     }
// // // }

// // // // Function for the master process to collect results and print matrix C
// // // void master_collect(int num_workers) {
// // //     char key[KEY_BUF];
// // //     char val[VAL_BUF];
// // //     int C[N][N];
// // //     memset(C, 0, sizeof C);
// // //     for (int i = 0; i < N; ++i) {
// // //         for (int j = 0; j < N; ++j) {
// // //             snprintf(key, sizeof key, "C_%d_%d", i, j);
// // //             TshGET(key, val);
// // //             C[i][j] = atoi(val);
// // //         }
// // //     }
// // //     printf("Result C:\n");
// // //     printMatrix(C);
// // // }

// // // // Function for worker processes to compute assigned rows
// // // void worker_compute(int worker_id, int num_workers, int block_size) {
// // //     char key[KEY_BUF];
// // //     char valA[VAL_BUF], valB[VAL_BUF];
// // //     char valC[VAL_BUF];
// // //     int num_blocks = N / block_size;
// // //     int block_row_start, block_col_start;
// // //     int worker_block_index;
// // //     int total_blocks = num_blocks * num_blocks;

// // //     for (worker_block_index = worker_id; worker_block_index < total_blocks; worker_block_index += num_workers) {
// // //         int block_row = worker_block_index / num_blocks;
// // //         int block_col = worker_block_index % num_blocks;

// // //         block_row_start = block_row * block_size;
// // //         block_col_start = block_col * block_size;

// // //         for (int i = block_row_start; i < block_row_start + block_size; ++i) {
// // //             for (int j = block_col_start; j < block_col_start + block_size; ++j) {
// // //                 int sum = 0;
// // //                 for (int k = 0; k < N; ++k) {
// // //                     snprintf(key, sizeof key, "A_%d_%d", i, k);
// // //                     TshREAD(key, valA);
// // //                     snprintf(key, sizeof key, "B_%d_%d", k, j);
// // //                     TshREAD(key, valB);
// // //                     sum += atoi(valA) * atoi(valB);
// // //                 }
// // //                 snprintf(valC, sizeof valC, "%d", sum);
// // //                 int len = strlen(valC) + 1;
// // //                 snprintf(key, sizeof key, "C_%d_%d", i, j);
// // //                 TshPUT(key, len, valC);
// // //             }
// // //         }
// // //     }
// // // }

// // // int main(int argc, char **argv) {
// // //     if (argc != 4) {
// // //         fprintf(stderr, "Usage: %s <num_workers> <port> <matrix_size>\n", argv[0]);
// // //         return 1;
// // //     }
// // //     int num_workers = atoi(argv[1]);
// // //     int port = atoi(argv[2]);
// // //     N = atoi(argv[3]);

// // //     if (num_workers < 1 || num_workers > N * N) {
// // //         fprintf(stderr, "num_workers must be between 1 and %d\n", N * N);
// // //         return 1;
// // //     }
// // //     if (N < 1) {
// // //         fprintf(stderr, "Matrix size must be at least 1\n");
// // //         return 1;
// // //     }

// // //     ConnectPORT(port);

// // //     int A[N][N], B[N][N];
// // //     initializeMatrix('A', A, 1);
// // //     initializeMatrix('B', B, 2);
// // //     printf("Matrix A (size %d):\n", N);
// // //     printMatrix(A);
// // //     printf("Matrix B (size %d):\n", N);
// // //     printMatrix(B);

// // //     int block_size = TILE_SIZE;

// // //     pid_t pids[num_workers];
// // //     for (int w = 0; w < num_workers; ++w) {
// // //         pid_t pid = fork();
// // //         if (pid < 0) {
// // //             perror("fork");
// // //             exit(1);
// // //         }
// // //         if (pid == 0) {
// // //             worker_compute(w, num_workers, block_size);
// // //             exit(0);
// // //         }
// // //         pids[w] = pid;
// // //     }

// // //     for (int w = 0; w < num_workers; ++w) {
// // //         waitpid(pids[w], NULL, 0);
// // //     }

// // //     master_collect(num_workers);
// // //     return 0;
// // // }

