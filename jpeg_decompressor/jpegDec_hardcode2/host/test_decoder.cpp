/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define _HLS_TEST_ 1

#ifndef _HLS_TEST_
#include "xcl2.hpp"
#endif
#ifndef __SYNTHESIS__
#include <sys/time.h>
#include <new>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <vector>
#endif

#ifdef _HLS_TEST_
#include "kernelJpegDecoder.hpp"
#else
#include "utils_XAcc_jpeg.hpp"
#include "xf_utils_sw/logger.hpp"
#endif

// ------------------------------------------------------------
#ifndef __SYNTHESIS__

#if __linux
template <typename T>
T* aligned_alloc(std::size_t num) {
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 4096, num * sizeof(T))) {
        throw std::bad_alloc();
    }
    return reinterpret_cast<T*>(ptr);
}
#endif

// ------------------------------------------------------------
// Compute time difference
unsigned long diff(const struct timeval* newTime, const struct timeval* oldTime) {
    return (newTime->tv_sec - oldTime->tv_sec) * 1000000 + (newTime->tv_usec - oldTime->tv_usec);
}

// ------------------------------------------------------------
// load the data file (.txt, .bin, .jpg ...)to ptr
template <typename T>
int load_dat(T*& data, const std::string& name, int& size) {
    uint64_t n;
    std::string fn = name;
    FILE* f = fopen(fn.c_str(), "rb");
    std::cout << "WARNING: " << fn << " will be opened for binary read." << std::endl;
    if (!f) {
        std::cerr << "ERROR: " << fn << " cannot be opened for binary read." << std::endl;
        return -1;
    }

    fseek(f, 0, SEEK_END);
    n = (uint64_t)ftell(f);
    if (n > MAX_DEC_PIX) {
        std::cout << " read n bytes > MAX_DEC_PIX, please set a larger MAX_DEC_PIX " << std::endl;
        return 1;
    }
#if __linux
    data = aligned_alloc<T>(n);
#else
    data = (T*)malloc(MAX_DEC_PIX);
#endif
    fseek(f, 0, SEEK_SET);
    size = fread(data, sizeof(char), n, f);
    fclose(f);
    std::cout << n << " entries read from " << fn << std::endl;

    return 0;
}

// ------------------------------------------------------------
// get the arg
class ArgParser {
   public:
    ArgParser(int& argc, const char* argv[]) {
        for (int i = 1; i < argc; ++i) mTokens.push_back(std::string(argv[i]));
    }
    bool getCmdOption(const std::string option, std::string& value) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(this->mTokens.begin(), this->mTokens.end(), option);
        if (itr != this->mTokens.end() && ++itr != this->mTokens.end()) {
            value = *itr;
            return true;
        }
        return false;
    }
    bool getCmdOption(const std::string option) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(this->mTokens.begin(), this->mTokens.end(), option);
        if (itr != this->mTokens.end())
            return true;
        else
            return false;
    }

   private:
    std::vector<std::string> mTokens;
};

// ------------------------------------------------------------
// for HBM
#define XCL_BANK(n) (((unsigned int)(n)) | XCL_MEM_TOPOLOGY)

#define XCL_BANK0 XCL_BANK(0)
#define XCL_BANK1 XCL_BANK(1)
#define XCL_BANK2 XCL_BANK(2)

#define XCL_BANK3 XCL_BANK(3)
#define XCL_BANK4 XCL_BANK(4)
#define XCL_BANK5 XCL_BANK(5)

