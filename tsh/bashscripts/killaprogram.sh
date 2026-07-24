#!/bin/bash

# === Configuration ===
PORT=38789
NUM_WORKERS=4
MATRIX_SIZE=16 # Use a larger size where worker failure impact is more likely visible
SERVER_EXEC="./tsh"
CLIENT_EXEC="./matrix" # Assumes this is the fault-tolerant version
MAKE_TARGETS=""
SERVER_STARTUP_WAIT=2
KILL_DELAY=5       # Seconds to wait after client starts before killing a worker
NUM_KILLS=1        # How many workers to attempt to kill
RUN_LOG_FILE="fault_test_run_$(date +%Y%m%d_%H%M%S).log"
RESULTS_FILE="fault_test_timing_$(date +%Y%m%d_%H%M%S).csv"

# === End Configuration ===

exec > "$RUN_LOG_FILE" 2>&1

# ... (Print Configuration, Cleanup Function, Compile Code, Check Executables) ...

# --- Start Server ---
# ... (Start the server './tsh $PORT &' and get SERVER_PID) ...
echo "--- Starting Server ---"
echo "Command: $SERVER_EXEC $PORT &"
"$SERVER_EXEC" "$PORT" &
SERVER_PID=$!

# --- Run Test WITH Worker Killing ---
echo "--- Running Test WITH Worker Killing (Size: $MATRIX_SIZE) ---"

# Start the client program in the background
echo "Starting client: $CLIENT_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE &"
# Use time -p, redirect stderr to stdout to capture time output later
# Run client in background to allow the script to kill workers
TMP_OUTPUT_FILE=$(mktemp)
( time -p "$CLIENT_EXEC" "$NUM_WORKERS" "$PORT" "$MATRIX_SIZE" ) > "$TMP_OUTPUT_FILE" 2>&1 &
CLIENT_PID=$! # Get the PID of the main client process
echo "Client started with PID: $CLIENT_PID"

# Wait for a short period to allow workers to start
echo "Waiting $KILL_DELAY seconds before attempting kill..."
sleep $KILL_DELAY

# Find worker PIDs (children of the client process)
# This assumes worker processes have the same name or can be identified as children
# Adjust the pgrep pattern if necessary based on your actual worker executable/process name
WORKER_PIDS=$(pgrep -P $CLIENT_PID) # Find children of the client PID

if [[ -z "$WORKER_PIDS" ]]; then
    echo "Warning: Could not find worker processes for client PID $CLIENT_PID."
else
    echo "Found worker PIDs: $WORKER_PIDS"
    # Convert PIDs to an array
    read -r -a WORKER_PID_ARRAY <<< "$WORKER_PIDS"
    NUM_FOUND_WORKERS=${#WORKER_PID_ARRAY[@]}
    echo "Number of workers found: $NUM_FOUND_WORKERS"

    # Randomly select worker(s) to kill
    if [[ $NUM_FOUND_WORKERS -gt 0 ]]; then
        echo "Attempting to kill $NUM_KILLS worker(s) randomly..."
        # Use shuf to randomize and head to select
        PIDS_TO_KILL=$(echo "$WORKER_PIDS" | shuf | head -n "$NUM_KILLS")

        for W_PID in $PIDS_TO_KILL; do
            echo "Killing worker PID: $W_PID ..."
            kill -9 "$W_PID" # Send SIGKILL signal
            if [ $? -eq 0 ]; then
                echo "Sent SIGKILL to PID $W_PID."
            else
                echo "Failed to send SIGKILL to PID $W_PID (maybe already finished?)."
            fi
        done
    else
         echo "No running workers found to kill."
    fi
fi

# Wait for the main client process to finish
echo "Waiting for client process $CLIENT_PID to complete..."
wait $CLIENT_PID
CLIENT_EXIT_CODE=$?
echo "Client process $CLIENT_PID finished with exit code $CLIENT_EXIT_CODE."

# Process the time output captured earlier
TIME_OUTPUT=$(cat "$TMP_OUTPUT_FILE")
rm "$TMP_OUTPUT_FILE"
echo "--- Raw Time & Client Output (With Kill) ---"
echo "$TIME_OUTPUT"
echo "--- End Raw Output ---"
REAL_TIME_KILL=$(echo "$TIME_OUTPUT" | awk '/^real/ {print $2}')
USER_TIME_KILL=$(echo "$TIME_OUTPUT" | awk '/^user/ {print $2}')
SYS_TIME_KILL=$(echo "$TIME_OUTPUT" | awk '/^sys/ {print $2}')
REAL_TIME_KILL=${REAL_TIME_KILL:-"NaN"}
USER_TIME_KILL=${USER_TIME_KILL:-"NaN"}
SYS_TIME_KILL=${SYS_TIME_KILL:-"NaN"}

# --- Run Test WITHOUT Worker Killing (Baseline) ---
echo
echo "--- Running Test WITHOUT Worker Killing (Size: $MATRIX_SIZE) ---"
echo "Client Command: time -p $CLIENT_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE"
TMP_OUTPUT_FILE=$(mktemp)
( time -p "$CLIENT_EXEC" "$NUM_WORKERS" "$PORT" "$MATRIX_SIZE" ) > "$TMP_OUTPUT_FILE" 2>&1
BASELINE_EXIT_CODE=$?
TIME_OUTPUT=$(cat "$TMP_OUTPUT_FILE")
rm "$TMP_OUTPUT_FILE"
echo "Client finished with exit code $BASELINE_EXIT_CODE."
echo "--- Raw Time & Client Output (Baseline) ---"
echo "$TIME_OUTPUT"
echo "--- End Raw Output ---"
REAL_TIME_BASE=$(echo "$TIME_OUTPUT" | awk '/^real/ {print $2}')
USER_TIME_BASE=$(echo "$TIME_OUTPUT" | awk '/^user/ {print $2}')
SYS_TIME_BASE=$(echo "$TIME_OUTPUT" | awk '/^sys/ {print $2}')
REAL_TIME_BASE=${REAL_TIME_BASE:-"NaN"}
USER_TIME_BASE=${USER_TIME_BASE:-"NaN"}
SYS_TIME_BASE=${SYS_TIME_BASE:-"NaN"}


# --- Record Results ---
echo "--- Test Results ---"
echo "Writing results to $RESULTS_FILE"
echo "TestType,MatrixSize,NumWorkers,WorkersKilled,ExitCode,RealTime(s),UserTime(s),SystemTime(s)" > "$RESULTS_FILE"
echo "WithKill,$MATRIX_SIZE,$NUM_WORKERS,$NUM_KILLS,$CLIENT_EXIT_CODE,$REAL_TIME_KILL,$USER_TIME_KILL,$SYS_TIME_KILL" >> "$RESULTS_FILE"
echo "Baseline,$MATRIX_SIZE,$NUM_WORKERS,0,$BASELINE_EXIT_CODE,$REAL_TIME_BASE,$USER_TIME_BASE,$SYS_TIME_BASE" >> "$RESULTS_FILE"

echo "Baseline Run (No Kill): Exit=$BASELINE_EXIT_CODE, RealTime=${REAL_TIME_BASE}s"
echo "Fault Test Run (Kill): Exit=$CLIENT_EXIT_CODE, RealTime=${REAL_TIME_KILL}s"
echo "Run log saved to: $RUN_LOG_FILE"
echo "Timing comparison saved to: $RESULTS_FILE"
echo

# --- Cleanup --- (Handled by trap)
exit 0