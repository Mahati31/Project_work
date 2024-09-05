#!/bin/bash

# Define the base directory where the JPEG data is stored
BASE_DIR="jpeg_data"
LOG_DIR="logs"

# Create the logs directory if it doesn't exist
mkdir -p $LOG_DIR

# Function to run the application on each image in a given directory
process_directory() {
    local dir=$1
    local log_file=$2

    # Create or clear the log file
    : > $log_file

    # Find all JPEG files in the directory
    find $dir -type f -name "*.JPEG" | while read -r image; do
        echo "Processing $image"
        ./build_dir.hw.xilinx_u280_gen3x16_xdma_1_202211_1/host.exe -xclbin build_dir.hw.xilinx_u280_gen3x16_xdma_1_202211_1/kernelJpegDecoder.xclbin -JPEGFile "$image" >> $log_file 2>&1
    done
}

# Traverse all quality directories
find $BASE_DIR -type d -name "data_jpeg_*" | while read -r quality_dir; do
    # Traverse all image directories within each quality directory
    find $quality_dir -type d -name "n01440764" | while read -r image_dir; do
        # Generate a log file name based on the directory structure
        log_file="$LOG_DIR/$(echo $image_dir | tr '/' '_').log"

        echo "Processing directory $image_dir, logging to $log_file"
        process_directory $image_dir $log_file
    done
done