// ------------------------------------------------------------
// for tmp application and reorder
int16_t* hls_block_0 = (int16_t*)malloc(sizeof(int16_t) * MAX_NUM_COLOR * MAXCMP_BC * 64);
int16_t* hls_block_1 = (int16_t*)malloc(sizeof(int16_t) * MAX_NUM_COLOR * MAXCMP_BC * 64);
xf::codec::idct_out_t* yuv_row_pointer_0 = (uint8_t*)malloc(sizeof(uint8_t) * MAX_NUM_COLOR * MAXCMP_BC * 64);
xf::codec::idct_out_t* yuv_row_pointer_1 = (uint8_t*)malloc(sizeof(uint8_t) * MAX_NUM_COLOR * MAXCMP_BC * 64);
// ------------------------------------------------------------
// input strm_iDCT_x8[8] is the row of block yuv in mcu order of sample
// output image_height*image_width*Y ... image_height*image_width*U ... image_height*image_width*V 0a to form a file to
// show the picture
void rebuild_raw_yuv(std::string file_name,
                     xf::codec::bas_info* bas_info,
                     int hls_bc[MAX_NUM_COLOR],
                     // hls::stream<xf::codec::idct_out_t >   strm_iDCT_x8[8],
                     ap_uint<64>* yuv_mcu_pointer) {
    std::string file = file_name.substr(file_name.find_last_of('/') + 1);
    std::string fn = file.substr(0, file.find_last_of(".")) + ".raw";
    FILE* f = fopen(fn.c_str(), "wb");
    std::cout << "WARNING: " << fn << " will be opened for binary write." << std::endl;
    if (!f) {
        std::cerr << "ERROR: " << fn << " cannot be opened for binary write." << std::endl;
    }

    xf::codec::idct_out_t* yuv_mcu_pointer_pix = (uint8_t*)malloc(sizeof(uint8_t) * bas_info->all_blocks * 64);

    int cnt = 0;
    int cnt_row = 0;
    for (int b = 0; b < (int)(bas_info->all_blocks); b++) {
        for (int i = 0; i < 8; i++) { // write one block of Y or U or V
            for (int j = 0; j < 8; j++) {
                yuv_mcu_pointer_pix[cnt] = yuv_mcu_pointer[cnt_row](8 * (j + 1) - 1, 8 * j); // strm_iDCT_x8[j].read();
                cnt++;
            }
            cnt_row++;
        }
    }

write_mcu_raw_data:
    fwrite(yuv_mcu_pointer, sizeof(char), bas_info->all_blocks * 64, f);

    // fwrite(&end_file, 1, 1, f);//write 0x0a
    fclose(f);

    fn = file.substr(0, file.find(".")) + ".yuv";
    f = fopen(fn.c_str(), "wb");
    std::cout << "WARNING: " << fn << " will be opened for binary write." << std::endl;
    if (!f) {
        std::cerr << "ERROR: " << fn << " cannot be opened for binary write." << std::endl;
    }

    xf::codec::COLOR_FORMAT fmt = bas_info->format;

    int dpos[MAX_NUM_COLOR]; // the dc position of the pointer
    for (int cmp = 0; cmp < MAX_NUM_COLOR; cmp++) {
        dpos[cmp] = 0;
    }

    uint16_t block_width = bas_info->axi_width[0];
    int n_mcu = 0;

    printf("INFO: fmt %d, bas_info->mcu_cmp = %d \n", fmt, (int)(bas_info->mcu_cmp));
    printf("INFO: bas_info->hls_mbs[cmp] %d, %d, %d \n", bas_info->hls_mbs[0], bas_info->hls_mbs[1],
           bas_info->hls_mbs[2]);

LOOP_write_yuv_buffer:
    while (n_mcu < (int)(bas_info->hls_mcuc)) {
        for (int cmp = 0; cmp < MAX_NUM_COLOR; cmp++) {              // 0,1,2
            for (int mbs = 0; mbs < bas_info->hls_mbs[cmp]; mbs++) { // 0,1,2,3, 0, 0,

                for (int i = 0; i < 8; i++) { // write one block of Y or U or V
                    for (int j = 0; j < 8; j++) {
                        yuv_row_pointer[(cmp)*bas_info->axi_height[0] * bas_info->axi_width[0] * 64 + (dpos[cmp]) * 8 +
                                        j * bas_info->axi_width[cmp] * 8 + i] = *yuv_mcu_pointer_pix;
                        yuv_mcu_pointer_pix++;
                    }
                } // end block

                if (fmt == xf::codec::C420) { // 420 mbs= 0 1 2 3 0 0

                    if (mbs == 0) {
                        if (cmp != 0 && (dpos[cmp] % bas_info->axi_width[1] == bas_info->axi_width[1] - 1)) {
                            dpos[cmp] += 1 + bas_info->axi_width[1] * (8 - 1);
                        } else {
                            dpos[cmp] += 1;
                        }
                    } else if (mbs == 1) {
                        dpos[cmp] += block_width * 8 - 1;
                    } else if (mbs == 2) {
                        dpos[cmp] += 1;
                    } else {
                        if (dpos[cmp] % (block_width * (8) * 2) == (8 + 1) * block_width - 1) {
                            dpos[cmp] += 1 + block_width * (8 - 1);
                        } else {
                            dpos[cmp] -= block_width * 8 - 1;
                        }
                    }
                } else if (fmt == xf::codec::C422) { // 422 mbs 0 1 0 0
                    if (mbs == 0) {
                        if (cmp != 0 && (dpos[cmp] % bas_info->axi_width[1] == bas_info->axi_width[1] - 1)) {
                            dpos[cmp] += 1 + bas_info->axi_width[1] * (8 - 1);
                        } else {
                            dpos[cmp] += 1;
                        }
                    } else { // cmp=0, mbs=1
                        if (dpos[cmp] % (block_width) == block_width - 1) {
                            dpos[cmp] += 1 + block_width * (8 - 1);
                        } else {
                            dpos[cmp] += 1;
                        }
                    }
                } else {
                    if (dpos[cmp] % block_width == block_width - 1) {
                        dpos[cmp] += 1 + block_width * (8 - 1);
                    } else {
                        dpos[cmp] += 1;
                    }
                }
            }
        } // end one mcu
        n_mcu++;
    }

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            printf("%02X, ", (uint8_t)(yuv_row_pointer[8 * i + j]));
        }
        printf("\n");
    }

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 8; j++) {
            printf("%d, ", (uint8_t)(yuv_row_pointer[8 * i + j]));
        }
        printf("\n");
    }

