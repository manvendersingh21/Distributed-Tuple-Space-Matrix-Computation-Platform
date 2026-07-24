 #!/bin/bash

 # Compilation (assuming you have a Makefile)
 make

 # Number of workers to use
 num_workers=4 # You can adjust this

 # Port number for the tuple space
 port=38767 # Use the port your tuple space server is listening on

 # Matrix size to test
 matrix_size=4

 # Array of block sizes to test (granularity)
 block_sizes=(1 2 4 8) # Vary block size to change granularity

 # Output file to store results
 results_file="granularity_results.txt"

 # Print header to the results file
 echo "Matrix Size\tBlock Size\tElapsed Time (seconds)" > "$results_file"
 echo "-----------\t----------\t-----------------------" >> "$results_file"

 # Run the matrix multiplication tests
 echo "Running matrix multiplication tests for matrix size $matrix_size with $num_workers workers..."

 for block_size in "${block_sizes[@]}"; do
     echo "Testing block size: $block_size"

     # Record start time with nanosecond precision
     start_time_sec=$(date +%s)
     start_time_nsec=$(date +%N)

     # Execute the matrix program with the current block size
     ./matrix "$num_workers" "$port" "$matrix_size"

     # Record end time with nanosecond precision
     end_time_sec=$(date +%s)
     end_time_nsec=$(date +%N)

     # Calculate elapsed time in seconds and nanoseconds, forcing decimal
     elapsed_sec=$((10#$end_time_sec - 10#$start_time_sec))
     elapsed_nsec=$((10#$end_time_nsec - 10#$start_time_nsec))

     # Handle negative nanoseconds
     if [[ "$elapsed_nsec" -lt 0 ]]; then
         elapsed_sec=$((elapsed_sec - 1))
         elapsed_nsec=$((elapsed_nsec + 1000000000))
     fi

     # Calculate milliseconds
     elapsed_ms=$((elapsed_nsec / 1000000))

     # Format the elapsed time string robustly
     elapsed_time=$(printf "%d.%03d" "$elapsed_sec" "$elapsed_ms")

     echo "  Block Size: $block_size, Elapsed Time: $elapsed_time seconds"

     # Append the result to the results file
     echo "$matrix_size\t$block_size\t$elapsed_time" >> "$results_file"
 done

 # Print a message indicating where the results are saved
 echo "Results saved to $results_file"
 