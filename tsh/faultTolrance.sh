 #!/bin/bash
 

 # Configuration (adjust as needed)
 num_workers=4
 port=38771
 matrix_size=8
 test_duration=20  # How long to run the test
 failure_probability=60 # Increased chance of failure
 results_file="fault_tolerance_test_log.txt"
 tsh_exec="./tsh" # Path to your TSH server executable
 matrix_exec="./matrix" # Path to your matrix executable
 

 # Function to simulate killing a worker
 kill_worker() {
  local worker_pid=$1
  if [[ -n "$worker_pid" ]]; then
  echo "[chaos] Killing worker process (PID $worker_pid)" >> "$results_file" 2>&1
  kill -9 "$worker_pid"
  else
  echo "[chaos] No worker PID to kill" >> "$results_file" 2>&1
  fi
 }
 

 # 1. Compile the C code
 echo "Compiling the matrix program..."
 make  > "$results_file" 2>&1
 if [ $? -ne 0 ]; then
  echo "Compilation failed! Check $results_file"
  exit 1
 fi
 

 # 2. Start the tuple space server
 echo "Starting tuple space server on port $port..."
 if [ -z "$tsh_exec" ]; then
    echo "Error: TSH executable not specified. Please set the 'tsh_exec' variable."
    exit 1
 fi
 "$tsh_exec" "$port" > "$results_file" 2>&1 &
 server_pid=$!
 sleep 3 # Give the server a little time to start
 

 #check if the server is running
 if ! ps -p "$server_pid" > /dev/null; then
        echo "Error: TSH server did not start.  Exiting."
        exit 1
 fi
 

 # 3. Run the matrix program
 echo "Running matrix program (size $matrix_size, $num_workers workers)..."
 "$matrix_exec" "$num_workers" "$port" "$matrix_size" > "$results_file" 2>&1 &
 matrix_pid=$!
 sleep 2
 

 # Get initial worker pids
 worker_pids=($(pgrep -P "$matrix_pid"))
 if [ -z "$worker_pids" ]; then
  echo "Error: No worker processes found." >> "$results_file" 2>&1
  exit 1
 fi
  echo "Initial worker PIDs: ${worker_pids[*]}"  >> "$results_file" 2>&1
 

 # 4. Simulate worker failures
 echo "Simulating worker failures for $test_duration seconds..." >> "$results_file" 2>&1
 start_time=$(date +%s)
 end_time=$((start_time + test_duration))
 num_kills=0
 while [ $(date +%s) -lt "$end_time" ]; do
  sleep 1
  if [ $((RANDOM % 100)) -lt "$failure_probability" ]; then
  # Kill a random worker
  worker_pids=($(pgrep -P "$matrix_pid" | tr '\n' ' ')) #refresh the worker_pids
  if [ ${#worker_pids[@]} -gt 0 ]; then
    random_index=$((RANDOM % ${#worker_pids[@]}))
    kill_pid=${worker_pids[$random_index]}
    kill_random_worker "$kill_pid"
    num_kills=$((num_kills + 1))
  fi
  fi
 done
 echo "Simulated $num_kills worker failures."  >> "$results_file" 2>&1
 

 # 5. Wait for the matrix program to finish
 echo "Waiting for the matrix program to finish..." >> "$results_file" 2>&1
 wait "$matrix_pid"
 matrix_exit_code=$?
 echo "Matrix program exited with code: $matrix_exit_code"  >> "$results_file" 2>&1
 

 # 6. Check for success and fault tolerance
 echo "Analyzing results..."  >> "$results_file" 2>&1
 if [ "$matrix_exit_code" -eq 0 ]; then
  echo "  Test Passed: Matrix multiplication completed successfully."  >> "$results_file" 2>&1
 else
  echo "  Test Failed: Matrix multiplication finished with an error."  >> "$results_file" 2>&1
 fi
 

 grep -q "Forked new worker" "$results_file"
 if [ $? -eq 0 ]; then
  echo "  Test Passed: Worker revival detected."  >> "$results_file" 2>&1
 else
  echo "  Test Failed: Worker revival not detected."  >> "$results_file" 2>&1
 fi
  
 grep -q "Result C:" "$results_file"
  if [ $? -eq 0 ]; then
    echo "  Test Passed: Final result C was produced."  >> "$results_file" 2>&1
  else
     echo "  Test Failed: Final result C was not produced."  >> "$results_file" 2>&1
  fi
 

 # 7. Kill the tuple space server
 echo "Stopping tuple space server..."  >> "$results_file" 2>&1
 kill "$server_pid"
 echo "Tuple space server stopped."  >> "$results_file" 2>&1
 

 echo "Test finished. Check $results_file for detailed output."
 ​
 exit 0
 