LOOP_write_y:
    fwrite(yuv_row_pointer, sizeof(char), bas_info->axi_height[0] * bas_info->axi_width[0] * 64, f);
LOOP_write_u:
    fwrite(yuv_row_pointer + bas_info->axi_height[0] * bas_info->axi_width[0] * 64, sizeof(char),
           bas_info->axi_height[1] * bas_info->axi_width[1] * 64, f);
LOOP_write_v:
    fwrite(yuv_row_pointer + bas_info->axi_height[0] * bas_info->axi_width[0] * 128, sizeof(char),
           bas_info->axi_height[2] * bas_info->axi_width[2] * 64, f);

    // fwrite(&end_file, 1, 1, f);//write 0x0a
    fclose(f);

    printf("Please open the YUV file with fmt %d and (width, height) = (%d, %d) \n", fmt, bas_info->axi_width[0] * 8,
           bas_info->axi_height[0] * 8);

    // write yuv info to a file
    fn = file.substr(0, file.find(".")) + ".yuv.h";
    f = fopen(fn.c_str(), "aw");
    std::cout << "WARNING: " << fn << " will be opened for binary write." << std::endl;
    if (!f) {
        std::cerr << "ERROR: " << fn << " cannot be opened for binary write." << std::endl;
    }
    fprintf(f, "INFO: fmt=%d, bas_info->mcu_cmp=%d\n", fmt, (int)(bas_info->mcu_cmp));
    fprintf(f, "INFO: bas_info->hls_mbs[cmp] %d, %d, %d \n", bas_info->hls_mbs[0], bas_info->hls_mbs[1],
            bas_info->hls_mbs[2]);
    fprintf(f, "Please open the YUV file with fmt %d and (width, height) = (%d, %d) \n", fmt,
            bas_info->axi_width[0] * 8, bas_info->axi_height[0] * 8);
    fclose(f);
}

// ------------------------------------------------------------
void rebuild_infos(xf::codec::img_info& img_info,
                   xf::codec::cmp_info cmp_info[MAX_NUM_COLOR],
                   xf::codec::bas_info& bas_info,
                   int& rtn,
                   int& rtn2,
                   ap_uint<32> infos[1024]) {
    img_info.hls_cs_cmpc = *(infos + 0);
    img_info.hls_mcuc = *(infos + 1);
    img_info.hls_mcuh = *(infos + 2);
    img_info.hls_mcuv = *(infos + 3);
    rtn = *(infos + 4);
    rtn2 = *(infos + 5);

    bas_info.all_blocks = *(infos + 10);
    for (int i = 0; i < MAX_NUM_COLOR; i++) {
#pragma HLS PIPELINE II = 1
        bas_info.axi_height[i] = *(infos + 11 + i);
    }
    for (int i = 0; i < 4; i++) {
#pragma HLS PIPELINE II = 1
        bas_info.axi_map_row2cmp[i] = *(infos + 14 + i);
    }
    bas_info.axi_mcuv = *(infos + 18);
    bas_info.axi_num_cmp = *(infos + 19);
    bas_info.axi_num_cmp_mcu = *(infos + 20);
    for (int i = 0; i < MAX_NUM_COLOR; i++) {
#pragma HLS PIPELINE II = 1
        bas_info.axi_width[i] = *(infos + 21 + i);
    }
    int format = *(infos + 24);
    bas_info.format = (xf::codec::COLOR_FORMAT)format;
    for (int i = 0; i < MAX_NUM_COLOR; i++) {
#pragma HLS PIPELINE II = 1
        bas_info.hls_mbs[i] = *(infos + 25 + i);
    }
    bas_info.hls_mcuc = *(infos + 28);
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
#pragma HLS PIPELINE II = 1
                bas_info.idct_q_table_x[c][i][j] = *(infos + 29 + c * 64 + i * 8 + j);
            }
        }
    }
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
#pragma HLS PIPELINE II = 1
                bas_info.idct_q_table_y[c][i][j] = *(infos + 221 + c * 64 + i * 8 + j);
            }
        }
    }
    bas_info.mcu_cmp = *(infos + 413);
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 64; i++) {
#pragma HLS PIPELINE II = 1
            bas_info.min_nois_thld_x[c][i] = *(infos + 414 + c * 64 + i);
        }
    }
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 64; i++) {
#pragma HLS PIPELINE II = 1
            bas_info.min_nois_thld_y[c][i] = *(infos + 606 + c * 64 + i);
        }
    }
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
#pragma HLS PIPELINE II = 1
                bas_info.q_tables[c][i][j] = *(infos + 798 + c * 64 + i * 8 + j);
            }
        }
    }
    for (int c = 0; c < MAX_NUM_COLOR; c++) {
        cmp_info[c].bc = *(infos + 990 + c * 6);
        cmp_info[c].bch = *(infos + 991 + c * 6);
        cmp_info[c].bcv = *(infos + 992 + c * 6);
        cmp_info[c].mbs = *(infos + 993 + c * 6);
        cmp_info[c].sfh = *(infos + 994 + c * 6);
        cmp_info[c].sfv = *(infos + 995 + c * 6);
    }
}

