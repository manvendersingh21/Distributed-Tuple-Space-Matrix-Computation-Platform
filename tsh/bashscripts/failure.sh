#!/bin/bash

# --- Configuration ---
MATRIX_EXEC="./matrix"   # Path to your compiled matrix program
TSH_EXEC="./tsh"       # Path to your tuple space server program

NUM_WORKERS=4          # Number of workers to use
MATRIX_SIZE=16         # Size of the matrix (NxN)
PORT=39876             # Port for the tuple space server (use a different one)

# Time to wait before killing a worker in the fault test (seconds)
KILL_DELAY=7

# Log file setup
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_DIR="test_logs_${TIMESTAMP}"
BASELINE_LOG="${LOG_DIR}/baseline_run.log"
FAULT_TEST_LOG="${LOG_DIR}/fault_test_run.log"
SUMMARY_LOG="${LOG_DIR}/summary.log"
TIME_FORMAT="\nRealTime=%es\nUserTime=%Us\nSysTime=%Ss\nExitCode=%x"

# --- Ensure executables exist ---
if [ ! -x "$MATRIX_EXEC" ]; then
    echo "Error: Matrix executable '$MATRIX_EXEC' not found or not executable."
    exit 1
fi
if [ ! -x "$TSH_EXEC" ]; then
    echo "Error: TSH server executable '$TSH_EXEC' not found or not executable."
    exit 1
fi

# --- Setup ---
mkdir -p "$LOG_DIR"
echo "Test logs will be saved in: $LOG_DIR" | tee "$SUMMARY_LOG"
echo "Timestamp: $TIMESTAMP" | tee -a "$SUMMARY_LOG"
echo "-----------------------------" | tee -a "$SUMMARY_LOG"
echo "Parameters:" | tee -a "$SUMMARY_LOG"
echo "  Matrix Exec: $MATRIX_EXEC" | tee -a "$SUMMARY_LOG"
echo "  TSH Exec:    $TSH_EXEC" | tee -a "$SUMMARY_LOG"
echo "  Workers:     $NUM_WORKERS" | tee -a "$SUMMARY_LOG"
echo "  Matrix Size: $MATRIX_SIZE" | tee -a "$SUMMARY_LOG"
echo "  Port:        $PORT" | tee -a "$SUMMARY_LOG"
echo "  Kill Delay:  $KILL_DELAY seconds" | tee -a "$SUMMARY_LOG"
echo "-----------------------------" | tee -a "$SUMMARY_LOG"

# --- Server Handling ---
SERVER_PID=0
function start_server() {
    echo "Starting Tuple Space server ($TSH_EXEC) on port $PORT..."
    "$TSH_EXEC" "$PORT" &> "${LOG_DIR}/tsh_server.log" &
    SERVER_PID=$!
    sleep 2 # Give server time to start
    if ! ps -p $SERVER_PID > /dev/null; then
        echo "Error: Failed to start TSH server. Check ${LOG_DIR}/tsh_server.log" | tee -a "$SUMMARY_LOG"
        exit 1
    fi
    echo "Server started with PID $SERVER_PID." | tee -a "$SUMMARY_LOG"
}

function stop_server() {
    if [ $SERVER_PID -ne 0 ] && ps -p $SERVER_PID > /dev/null; then
        echo "Stopping Tuple Space server (PID $SERVER_PID)..."
        kill $SERVER_PID
        wait $SERVER_PID 2>/dev/null # Wait for it to exit, suppress "Terminated" message
        echo "Server stopped." | tee -a "$SUMMARY_LOG"
    else
        echo "Server process $SERVER_PID not found or already stopped." | tee -a "$SUMMARY_LOG"
    fi
    SERVER_PID=0
}

# Trap to ensure server is stopped on script exit/interrupt
trap stop_server EXIT INT TERM

# --- Start Server ---
start_server

# --- Baseline Test ---
echo -e "\n--- Running Baseline Test (No Worker Killing) ---" | tee -a "$SUMMARY_LOG"
echo "Command: $MATRIX_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE" | tee -a "$SUMMARY_LOG"
echo "Output logged to: $BASELINE_LOG"

# Use time command to capture timing and exit status
{ time -p $MATRIX_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE ; } &> "$BASELINE_LOG"
BASELINE_EXIT_CODE=$? # Capture exit code immediately

# Extract time from log (adjust parsing if your `time -p` format differs)
BASELINE_REAL_TIME=$(grep 'real' "$BASELINE_LOG" | awk '{print $2}')

echo "Baseline test finished." | tee -a "$SUMMARY_LOG"
echo "  Exit Code: $BASELINE_EXIT_CODE" | tee -a "$SUMMARY_LOG"
echo "  Real Time: ${BASELINE_REAL_TIME}s" | tee -a "$SUMMARY_LOG"
echo "-----------------------------" | tee -a "$SUMMARY_LOG"

sleep 2 # Small pause between tests

# --- Fault Injection Test ---
echo -e "\n--- Running Fault Injection Test (Killing 1 Worker) ---" | tee -a "$SUMMARY_LOG"
echo "Command: $MATRIX_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE &" | tee -a "$SUMMARY_LOG"
echo "Output logged to: $FAULT_TEST_LOG"

