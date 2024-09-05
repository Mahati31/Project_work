#!/bin/bash


# Input file
input_file="j2k_acc_results"

# Output CSV file
output_file="j2k_results.csv"
output_top5_file="j2k_results_top5.csv"

# Remove existing output file if it exists
rm -f "$output_file"
rm -f "$output_top5_file"

# Grep lines containing "Test Accuracy" and extract 4th and 5th columns
grep "Test Accuracy" "$input_file" | awk '{print $6 "," $3}' > "$output_file"

# Grep lines containing "Test Accuracy" and extract 4th and 5th columns
grep "Top-5" "$input_file" | awk '{print $6 "," $3}' > "$output_top5_file"

echo "Data has been extracted and saved to $output_file"