// ------------------------------------------------------------

int main(int argc, const char* argv[]) {
    std::cout << "\n------------ Test for decode image.jpg  -------------\n";
    std::string optValue;
    std::string JPEGFile;
    std::string xclbin_path;

    // cmd arg parser.
    ArgParser parser(argc, argv);

    // Read In paths addresses
    if (!parser.getCmdOption("-xclbin", xclbin_path)) {
        std::cout << "INFO: input path is not set!\n";
    }
    if (parser.getCmdOption("-JPEGFile", optValue)) {
        JPEGFile = optValue;
    } else {
        std::cout << "INFO: JPEG file not specified for this test. use "
                     "'-JPEGFile' to specified it. \n";
    }

    ///// declaration

    // load data to simulate the ddr data
    // size of jpeg_pointer, output of yuv_mcu_pointer, and output image infos
    int size;
    uint8_t* jpeg_pointer_0;
    uint8_t* jpeg_pointer_1;
#ifndef _HLS_TEST_
    ap_uint<64>* yuv_mcu_pointer_0 = aligned_alloc<ap_uint<64> >(sizeof(ap_uint<64>) * MAXCMP_BC * 8);
    ap_uint<32>* infos_0 = aligned_alloc<ap_uint<32> >(sizeof(ap_uint<32>) * 1024);
    ap_uint<64>* yuv_mcu_pointer_1 = aligned_alloc<ap_uint<64> >(sizeof(ap_uint<64>) * MAXCMP_BC * 8);
    ap_uint<32>* infos_1 = aligned_alloc<ap_uint<32> >(sizeof(ap_uint<32>) * 1024);

#else
    ap_uint<64>* yuv_mcu_pointer_0 = (ap_uint<64>*)malloc(sizeof(ap_uint<64>) * MAXCMP_BC * 8);
    ap_uint<32>* infos_0 = (ap_uint<32>*)malloc(sizeof(ap_uint<32>) * 1024);
    ap_uint<64>* yuv_mcu_pointer_1 = (ap_uint<64>*)malloc(sizeof(ap_uint<64>) * MAXCMP_BC * 8);
    ap_uint<32>* infos_1 = (ap_uint<32>*)malloc(sizeof(ap_uint<32>) * 1024);
#endif
    int err_0 = load_dat(jpeg_pointer_0, JPEGFile, size);
    int err_1 = load_dat(jpeg_pointer_1, JPEGFile, size);
    if (err_0) {
        printf("Alloc buf failed!, size:%d Bytes\n", size);
        return err_0;
    }

    if (err_1) {
        printf("Alloc buf failed!, size:%d Bytes\n", size);
        return err_1;
    }

    // Variables to measure time
    struct timeval startE2E_0, endE2E_0;
    struct timeval startE2E_1, endE2E_1;
    gettimeofday(&startE2E_0, 0);
    gettimeofday(&startE2E_1, 0);

    // To test SYNTHESIS top
    hls::stream<ap_uint<24> > block_strm_0;
    xf::codec::cmp_info cmp_info_0[MAX_NUM_COLOR];
    xf::codec::img_info img_info_0;
    xf::codec::bas_info bas_info_0;
    img_info_0.hls_cs_cmpc = 0; // init

    hls::stream<ap_uint<24> > block_strm_1;
    xf::codec::cmp_info cmp_info_1[MAX_NUM_COLOR];
    xf::codec::img_info img_info_1;
    xf::codec::bas_info bas_info_1;
    img_info_1.hls_cs_cmpc = 0; // init


    // 0: decode jfif successful
    // 1: marker in jfif is not in expectation
    int rtn_0 = 0;
    int rtn_1 = 0;

    // 0: decode huffman successful
    // 1: huffman data is not in expectation
    int rtn2_0 = false;
    int rtn2_1 = false;

#ifdef _HLS_TEST_
    uint32_t hls_mcuc_0;
    uint16_t hls_mcuh_0;
    uint16_t hls_mcuv_0;
    uint8_t hls_cs_cmpc_0;
    hls::stream<ap_uint<5> > idx_coef_0;
    hls::stream<xf::codec::idct_out_t> strm_iDCT_x8_0[8];
    
    uint32_t hls_mcuc_1;
    uint16_t hls_mcuh_1;
    uint16_t hls_mcuv_1;
    uint8_t hls_cs_cmpc_1;
    hls::stream<ap_uint<5> > idx_coef_1;
    hls::stream<xf::codec::idct_out_t> strm_iDCT_x8_1[8];
    

    // L2 top
    kernelJpegDecoder((ap_uint<CH_W>*)jpeg_pointer, (int)size,
                      //&img_info, cmp_info, &bas_info,
                      yuv_mcu_pointer, infos);
    // strm_iDCT_x8);//idx_coef,

    rebuild_infos(img_info_0, cmp_info_0, bas_info_0, rtn_0, rtn2_0, infos_0);
    rebuild_infos(img_info_1, cmp_info_1, bas_info_1, rtn_1, rtn2_1, infos_1);

    // one shoot test for the IDCT
    printf("INFO: bas_info_0.q_tables are : \n");
    for (int id = 0; id < 2; id++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                printf("%d, ", (int)(bas_info_0.q_tables[id][i][j]));
            }
            printf("\n");
        }
    }

    printf("INFO: bas_info_1.q_tables are : \n");
    for (int id = 0; id < 2; id++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                printf("%d, ", (int)(bas_info_1.q_tables[id][i][j]));
            }
            printf("\n");
        }
    }
