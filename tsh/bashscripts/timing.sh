#!/bin/bash

# === Configuration ===
# Adjust these variables according to your setup

# Port for the tuple space server
PORT=38767

# Number of worker processes for the matrix client
NUM_WORKERS=4

# List of matrix sizes (N x N) to test
# Example: SIZES=(8 16 32 64 128)
SIZES=(8 16) # Keep smaller for quick demo runs

# Path to the server executable
SERVER_EXEC="./tsh"

# Path to the client executable
CLIENT_EXEC="./matrix"

# Optional: Specify specific make targets (e.g., "all"). Leave empty for default.
MAKE_TARGETS=""

# Time (in seconds) to wait for the server to start up
SERVER_STARTUP_WAIT=2

# File to store all general script output (stdout and stderr)
RUN_LOG_FILE="test_run_$(date +%Y%m%d_%H%M%S).log"

# File to store timing results (Size vs Time)
RESULTS_FILE="timing_results_$(date +%Y%m%d_%H%M%S).csv"
# === End Configuration ===


# --- Script Logic ---

# Redirect all subsequent stdout and stderr to the specified run log file
exec > "$RUN_LOG_FILE" 2>&1

# Print configuration details to the log file
echo "--- Script Configuration ---"
echo "Timestamp: $(date)"
echo "Port: $PORT"
echo "Workers: $NUM_WORKERS"
echo "Matrix Sizes to Test: ${SIZES[@]}"
echo "Server Executable: $SERVER_EXEC"
echo "Client Executable: $CLIENT_EXEC"
echo "Make Targets: ${MAKE_TARGETS:-default}"
echo "Server Startup Wait: $SERVER_STARTUP_WAIT seconds"
echo "Run Log File: $RUN_LOG_FILE"
echo "Results File: $RESULTS_FILE"
echo "--------------------------"
echo

# Global variable to store the server's Process ID (PID)
SERVER_PID=""

# Function to clean up the server process upon script exit
cleanup() {
    echo # Newline for cleaner output
    echo "--- Cleaning up ---"
    # Check if the SERVER_PID variable is set and if the process exists
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Stopping server (PID: $SERVER_PID)..."
        # Send SIGTERM first (graceful shutdown)
        kill "$SERVER_PID" 2>/dev/null
        sleep 0.5 # Wait a moment
        # If it's still running, force kill with SIGKILL
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            echo "Server did not stop gracefully, sending SIGKILL..."
            kill -9 "$SERVER_PID" 2>/dev/null
        fi
        wait "$SERVER_PID" 2>/dev/null # Wait for the process to be reaped
        echo "Server stopped."
    elif [[ -n "$SERVER_PID" ]]; then
        echo "Server process (PID: $SERVER_PID) not found or already stopped."
    else
        echo "No server process was started by this script."
    fi
    echo "Run log saved to: $RUN_LOG_FILE"
    echo "Timing results saved to: $RESULTS_FILE"
    echo "Cleanup complete."
}

# Set trap: The 'cleanup' function will be called when the script exits,
# receives an interrupt (Ctrl+C), or termination signal.
trap cleanup EXIT SIGINT SIGTERM

# 1. Compile the code
echo "--- Compiling Code ---"
echo "Running 'make clean'..."
make clean
if [ $? -ne 0 ]; then
    echo "Warning: 'make clean' failed, continuing anyway..."
fi

echo "Running 'make $MAKE_TARGETS'..."
make $MAKE_TARGETS
if [ $? -ne 0 ]; then
    echo "Error: Compilation failed. Exiting."
    exit 1
fi
echo "Compilation successful."
echo

# 2. Check if executables exist and are executable
echo "--- Checking Executables ---"
if [ ! -x "$SERVER_EXEC" ]; then
    echo "Error: Server executable '$SERVER_EXEC' not found or not executable."
    exit 1
fi
if [ ! -x "$CLIENT_EXEC" ]; then
    echo "Error: Client executable '$CLIENT_EXEC' not found or not executable."
    exit 1
fi
echo "Executables found."
echo

# 3. Start the server ONCE before the loop
echo "--- Starting Server ---"
echo "Command: $SERVER_EXEC $PORT &"
"$SERVER_EXEC" "$PORT" &
SERVER_PID=$!
echo "Server potentially started with PID: $SERVER_PID"
echo "Waiting $SERVER_STARTUP_WAIT seconds for server to initialize..."
sleep "$SERVER_STARTUP_WAIT"
if ! ps -p "$SERVER_PID" > /dev/null; then
    echo "Error: Server process (PID: $SERVER_PID) failed to start or terminated prematurely."
    SERVER_PID="" # Unset PID
    exit 1
fi
echo "Server appears to be running (checked via ps)."
echo

# 4. Loop through sizes and run the client, recording time
echo "--- Running Timed Tests ---"
# Create header for the results CSV file
echo "MatrixSize,RealTime(s),UserTime(s),SystemTime(s),ExitCode" > "$RESULTS_FILE"

for CURRENT_SIZE in "${SIZES[@]}"; do
    echo # Newline in run log
    echo "--- Testing Size: $CURRENT_SIZE ---"

    MATRIX_SIZE=$CURRENT_SIZE # Use the current size from the loop

    echo "Client Command: time -p $CLIENT_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE"

    # Use process substitution to capture stderr (where time -p writes)
    # Run the client and capture time output along with any client stderr
    TIME_OUTPUT=$( { time -p "$CLIENT_EXEC" "$NUM_WORKERS" "$PORT" "$MATRIX_SIZE"; } 2>&1 )
    CLIENT_EXIT_CODE=$? # Get exit code of the client command itself

    echo "Client finished for size $CURRENT_SIZE with exit code $CLIENT_EXIT_CODE."
    echo "--- Raw Time & Client Stderr Output ---"
    echo "$TIME_OUTPUT" # Log the full time output and any client stderr
    echo "--- End Raw Time & Client Stderr Output ---"

    # Parse time components using awk for better precision handling
    REAL_TIME=$(echo "$TIME_OUTPUT" | awk '/^real/ {print $2}')
    USER_TIME=$(echo "$TIME_OUTPUT" | awk '/^user/ {print $2}')
    SYS_TIME=$(echo "$TIME_OUTPUT" | awk '/^sys/ {print $2}')

    # Handle cases where time parsing might fail (e.g., client error before time prints)
    REAL_TIME=${REAL_TIME:-"NaN"}
    USER_TIME=${USER_TIME:-"NaN"}
    SYS_TIME=${SYS_TIME:-"NaN"}

    # Append result to the results CSV file
    echo "$CURRENT_SIZE,$REAL_TIME,$USER_TIME,$SYS_TIME,$CLIENT_EXIT_CODE" >> "$RESULTS_FILE"

    if [ $CLIENT_EXIT_CODE -eq 0 ]; then
        echo "Result: Success. Real Time: ${REAL_TIME}s"
    else
        echo "Result: Failure (Exit Code: $CLIENT_EXIT_CODE). Check run log."
        # Optionally add a sleep or break here if one failure means stopping the test
        # sleep 1
    fi
    echo # Newline in run log

done # End of loop through sizes

echo "--- Timing Tests Complete ---"
echo "Timing results stored in: $RESULTS_FILE"
echo # Newline in run log

# 5. Cleanup (will be handled automatically by the trap)
# Exit with 0 if the loop completed, regardless of individual client failures (which are logged)
exit 0
