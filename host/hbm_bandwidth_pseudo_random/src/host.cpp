/**
* Copyright (C) 2020 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/

/********************************************************************************************
Description:
    This is a HBM bandwidth example using a pseudo random 1024 bit data access
pattern to mimic Ethereum Ethash workloads.
    The design contains 3 compute units of a kernel, reading 1024 bits from a
pseudo random address in each of 2 pseudo channels and writing the results of
a simple mathematical operation to a pseudo random address in 2 other pseudo
channels.
    To maximize bandwidth the pseudo channels are used in  P2P like
configuration.
    The host application allocates buffers in 12  HBM banks and runs the compute
units concurrently to measure the overall bandwidth between kernel and HBM
Memory.
 ******************************************************************************************/

#include <algorithm>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "xcl2.hpp"

#define NUM_KERNEL 3

// HBM Banks requirements
#define MAX_HBM_BANKCOUNT 32
#define BANK_NAME(n) n | XCL_MEM_TOPOLOGY
const int bank[MAX_HBM_BANKCOUNT] = {
    BANK_NAME(0),  BANK_NAME(1),  BANK_NAME(2),  BANK_NAME(3),  BANK_NAME(4),  BANK_NAME(5),  BANK_NAME(6),
    BANK_NAME(7),  BANK_NAME(8),  BANK_NAME(9),  BANK_NAME(10), BANK_NAME(11), BANK_NAME(12), BANK_NAME(13),
    BANK_NAME(14), BANK_NAME(15), BANK_NAME(16), BANK_NAME(17), BANK_NAME(18), BANK_NAME(19), BANK_NAME(20),
    BANK_NAME(21), BANK_NAME(22), BANK_NAME(23), BANK_NAME(24), BANK_NAME(25), BANK_NAME(26), BANK_NAME(27),
    BANK_NAME(28), BANK_NAME(29), BANK_NAME(30), BANK_NAME(31)};

