#!/bin/bash

# === Configuration ===
# Adjust these variables according to your setup

# Port for the tuple space sserver
PORT=36754

# Number of worker processes for the matrix client
NUM_WORKERS=4

# Size of the square msatrices (N x N)
MATRIX_SIZE=8 # Keep reasonably small for testing, e.g., 4, 8, 16

# Path to the server executable
SERVER_EXEC="./tsh"

# Path to the client executable
CLIENT_EXEC="./matrix"

# Optional: Specify specific make targets (e.g., "all"). Leave empty for default.
MAKE_TARGETS=""

# Time (in seconds) to wait for the server to start up
SERVER_STARTUP_WAIT=2

# File to store all output (stdout and stderr) from this script
OUTPUT_LOG_FILE="test_run_$(date +%Y%m%d_%H%M%S).log"
# === End Configuration ===


# --- Script Logic ---

# Redirect all subsequent stdout and stderr to the specified log file
# Note: Output before this line (like errors parsing the script itself) won't be captured.
exec > "$OUTPUT_LOG_FILE" 2>&1

# Print configuration details to the log file
echo "--- Script Configuration ---"
echo "Timestamp: $(date)"
echo "Port: $PORT"
echo "Workers: $NUM_WORKERS"
echo "Matrix Size: $MATRIX_SIZE"
echo "Server Executable: $SERVER_EXEC"
echo "Client Executable: $CLIENT_EXEC"
echo "Make Targets: ${MAKE_TARGETS:-default}"
echo "Server Startup Wait: $SERVER_STARTUP_WAIT seconds"
echo "Output Log File: $OUTPUT_LOG_FILE"
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
        # Wait a moment
        sleep 0.5
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
    # Optional: Clean up compiled files
    # echo "Running make clean..."
    # make clean > /dev/null 2>&1
    echo "Cleanup complete."
    # Note: Final exit messages might not appear in the log if the script exits abruptly here.
}

# Set trap: The 'cleanup' function will be called when the script exits,
# receives an interrupt (Ctrl+C), or termination signal.
trap cleanup EXIT SIGINT SIGTERM

# 1. Compile the code
echo "--- Compiling Code ---"
# Optional: Clean previous build artifacts
echo "Running 'make clean'..."
make clean # Output will go to the log file
if [ $? -ne 0 ]; then
    echo "Warning: 'make clean' failed, continuing anyway..."
fi

# Build the project
echo "Running 'make $MAKE_TARGETS'..."
make $MAKE_TARGETS # Output will go to the log file
# Check if make was successful
if [ $? -ne 0 ]; then
    echo "Error: Compilation failed. Exiting."
    # The exit message below might not make it to the log file if exec fails early
    # but the cleanup trap should still run.
    exit 1 # Exit immediately, cleanup trap will run
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

# 3. Start the server in the background
echo "--- Starting Server ---"
echo "Command: $SERVER_EXEC $PORT &"
# Execute the server in the background - its output will also go to the log file
"$SERVER_EXEC" "$PORT" &
# Capture the Process ID (PID) of the last background command
SERVER_PID=$!
echo "Server potentially started with PID: $SERVER_PID"

# Give the server a moment to initialize
echo "Waiting $SERVER_STARTUP_WAIT seconds for server to initialize..."
sleep "$SERVER_STARTUP_WAIT"

# Verify the server is actually running
# Use ps command as kill -0 output might be suppressed by redirection
if ! ps -p "$SERVER_PID" > /dev/null; then
    echo "Error: Server process (PID: $SERVER_PID) failed to start or terminated prematurely."
    SERVER_PID="" # Unset PID so cleanup doesn't try to kill a non-existent process
    exit 1
fi
echo "Server appears to be running (checked via ps)."
echo

# 4. Run the client
echo "--- Running Client ---"
echo "Command: $CLIENT_EXEC $NUM_WORKERS $PORT $MATRIX_SIZE"
# Execute the client in the foreground - its output goes to the log file
"$CLIENT_EXEC" "$NUM_WORKERS" "$PORT" "$MATRIX_SIZE"
# Capture the exit code of the client
CLIENT_EXIT_CODE=$?
echo "Client finished."
echo

# 5. Report result
echo "--- Test Result ---"
if [ $CLIENT_EXIT_CODE -eq 0 ]; then
    echo "Success: Client exited normally (Exit Code: 0)."
else
    echo "Failure: Client exited with error (Exit Code: $CLIENT_EXIT_CODE)."
fi
echo

# 6. Cleanup (will be handled automatically by the trap)
# The script will exit here, triggering the 'cleanup' function via the trap.
# Exit with the client's exit code to allow scripting based on success/failure.
exit $CLIENT_EXIT_CODE

