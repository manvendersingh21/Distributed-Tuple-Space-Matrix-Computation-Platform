 #!/bin/bash

 # Script to measure the execution time of the matrix program

 # Configuration
 num_workers=2 # You can adjust this
 port=38761 # Use the port your tuple space server is listening on
 matrix_size=10 # Size of the matrix to multiply

 # 1. Compile the C code using make
 make
 if [ $? -ne 0 ]; then
  echo "Compilation failed!"
  exit 1
 fi

 # 2. Start the tuple space server (assuming it's running in another terminal)
 #  You need to start your TSH server manually before running this script.
 echo "Make sure your tuple space server is running on port $port!"

 # 3. Run the matrix program and measure the execution time
 echo "Running matrix program (size ${matrix_size}, ${num_workers} workers)..."
 start_time=$(date +%s) # Record start time
 ./matrix "$num_workers" "$port" "$matrix_size" # Execute the matrix program
 end_time=$(date +%s) # Record end time

 # Calculate the elapsed time in seconds
 elapsed_time=$((end_time - start_time))

 # Print the elapsed time
 echo "Elapsed time: $elapsed_time seconds"

 echo "Program finished."

 exit 0
 