# Start client in background
$MATRIX_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE &> "$FAULT_TEST_LOG" &
CLIENT_PID=$!
echo "Client started with PID: $CLIENT_PID" | tee -a "$FAULT_TEST_LOG"

echo "Waiting $KILL_DELAY seconds before attempting kill..." | tee -a "$FAULT_TEST_LOG"
sleep $KILL_DELAY

# Find worker PIDs (children of the client process)
# Using pgrep -P is generally reliable for direct children
WORKER_PIDS=$(pgrep -P $CLIENT_PID)

if [ -z "$WORKER_PIDS" ]; then
    echo "Warning: Could not find any worker processes (children of $CLIENT_PID)." | tee -a "$SUMMARY_LOG" "$FAULT_TEST_LOG"
    # Decide how to proceed: wait longer? fail test? continue and let client finish?
    # For now, we'll just wait for the client without killing.
else
    NUM_WORKERS_FOUND=$(echo "$WORKER_PIDS" | wc -l)
    echo "Found worker PIDs: $WORKER_PIDS" | tee -a "$FAULT_TEST_LOG"
    echo "Number of workers found: $NUM_WORKERS_FOUND" | tee -a "$FAULT_TEST_LOG"

    # Select a random worker to kill
    WORKER_TO_KILL=$(echo "$WORKER_PIDS" | shuf -n 1)

    echo "Attempting to kill worker PID: $WORKER_TO_KILL ..." | tee -a "$FAULT_TEST_LOG"
    if kill -KILL "$WORKER_TO_KILL"; then
        echo "Sent SIGKILL to PID $WORKER_TO_KILL." | tee -a "$FAULT_TEST_LOG"
    else
        echo "Failed to send SIGKILL to PID $WORKER_TO_KILL." | tee -a "$FAULT_TEST_LOG" "$SUMMARY_LOG"
    fi
fi

echo "Waiting for client process $CLIENT_PID to complete..." | tee -a "$FAULT_TEST_LOG"
# Use `time` on the `wait` command to capture total time until the background client finishes
{ time -p wait $CLIENT_PID ; } &>> "$FAULT_TEST_LOG" # Append time output to log
FAULT_EXIT_CODE=$? # Exit code of the background client process

# Extract time from log (appending might make this slightly trickier, get last 'real')
FAULT_REAL_TIME=$(grep 'real' "$FAULT_TEST_LOG" | tail -n 1 | awk '{print $2}')

echo "Fault injection test finished." | tee -a "$SUMMARY_LOG"
echo "  Exit Code: $FAULT_EXIT_CODE" | tee -a "$SUMMARY_LOG"
echo "  Real Time: ${FAULT_REAL_TIME}s" | tee -a "$SUMMARY_LOG"
echo "-----------------------------" | tee -a "$SUMMARY_LOG"


# --- Summary ---
echo -e "\n--- Test Summary ---" | tee -a "$SUMMARY_LOG"
FINAL_VERDICT="FAIL"

# Check Baseline: Should succeed (Exit Code 0)
if [ "$BASELINE_EXIT_CODE" -eq 0 ]; then
    echo "Baseline Run: PASSED (Exit Code 0)" | tee -a "$SUMMARY_LOG"
    BASELINE_PASS=true
else
    echo "Baseline Run: FAILED (Exit Code $BASELINE_EXIT_CODE, expected 0)" | tee -a "$SUMMARY_LOG"
    BASELINE_PASS=false
fi

# Check Fault Test: Should fail gracefully (Exit Code non-zero, ideally 1)
# Allow any non-zero code, but print a note if it's not 1 (EXIT_FAILURE)
if [ "$FAULT_EXIT_CODE" -ne 0 ]; then
    echo "Fault Test Run: PASSED (Exit Code $FAULT_EXIT_CODE, non-zero as expected)" | tee -a "$SUMMARY_LOG"
    if [ "$FAULT_EXIT_CODE" -ne 1 ]; then
         echo "  -> Note: Exit code was not 1 (EXIT_FAILURE), check if this is intended." | tee -a "$SUMMARY_LOG"
    fi
    FAULT_PASS=true
else
    echo "Fault Test Run: FAILED (Exit Code 0, expected non-zero)" | tee -a "$SUMMARY_LOG"
    FAULT_PASS=false
fi

# Final Verdict
if $BASELINE_PASS && $FAULT_PASS; then
    FINAL_VERDICT="PASS"
    echo -e "\nOverall Result: PASS" | tee -a "$SUMMARY_LOG"
else
    echo -e "\nOverall Result: FAIL" | tee -a "$SUMMARY_LOG"
fi
echo "-----------------------------" | tee -a "$SUMMARY_LOG"

echo "Test complete. Check logs in '$LOG_DIR' for details."

# Return overall status (0 for PASS, 1 for FAIL)
if [ "$FINAL_VERDICT" = "PASS" ]; then
    exit 0
else
    exit 1
fi