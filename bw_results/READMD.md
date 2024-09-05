This Repo consists of the files required to evaluate the bandwidth of the HBM possible at different image sizes.

The source code used as reference is from the hbm_bandwidth repo of the Vitis_accel_examples at https://github.com/Xilinx/Vitis_Accel_Examples/tree/main/performance/hbm_bandwidth

The bw_script.py is used to change the transaction size at regular intervals within the range of image sizes of the imagenet jpeg images used at different compression quality factors. These transaction sizes are updated in the host.cpp file while creating coppies for each size. These host files are copied to the host connected to the Alveo board and run on it to get the bandwidth at different image sizes back.