#else
    xf::common::utils_sw::Logger logger(std::cout, std::cerr);
    // Get CL devices.
    cl_int fail;

    // Platform related operations
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    // Creating Context and Command Queue for selected Device
    cl::Context context(device, NULL, NULL, NULL, &fail);
    logger.logCreateContext(fail);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &fail);
    logger.logCreateCommandQueue(fail);
    std::string devName = device.getInfo<CL_DEVICE_NAME>();
    printf("INFO: Found Device=%s\n", devName.c_str());

    cl::Program::Binaries xclBins = xcl::import_binary_file(xclbin_path);
    devices.resize(1);
    cl::Program program(context, devices, xclBins, NULL, &fail);
    logger.logCreateProgram(fail);

    cl::Kernel kernel_jpegDecoder_0(program, "kernelJpegDecoder:{kernelJpegDecoder_1}");
    cl::Kernel kernel_jpegDecoder_1(program, "kernelJpegDecoder:{kernelJpegDecoder_2}");
    std::cout << "INFO: Kernel has been created" << std::endl;
    logger.logCreateKernel(fail);
    std::cout << "INFO: Kernel has been created" << std::endl;

#ifndef USE_HBM
    // DDR Settings
    std::vector<cl_mem_ext_ptr_t> mext_in_0(3);
    std::vector<cl_mem_ext_ptr_t> mext_in_1(3);
    
    mext_in_0[0].flags = XCL_MEM_DDR_BANK0;
    mext_in_0[0].obj = jpeg_pointer_0;
    mext_in_0[0].param = 0;
    mext_in_0[1].flags = XCL_MEM_DDR_BANK1;
    mext_in_0[1].obj = yuv_mcu_pointer_0;
    mext_in_0[1].param = 0;
    mext_in_0[2].flags = XCL_MEM_DDR_BANK1;
    mext_in_0[2].obj = infos_0;
    mext_in_0[2].param = 0;


    mext_in_1[0].flags = XCL_MEM_DDR_BANK3;
    mext_in_1[0].obj = jpeg_pointer_1;
    mext_in_1[0].param = 0;
    mext_in_1[1].flags = XCL_MEM_DDR_BANK4;
    mext_in_1[1].obj = yuv_mcu_pointer_1;
    mext_in_1[1].param = 0;
    mext_in_1[2].flags = XCL_MEM_DDR_BANK5;
    mext_in_1[2].obj = infos_1;
    mext_in_1[2].param = 0;
