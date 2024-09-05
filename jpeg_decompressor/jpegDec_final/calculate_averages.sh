#!/bin/bash

LOG_DIR="logs"
OUTPUT_FILE="summary_table.txt"

# Initialize the output file
echo -e "Quality\tHost to Device (us)\tDevice to Host (us)\tKernel Execution (us)\tE2E (us)" > $OUTPUT_FILE

# Function to process a single log file and calculate averages
process_log_file() {
    local log_file=$1

    # Extract the quality factor from the log file name
    local quality_factor=$(basename $log_file | sed -n 's/.*data_jpeg_\([0-9]\+\)_.*\.log/\1/p')

    # Extract and average values using awk
    host_to_device_avg=$(awk '/INFO: Data transfer from host to device/ {sum += $(NF-1); count++} END {if (count > 0) print sum/count; else print 0}' $log_file)
    device_to_host_avg=$(awk '/INFO: Data transfer from device to host/ {sum += $(NF-1); count++} END {if (count > 0) print sum/count; else print 0}' $log_file)
    avg_kernel_exec_avg=$(awk '/INFO: Average kernel execution per run/ {sum += $(NF-1); count++} END {if (count > 0) print sum/count; else print 0}' $log_file)
    avg_e2e_avg=$(awk '/INFO: Average E2E per run/ {sum += $(NF-1); count++} END {if (count > 0) print sum/count; else print 0}' $log_file)

    # Append the results to the output file
    echo -e "$quality_factor\t$host_to_device_avg\t$device_to_host_avg\t$avg_kernel_exec_avg\t$avg_e2e_avg" >> $OUTPUT_FILE
}

# Process each log file in the log directory
for log_file in $LOG_DIR/*.log; do
    process_log_file $log_file
done

echo "Summary table has been created: $OUTPUT_FILE"