// Function for verifying results
bool verify(std::vector<uint32_t, aligned_allocator<uint32_t> >& source_sw_add_results,
            std::vector<uint32_t, aligned_allocator<uint32_t> >& source_sw_mul_results,
            std::vector<uint32_t, aligned_allocator<uint32_t> >& source_hw_add_results,
            std::vector<uint32_t, aligned_allocator<uint32_t> >& source_hw_mul_results,
            unsigned int size) {
    bool check = true;
    for (size_t i = 0; i < size; i++) {
        if (source_hw_add_results[i] != source_sw_add_results[i]) {
            std::cout << "Error: Result mismatch in Addition Operation" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_add_results[i]
                      << " Device result = " << source_hw_add_results[i] << std::endl;
            check = false;
            break;
        }
        if (source_hw_mul_results[i] != source_sw_mul_results[i]) {
            std::cout << "Error: Result mismatch in Multiplication Operation" << std::endl;
            std::cout << "i = " << i << " CPU result = " << source_sw_mul_results[i]
                      << " Device result = " << source_hw_mul_results[i] << std::endl;
            check = false;
            break;
        }
    }
    return check;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << "<XCLBIN> \n";
        return -1;
    }

    unsigned int dataSize = 64 * 1024 * 1024; // taking maximum possible data size value for an HBM bank
    unsigned int num_times = 1024;            // num_times specify, number of times a kernel
                                              // will execute the same operation. This is
                                              // needed
    // to keep the kernel busy to test the actual bandwidth of all banks running
    // concurrently.

    // reducing the test data capacity to run faster in emulation mode
    if (xcl::is_emulation()) {
        dataSize = 1024;
        num_times = 64;
    }

    std::string binaryFile = argv[1];
    cl_int err;
    cl::CommandQueue q;
    std::string krnl_name = "krnl_vaddmul";
    std::vector<cl::Kernel> krnls(NUM_KERNEL);
    cl::Context context;
    std::vector<uint32_t, aligned_allocator<uint32_t> > source_in1(dataSize);
    std::vector<uint32_t, aligned_allocator<uint32_t> > source_in2(dataSize);
    std::vector<uint32_t, aligned_allocator<uint32_t> > source_sw_add_results(dataSize);
    std::vector<uint32_t, aligned_allocator<uint32_t> > source_sw_mul_results(dataSize);

    std::vector<uint32_t, aligned_allocator<uint32_t> > source_hw_add_results[NUM_KERNEL];
    std::vector<uint32_t, aligned_allocator<uint32_t> > source_hw_mul_results[NUM_KERNEL];

    for (int i = 0; i < NUM_KERNEL; i++) {
        source_hw_add_results[i].resize(dataSize);
        source_hw_mul_results[i].resize(dataSize);
    }

    // Create the test data
    std::generate(source_in1.begin(), source_in1.end(), std::rand);
    std::generate(source_in2.begin(), source_in2.end(), std::rand);
    for (size_t i = 0; i < dataSize; i++) {
        source_sw_add_results[i] = source_in1[i] + source_in2[i];
        source_sw_mul_results[i] = source_in1[i] * source_in2[i];
    }

    // OPENCL HOST CODE AREA START
    // The get_xil_devices will return vector of Xilinx Devices
    auto devices = xcl::get_xil_devices();

    // read_binary_file() command will find the OpenCL binary file created using
    // the
    // V++ compiler load into OpenCL Binary and return pointer to file buffer.
    auto fileBuf = xcl::read_binary_file(binaryFile);

    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    bool valid_device = false;
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, context = cl::Context(device, NULL, NULL, NULL, &err));
        OCL_CHECK(err, q = cl::CommandQueue(context, device,
                                            CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE, &err));

        std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
        cl::Program program(context, {device}, bins, NULL, &err);
        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
        } else {
            std::cout << "Device[" << i << "]: program successful!\n";
            // Creating Kernel object using Compute unit names

            for (int i = 0; i < NUM_KERNEL; i++) {
                std::string cu_id = std::to_string(i + 1);
                std::string krnl_name_full = krnl_name + ":{" + "krnl_vaddmul_" + cu_id + "}";

                std::cout << "Creating a kernel [" << krnl_name_full.c_str() << "] for CU(" << i + 1 << ")\n";

                // Here Kernel object is created by specifying kernel name along with
                // compute unit.
                // For such case, this kernel object can only access the specific
                // Compute unit

                OCL_CHECK(err, krnls[i] = cl::Kernel(program, krnl_name_full.c_str(), &err));
            }
            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (valid_device == false) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }

    std::vector<cl_mem_ext_ptr_t> inBufExt1(NUM_KERNEL);
    std::vector<cl_mem_ext_ptr_t> inBufExt2(NUM_KERNEL);
    std::vector<cl_mem_ext_ptr_t> outAddBufExt(NUM_KERNEL);
    std::vector<cl_mem_ext_ptr_t> outMulBufExt(NUM_KERNEL);

    std::vector<cl::Buffer> buffer_input1(NUM_KERNEL);
    std::vector<cl::Buffer> buffer_input2(NUM_KERNEL);
    std::vector<cl::Buffer> buffer_output_add(NUM_KERNEL);
    std::vector<cl::Buffer> buffer_output_mul(NUM_KERNEL);

    // For Allocating Buffer to specific Global Memory Bank, user has to use
    // cl_mem_ext_ptr_t
    // and provide the Banks
    for (int i = 0; i < NUM_KERNEL; i++) {
        inBufExt1[i].obj = source_in1.data();
        inBufExt1[i].param = 0;
        inBufExt1[i].flags = bank[i * 4];

        inBufExt2[i].obj = source_in2.data();
        inBufExt2[i].param = 0;
        inBufExt2[i].flags = bank[(i * 4) + 1];

        outAddBufExt[i].obj = source_hw_add_results[i].data();
        outAddBufExt[i].param = 0;
        outAddBufExt[i].flags = bank[(i * 4) + 2];

        outMulBufExt[i].obj = source_hw_mul_results[i].data();
        outMulBufExt[i].param = 0;
        outMulBufExt[i].flags = bank[(i * 4) + 3];
    }

    // These commands will allocate memory on the FPGA. The cl::Buffer objects can
    // be used to reference the memory locations on the device.
    // Creating Buffers
    for (int i = 0; i < NUM_KERNEL; i++) {
        OCL_CHECK(err,
                  buffer_input1[i] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                                                sizeof(uint32_t) * dataSize, &inBufExt1[i], &err));
        OCL_CHECK(err,
                  buffer_input2[i] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                                                sizeof(uint32_t) * dataSize, &inBufExt2[i], &err));
        OCL_CHECK(err, buffer_output_add[i] =
                           cl::Buffer(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                                      sizeof(uint32_t) * dataSize, &outAddBufExt[i], &err));
        OCL_CHECK(err, buffer_output_mul[i] =
                           cl::Buffer(context, CL_MEM_WRITE_ONLY | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR,
                                      sizeof(uint32_t) * dataSize, &outMulBufExt[i], &err));
    }

    // Copy input data to Device Global Memory
    for (int i = 0; i < NUM_KERNEL; i++) {
        OCL_CHECK(err,
                  err = q.enqueueMigrateMemObjects({buffer_input1[i], buffer_input2[i]}, 0 /* 0 means from host*/));
    }
    q.finish();

    double kernel_time_in_sec = 0, result = 0;

    std::chrono::duration<double> kernel_time(0);

    auto kernel_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_KERNEL; i++) {
        // Setting the k_vadd Arguments
        OCL_CHECK(err, err = krnls[i].setArg(0, buffer_input1[i]));
        OCL_CHECK(err, err = krnls[i].setArg(1, buffer_input2[i]));
        OCL_CHECK(err, err = krnls[i].setArg(2, buffer_output_add[i]));
        OCL_CHECK(err, err = krnls[i].setArg(3, buffer_output_mul[i]));
        OCL_CHECK(err, err = krnls[i].setArg(4, dataSize));
        OCL_CHECK(err, err = krnls[i].setArg(5, num_times));

        // Invoking the kernel
        OCL_CHECK(err, err = q.enqueueTask(krnls[i]));
    }
    q.finish();
    auto kernel_end = std::chrono::high_resolution_clock::now();

    kernel_time = std::chrono::duration<double>(kernel_end - kernel_start);

    kernel_time_in_sec = kernel_time.count();
    kernel_time_in_sec /= NUM_KERNEL;

    // Copy Result from Device Global Memory to Host Local Memory
    for (int i = 0; i < NUM_KERNEL; i++) {
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_output_add[i], buffer_output_mul[i]},
                                                        CL_MIGRATE_MEM_OBJECT_HOST));
    }
    q.finish();

    bool match = true;

    for (int i = 0; i < NUM_KERNEL; i++) {
        match = verify(source_sw_add_results, source_sw_mul_results, source_hw_add_results[i], source_hw_mul_results[i],
                       dataSize);
        if (!match) {
            std::cerr << "TEST FAILED" << std::endl;
            return EXIT_FAILURE;
        }
    }

    // Multiplying the actual data size by 4 because four buffers are being
    // used.
    result = 4 * (float)dataSize * num_times * sizeof(uint32_t);
    result /= 1000;               // to KB
    result /= 1000;               // to MB
    result /= 1000;               // to GB
    result /= kernel_time_in_sec; // to GBps

    std::cout << "OVERALL THROUGHPUT = " << result << " GB/s" << std::endl;
    std::cout << "CHANNEL THROUGHPUT = " << result / (NUM_KERNEL * 4) << " GB/s" << std::endl;

    std::cout << "TEST PASSED" << std::endl;
    return EXIT_SUCCESS;
}