#else
    std::vector<cl_mem_ext_ptr_t> mext_in_0(3);
    std::vector<cl_mem_ext_ptr_t> mext_in_1(3);
    
    mext_in_0[0].flags = XCL_BANK0;
    mext_in_0[0].obj = jpeg_pointer_0;
    mext_in_0[0].param = 0;
    mext_in_0[1].flags = XCL_BANK1;
    mext_in_0[1].obj = yuv_mcu_pointer_0;
    mext_in_0[1].param = 0;
    mext_in_0[2].flags = XCL_BANK1;
    mext_in_0[2].obj = infos_0;
    mext_in_0[2].param = 0;


    mext_in_1[0].flags = XCL_BANK3;
    mext_in_1[0].obj = jpeg_pointer_1;
    mext_in_1[0].param = 0;
    mext_in_1[1].flags = XCL_BANK4;
    mext_in_1[1].obj = yuv_mcu_pointer_1;
    mext_in_1[1].param = 0;
    mext_in_1[2].flags = XCL_BANK5;
    mext_in_1[2].obj = infos_1;
    mext_in_1[2].param = 0;

#endif

    // Create device buffer and map dev buf to host buf
    std::vector<cl::Buffer> buffer_0(3);
    
    buffer_0[0] = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE,
                           sizeof(uint8_t) * (size), &mext_in_0[0]);
    buffer_0[1] = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                           sizeof(ap_uint<64>) * MAXCMP_BC * 8, &mext_in_0[1]);
    buffer_0[2] = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                           sizeof(ap_uint<32>) * 1024, &mext_in_0[2]);


    std::vector<cl::Buffer> buffer_1(3);

    buffer_1[0] = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE,
                           sizeof(uint8_t) * (size), &mext_in_1[0]);
    buffer_1[1] = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                           sizeof(ap_uint<64>) * MAXCMP_BC * 8, &mext_in_1[1]);
    buffer_1[2] = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY,
                           sizeof(ap_uint<32>) * 1024, &mext_in_1[2]);

    // add buffers to migrate
    std::vector<cl::Memory> init_0;
    for (int i = 0; i < 3; i++) {
        init_0.push_back(buffer_0[i]);
    }

    std::vector<cl::Memory> init_1;
    for (int i = 0; i < 3; i++) {
        init_1.push_back(buffer_1[i]);
    }

    // migrate data from host to device
    q.enqueueMigrateMemObjects(init_0, CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED, nullptr, nullptr);
    q.enqueueMigrateMemObjects(init_1, CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED, nullptr, nullptr);
    q.finish();

    // Data transfer from host buffer to device buffer
    std::vector<cl::Memory> ob_in_0;
    std::vector<cl::Memory> ob_out_0;
    ob_in_0.push_back(buffer_0[0]);
    ob_out_0.push_back(buffer_0[1]);
    ob_out_0.push_back(buffer_0[2]);

    kernel_jpegDecoder_0.setArg(0, buffer_0[0]);
    kernel_jpegDecoder_0.setArg(1, size);
    kernel_jpegDecoder_0.setArg(2, buffer_0[1]);
    kernel_jpegDecoder_0.setArg(3, buffer_0[2]);
    //    kernel_jpegDecoder_0.setArg(4, rtn);
    //    kernel_jpegDecoder_0.setArg(5, rtn2);

    std::vector<cl::Memory> ob_in_1;
    std::vector<cl::Memory> ob_out_1;
    ob_in_1.push_back(buffer_1[0]);
    ob_out_1.push_back(buffer_1[1]);
    ob_out_1.push_back(buffer_1[2]);

    kernel_jpegDecoder_1.setArg(0, buffer_1[0]);
    kernel_jpegDecoder_1.setArg(1, size);
    kernel_jpegDecoder_1.setArg(2, buffer_1[1]);
    kernel_jpegDecoder_1.setArg(3, buffer_1[2]);
    //    kernel_jpegDecoder_1.setArg(4, rtn);
    //    kernel_jpegDecoder_1.setArg(5, rtn2);

    

    // Setup kernel
    std::cout << "INFO: Finish kernel setup" << std::endl;
    const int num_runs = 10;
    std::vector<std::vector<cl::Event> > events_write_0(num_runs);
    std::vector<std::vector<cl::Event> > events_kernel_0(num_runs);
    std::vector<std::vector<cl::Event> > events_read_0(num_runs);

    std::vector<std::vector<cl::Event> > events_write_1(num_runs);
    std::vector<std::vector<cl::Event> > events_kernel_1(num_runs);
    std::vector<std::vector<cl::Event> > events_read_1(num_runs);
    

    for (int i = 0; i < num_runs; ++i) {
        events_kernel_0[i].resize(1);
        events_write_0[i].resize(1);
        events_read_0[i].resize(1);

        events_kernel_1[i].resize(1);
        events_write_1[i].resize(1);
        events_read_1[i].resize(1);
    }

    for (int i = 0; i < num_runs; ++i) {
        if (i < 1) {
            q.enqueueMigrateMemObjects(ob_in_0, 0, nullptr, &events_write_0[i][0]); // 0 : migrate from host to dev
            q.enqueueMigrateMemObjects(ob_in_1, 0, nullptr, &events_write_1[i][0]); // 0 : migrate from host to dev
        } else {
            q.enqueueMigrateMemObjects(ob_in_0, 0, &events_read_0[i - 1],
                                       &events_write_0[i][0]); // 0 : migrate from host to dev
            q.enqueueMigrateMemObjects(ob_in_1, 0, &events_read_1[i - 1],
                                       &events_write_1[i][0]);
        }
        // Launch kernel and compute kernel execution time
        q.enqueueTask(kernel_jpegDecoder_0, &events_write_0[i], &events_kernel_0[i][0]);
        q.enqueueTask(kernel_jpegDecoder_1, &events_write_1[i], &events_kernel_1[i][0]);

        // Data transfer from device buffer to host buffer
        q.enqueueMigrateMemObjects(ob_out_0, 1, &events_kernel_0[i], &events_read_0[i][0]); // 1 : migrate from dev to host
        q.enqueueMigrateMemObjects(ob_out_1, 1, &events_kernel_1[i], &events_read_1[i][0]);
    }
    q.finish();

    gettimeofday(&endE2E_0, 0);
    gettimeofday(&endE2E_1, 0);
    std::cout << "INFO: Finish kernel execution" << std::endl;
    std::cout << "INFO: Finish E2E execution" << std::endl;

    // print related times
    unsigned long timeStart_0, timeEnd_0, exec_time0_0, write_time_0, read_time_0;
    std::cout << "-------------------------------------------------------" << std::endl;
    events_write_0[0][0].getProfilingInfo(CL_PROFILING_COMMAND_START, &timeStart_0);
    events_write[0][0].getProfilingInfo(CL_PROFILING_COMMAND_END, &timeEnd_0);
    write_time_0 = (timeEnd_0 - timeStart_0) / 1000.0;
    std::cout << "INFO: Data transfer from host to device: " << write_time_0 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;
    events_read_0[0][0].getProfilingInfo(CL_PROFILING_COMMAND_START, &timeStart_0);
    events_read_0[0][0].getProfilingInfo(CL_PROFILING_COMMAND_END, &timeEnd_0);
    read_time_0 = (timeEnd_0 - timeStart_0) / 1000.0;
    std::cout << "INFO: Data transfer from device to host: " << read_time_0 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;

    exec_time0_0 = 0;
    for (int i = 0; i < num_runs; ++i) {
        events_kernel_0[i][0].getProfilingInfo(CL_PROFILING_COMMAND_START, &timeStart_0);
        events_kernel_0[i][0].getProfilingInfo(CL_PROFILING_COMMAND_END, &timeEnd_0);
        exec_time0_0 += (timeEnd_0 - timeStart_0) / 1000.0;
        unsigned long t = (timeEnd_0 - timeStart_0) / 1000.0;
        printf("INFO: kernel %d: execution time %lu usec\n", i, t);
    }
    exec_time0_0 = exec_time0_0 / (unsigned long)num_runs;
    std::cout << "INFO: Average kernel execution per run: " << exec_time0_0 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;
    unsigned long exec_timeE2E_0 = diff(&endE2E_0, &startE2E_0);
    std::cout << "INFO: Average E2E per run: " << exec_timeE2E_0 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;

    rebuild_infos(img_info_0, cmp_info_0, bas_info_0, rtn_0, rtn2_0, infos_0);
