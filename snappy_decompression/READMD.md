The reference snappy decompression code is taken from the project at https://github.com/XorJoep/HBM/tree/master and is altered to be run on the HACC cluster Alveo u280 board.

A set of files including the Makefile and the benchmarking script (benchmark.sh) is shown here with the required changes. Though these files are taken from the baseline64 benchmarks in configurations 1 and 2, similar changes are required in all the benchmarking paths. 

The platform is changed to reflect the alveo u280 board by setting
"DEVICE ?= xilinx_u280_xdma_201920_3"

In the benchmarking scripts the library "libstdc++.so.6" is located in a different path on the HACC cluster depending on the version of Vivado uses. In our implementation it is set as follows.

"export LD_PRELOAD="/tools/Xilinx/Vivado/2022.1/tps/lnx64/gcc-6.2.0/lib64/libstdc++.so.6"
"

The benchmarks need to be build on the "hacc_build" system and run on one of the 4 available systems with the alveo u280 boards available, this is due to the fact that Vitis tools used in building the benchmarks are only available on the build system. 