#endif
    // for image info
    int hls_bc_0[MAX_NUM_COLOR];
    for (int i = 0; i < MAX_NUM_COLOR; i++) {
        hls_bc_0[i] = cmp_info_0[i].bc;
    }

    // todo merge to syn-code

    if (rtn_0 || rtn2_0) {
        printf("Warning: Decoding the bad case input file!\n");
        if (rtn_0 == 1) {
            printf("Warning: [code 1] marker in jfif is not in expectation!\n");
        } else if (rtn_0 == 2) {
            printf("ERROR: [code 2] huffman table is not in expectation!\n");
        } else {
            if (rtn2_0) {
                printf("Warning: [code 3] huffman data is not in expectation!\n");
            }
        }
        return 1;
#ifndef _HLS_TEST_
        logger.error(xf::common::utils_sw::Logger::Message::TEST_FAIL);
    } else {
        logger.info(xf::common::utils_sw::Logger::Message::TEST_PASS);
#endif
    }

    printf("INFO: writing the YUV file!\n");
    rebuild_raw_yuv(JPEGFile, &bas_info_0, hls_bc_0, yuv_mcu_pointer_0);

    
// print related times
    unsigned long timeStart_1, timeEnd_1, exec_time0_1, write_time_1, read_time_1;
    std::cout << "-------------------------------------------------------" << std::endl;
    events_write_1[0][0].getProfilingInfo(CL_PROFILING_COMMAND_START, &timeStart_1);
    events_write[0][0].getProfilingInfo(CL_PROFILING_COMMAND_END, &timeEnd_1);
    write_time_1 = (timeEnd_1 - timeStart_1) / 1000.0;
    std::cout << "INFO: Data transfer from host to device: " << write_time_1 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;
    events_read_1[0][0].getProfilingInfo(CL_PROFILING_COMMAND_START, &timeStart_1);
    events_read_1[0][0].getProfilingInfo(CL_PROFILING_COMMAND_END, &timeEnd_1);
    read_time_1 = (timeEnd_1 - timeStart_1) / 1000.0;
    std::cout << "INFO: Data transfer from device to host: " << read_time_1 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;

    exec_time0_1 = 0;
    for (int i = 0; i < num_runs; ++i) {
        events_kernel_1[i][0].getProfilingInfo(CL_PROFILING_COMMAND_START, &timeStart_1);
        events_kernel_1[i][0].getProfilingInfo(CL_PROFILING_COMMAND_END, &timeEnd_1);
        exec_time0_1 += (timeEnd_1 - timeStart_1) / 1000.0;
        unsigned long t = (timeEnd_1 - timeStart_1) / 1000.0;
        printf("INFO: kernel %d: execution time %lu usec\n", i, t);
    }
    exec_time0_1 = exec_time0_1 / (unsigned long)num_runs;
    std::cout << "INFO: Average kernel execution per run: " << exec_time0_1 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;
    unsigned long exec_timeE2E_1 = diff(&endE2E_1, &startE2E_1);
    std::cout << "INFO: Average E2E per run: " << exec_timeE2E_1 << " us\n";
    std::cout << "-------------------------------------------------------" << std::endl;

    rebuild_infos(img_info_1, cmp_info_1, bas_info_1, rtn_1, rtn2_1, infos_1);
#endif
    // for image info
    int hls_bc_1[MAX_NUM_COLOR];
    for (int i = 0; i < MAX_NUM_COLOR; i++) {
        hls_bc_1[i] = cmp_info_1[i].bc;
    }

    // todo merge to syn-code

    if (rtn_1 || rtn2_1) {
        printf("Warning: Decoding the bad case input file!\n");
        if (rtn_1 == 1) {
            printf("Warning: [code 1] marker in jfif is not in expectation!\n");
        } else if (rtn_1 == 2) {
            printf("ERROR: [code 2] huffman table is not in expectation!\n");
        } else {
            if (rtn2_1) {
                printf("Warning: [code 3] huffman data is not in expectation!\n");
            }
        }
        return 1;
#ifndef _HLS_TEST_
        logger.error(xf::common::utils_sw::Logger::Message::TEST_FAIL);
    } else {
        logger.info(xf::common::utils_sw::Logger::Message::TEST_PASS);
#endif
    }

    printf("INFO: writing the YUV file!\n");

    rebuild_raw_yuv(JPEGFile, &bas_info_1, hls_bc_1, yuv_mcu_pointer_1);

    free(jpeg_pointer_0);
    free(hls_block_0);
    free(infos_0);
    free(yuv_row_pointer_0);

    

    free(jpeg_pointer_1);
    free(hls_block_1);
    free(infos_1);
    free(yuv_row_pointer_1);

    std::cout << "Ready for next image!\n ";

    return 0;
}
#endif

// ************************************************************
