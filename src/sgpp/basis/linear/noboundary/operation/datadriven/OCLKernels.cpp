/******************************************************************************
* Copyright (C) 2010 Technische Universitaet Muenchen                         *
* This file is part of the SG++ project. For conditions of distribution and   *
* use, please see the copyright notice at http://www5.in.tum.de/SGpp          *
******************************************************************************/
// @author Alexander Heinecke (Alexander.Heinecke@mytum.de)

#include "basis/linear/noboundary/operation/datadriven/OCLKernels.hpp"

#include <cmath>
#include <algorithm>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>

// include OpenCL
#include "CL/cl.h"

namespace sg
{

namespace parallel
{

cl_int err;
cl_platform_id platform_id;
cl_platform_id* platform_ids;
cl_device_id* device_ids;
cl_uint num_platforms;
cl_uint num_devices;
cl_context context;
cl_command_queue command_queue[MAX_OCL_DEVICE_COUNT];


cl_mem clDataDP[MAX_OCL_DEVICE_COUNT], clLevelDP[MAX_OCL_DEVICE_COUNT], clIndexDP[MAX_OCL_DEVICE_COUNT];

cl_kernel kernel_multTransDP[MAX_OCL_DEVICE_COUNT];
cl_program program_multTransDP;

cl_kernel kernel_multDP[MAX_OCL_DEVICE_COUNT];
cl_program program_multDP;

cl_kernel kernel_multTransModDP[MAX_OCL_DEVICE_COUNT];
cl_program program_multTransModDP;

cl_kernel kernel_multModDP[MAX_OCL_DEVICE_COUNT];
cl_program program_multModDP;


cl_mem clDataSP[MAX_OCL_DEVICE_COUNT], clLevelSP[MAX_OCL_DEVICE_COUNT], clIndexSP[MAX_OCL_DEVICE_COUNT];

cl_kernel kernel_multTransSP[MAX_OCL_DEVICE_COUNT];
cl_program program_multTransSP;

cl_kernel kernel_multSP[MAX_OCL_DEVICE_COUNT];
cl_program program_multSP;

cl_kernel kernel_multTransModSP[MAX_OCL_DEVICE_COUNT];
cl_program program_multTransModSP;

cl_kernel kernel_multModSP[MAX_OCL_DEVICE_COUNT];
cl_program program_multModSP;

OCLKernels::OCLKernels()
{
	// determine number of available OpenCL platforms
	err = clGetPlatformIDs(0, NULL, &num_platforms);
	if (err != CL_SUCCESS)
	{
		std::cout << "OCL Error: Unable to get number of OpenCL platforms. Error Code: " << err << std::endl;
	}
	std::cout << "OCL Info: " << num_platforms << " OpenCL Platforms have been found" << std::endl;

	// get available platforms
	platform_ids = new cl_platform_id[num_platforms];
	err = clGetPlatformIDs(num_platforms, platform_ids, NULL);
	if (err != CL_SUCCESS)
	{
		std::cout << "OCL Error: Unable to get Platform ID. Error Code: " << err << std::endl;
	}

	for (cl_uint ui = 0; ui < num_platforms; ui++)
	{
		char vendor_name[128] = {0};
		err = clGetPlatformInfo(platform_ids[ui], CL_PLATFORM_VENDOR, 128 * sizeof(char), vendor_name, NULL);
		if (CL_SUCCESS != err)
		{
			std::cout << "OCL Error: Can't get platform vendor!" << std::endl;
		}
		else
		{
			if (vendor_name != NULL)
			{
				std::cout << "OCL Info: Platform " << ui << " vendor name: " << vendor_name << std::endl;
			}
#ifdef USEOCL_CPU
			if (strcmp(vendor_name, "Intel(R) Corporation") == 0)
			{
				std::cout << "OCL Info: Using CPU Platform: " << vendor_name << std::endl;
				platform_id = platform_ids[ui];
			}
#else
			if (strcmp(vendor_name, "Intel(R) Corporation") != 0)
			{
				std::cout << "OCL Info: Using GPU Platform: " << vendor_name << std::endl;
				platform_id = platform_ids[ui];
			}
#endif
		}
	}
	std::cout << std::endl;

	// Find out how many devices there are
#ifdef USEOCL_CPU
	device_ids = new cl_device_id[MAX_OCL_DEVICE_COUNT];
	err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, MAX_OCL_DEVICE_COUNT, device_ids, &num_devices);
	if (err != CL_SUCCESS)
    {
    	std::cout << "OCL Error: Unable to get Device ID. Error Code: " << err << std::endl;
    }
	// set max number of devices
	if (num_devices > MAX_OCL_DEVICE_COUNT)
	{
		num_devices = MAX_OCL_DEVICE_COUNT;
	}
#else
	err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, MAX_OCL_DEVICE_COUNT, NULL, &num_devices);
	if (num_devices == 0)
    {
    	std::cout << "OCL Error: NO GPU OpenCL devices have been found!" << std::endl;
    }
	// set max number of devices
	if (num_devices > MAX_OCL_DEVICE_COUNT)
	{
		num_devices = MAX_OCL_DEVICE_COUNT;
	}
	device_ids = new cl_device_id[num_devices];
	err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, num_devices, device_ids, NULL);
	if (err != CL_SUCCESS)
    {
    	std::cout << "OCL Error: Unable to get Device ID. Error Code: " << err << std::endl;
    }
#endif
	std::cout << "OCL Info: " << num_devices << " OpenCL devices have been found!" << std::endl;

    // Create OpenCL context
    context = clCreateContext(0, num_devices, device_ids, NULL, NULL, &err);
    if (err != CL_SUCCESS)
	{
    	std::cout << "OCL Error: Failed to create OpenCL context! Error Code: " << err << std::endl;
	}

    // Creating the command queues
    for (size_t i = 0; i < num_devices; i++)
    {
#ifdef USEOCL_CPU
    	command_queue[i] = clCreateCommandQueue(context, device_ids[i], CL_QUEUE_PROFILING_ENABLE , &err);
#else
    	command_queue[i] = clCreateCommandQueue(context, device_ids[i], CL_QUEUE_PROFILING_ENABLE, &err);
#endif
    	if (err != CL_SUCCESS)
    	{
    		std::cout << "OCL Error: Failed to create command queue! Error Code: " << err << std::endl;
    	}
    }

    std::cout << std::endl;

    isFirstTimeMultTransSP = true;
    isFirstTimeMultSP = true;
    isFirstTimeMultTransModSP = true;
	isFirstTimeMultModSP = true;
    isVeryFirstTimeSP = true;

    isFirstTimeMultTransDP = true;
    isFirstTimeMultDP = true;
    isFirstTimeMultTransModDP = true;
	isFirstTimeMultModDP = true;
    isVeryFirstTimeDP = true;
}

OCLKernels::~OCLKernels()
{
	if (!isFirstTimeMultSP)
	{
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseKernel(kernel_multSP[i]);
	    }
	    clReleaseProgram(program_multSP);
	}

	if (!isFirstTimeMultDP)
	{
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseKernel(kernel_multDP[i]);
	    }
	    clReleaseProgram(program_multDP);
	}

	if (!isFirstTimeMultModSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clReleaseKernel(kernel_multModSP[i]);
		}
		clReleaseProgram(program_multModSP);
	}

	if (!isFirstTimeMultModDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clReleaseKernel(kernel_multModDP[i]);
		}
		clReleaseProgram(program_multModDP);
	}

	if (!isFirstTimeMultTransSP)
	{
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clLevelSP[i]);
	    	clReleaseMemObject(clIndexSP[i]);
	    	clReleaseKernel(kernel_multTransSP[i]);
	    }
	    clReleaseProgram(program_multTransSP);
	}

	if (!isFirstTimeMultTransDP)
	{
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clLevelDP[i]);
	    	clReleaseMemObject(clIndexDP[i]);
	    	clReleaseKernel(kernel_multTransDP[i]);
	    }
	    clReleaseProgram(program_multTransDP);
	}

	if (!isFirstTimeMultTransModSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clReleaseMemObject(clLevelSP[i]);
			clReleaseMemObject(clIndexSP[i]);
			clReleaseKernel(kernel_multTransModSP[i]);
		}
		clReleaseProgram(program_multTransModSP);
	}

	if (!isFirstTimeMultTransModDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clReleaseMemObject(clLevelDP[i]);
			clReleaseMemObject(clIndexDP[i]);
			clReleaseKernel(kernel_multTransModDP[i]);
		}
		clReleaseProgram(program_multTransModDP);
	}

	if (!isVeryFirstTimeSP)
	{
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clDataSP[i]);
	    }
	}

	if (!isVeryFirstTimeDP)
	{
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clDataDP[i]);
	    }
	}

    for (size_t i = 0; i < num_devices; i++)
    {
    	clReleaseCommandQueue(command_queue[i]);
    }
    clReleaseContext(context);

    delete[] device_ids;
}

double OCLKernels::multTransOCL(double* ptrSource, double* ptrData, double* ptrLevel, double* ptrIndex, double* ptrGlobalResult, size_t sourceSize, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultTransDP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable" << std::endl << std::endl;
		stream_program_src << "__kernel void multTransOCL(__global double* ptrSource," << std::endl;
		stream_program_src << "						__global double* ptrData," << std::endl;
		stream_program_src << "						__global double* ptrLevel," << std::endl;
		stream_program_src << "						__global double* ptrIndex," << std::endl;
		stream_program_src << "						__global double* ptrResult," << std::endl;
		stream_program_src << "						uint sourceSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	double eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	double myResult = 0.0f;" << std::endl << std::endl;
		stream_program_src << "	__local double locData[" << dims*OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << "];" << std::endl;
		stream_program_src << "	__local double locSource[" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << "];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	double level_" << d << " = ptrLevel[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "	double index_" << d << " = ptrIndex[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points" << std::endl;
		stream_program_src << "	for(int i = 0; i < sourceSize; i+=" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locData[(localIdx*" << dims << ")+" << d << "] = ptrData[((i+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locSource[localIdx] = ptrSource[i+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;

		stream_program_src << "			curSupport = locSource[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			eval = ((level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]));" << std::endl;
			stream_program_src << "			index_calc = eval - (index_" << d << ");" << std::endl;
			stream_program_src << "			abs = fabs(index_calc);" << std::endl;
			stream_program_src << "			last = 1.0 - abs;" << std::endl;
			stream_program_src << "			localSupport = fmax(last, 0.0);" << std::endl;
			stream_program_src << "			curSupport *= localSupport;" << std::endl;
		}
		stream_program_src << std::endl << "		myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

	    // setting the program
	    const char* kernel_src = program_src.c_str();
	    program_multTransDP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
	    if (err != CL_SUCCESS)
		{
	    	std::cout << "OCL Error: Failed to create program! Error Code: " << err << std::endl;
	    	return 0.0;
		}

	    // compiling the program
	    err = clBuildProgram(program_multTransDP, 0, NULL, "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math", NULL, NULL);

	    if (err != CL_SUCCESS)
	    {
	    	std::cout << "OCL Error: OpenCL Build Error. Error Code: " << err << std::endl;

	    	size_t len;
	    	char buffer[2048];

	    	// get the build log
	    	clGetProgramBuildInfo(program_multTransDP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

	    	std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
	    	return 0.0;
	    }

	    // creating the kernel
	    for (size_t i = 0; i < num_devices; i++)
	    {
			kernel_multTransDP[i] = clCreateKernel(program_multTransDP, "multTransOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "OCL Error: Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
	    }
	}

	if (isFirstTimeMultDP && isFirstTimeMultTransDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrLevel, NULL);
			clIndexDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*sourceSize, ptrData, NULL);
		}
		isVeryFirstTimeDP = false;
	}

	cl_mem clSource[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];
	for (size_t i = 0; i < num_devices; i++)
	{
		clSource[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*sourceSize, ptrSource, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(double)*gpu_partition, NULL, NULL);
	}

    cl_uint clSourceSize = (cl_uint)sourceSize;
    cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

    // determine best fit
	size_t numWGs = gpu_partition/OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP;
    size_t global = numWGs*(OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/num_devices);
    size_t local = OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2;
    size_t offset = 0;

    // if there is not enough workload for GPUs
    if (global == 0)
    {
    	return 0.0;
    }

    // set kernel arguments
    for (size_t i = 0; i < num_devices; i++)
    {
    	clOffsets[i] = (cl_uint)offset;
		if ( clSetKernelArg(kernel_multTransDP[i], 0, sizeof(cl_mem), &clSource[i]) ||
				clSetKernelArg(kernel_multTransDP[i], 1, sizeof(cl_mem), &clDataDP[i]) ||
				clSetKernelArg(kernel_multTransDP[i], 2, sizeof(cl_mem), &clLevelDP[i]) ||
				clSetKernelArg(kernel_multTransDP[i], 3, sizeof(cl_mem), &clIndexDP[i]) ||
				clSetKernelArg(kernel_multTransDP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multTransDP[i], 5, sizeof(cl_uint), &clSourceSize) ||
				clSetKernelArg(kernel_multTransDP[i], 6, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to create kernel Args for kernel " << i << "!" << std::endl;
			return 0.0;
		}

		offset += global;
    }

    cl_event clTimings[MAX_OCL_DEVICE_COUNT];
    cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

    // enqueue kernels
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multTransDP[i], 1, NULL, &global, &local, 0, NULL, &clTimings[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
    }

	// read data back
    offset = 0;
    for (size_t i = 0; i < num_devices; i++)
    {
    	err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(double)*offset, sizeof(double)*global, &ptrGlobalResult[offset], 0, NULL, &GPUDone[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue read buffer command (mult)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
    }

    // sync GPUs
    clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
    for (size_t i = 0; i < num_devices; i++)
	{
    	double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

    // clean up
    for (size_t i = 0; i < num_devices; i++)
    {
    	clReleaseMemObject(clSource[i]);
    	clReleaseMemObject(clResult[i]);
    }

    isFirstTimeMultTransDP = false;

	return time;
}

double OCLKernels::multOCL(double* ptrAlpha, double* ptrData, double* ptrLevel, double* ptrIndex, double* ptrResult, size_t result_size, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultDP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable" << std::endl << std::endl;
		stream_program_src << "__kernel void multOCL(__global double* ptrAlpha," << std::endl;
		stream_program_src << "						__global double* ptrData," << std::endl;
		stream_program_src << "						__global double* ptrLevel," << std::endl;
		stream_program_src << "						__global double* ptrIndex," << std::endl;
		stream_program_src << "						__global double* ptrResult," << std::endl;
		stream_program_src << "						uint fastStorageSize," << std::endl;
		stream_program_src << "						uint storageSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	__local double locLevel[" << dims * OCL_DATAPREFETCH_SIZE_DP << "];" << std::endl;
		stream_program_src << "	__local double locIndex[" << dims * OCL_DATAPREFETCH_SIZE_DP << "];" << std::endl;
		stream_program_src << "	__local double locAlpha[" << OCL_DATAPREFETCH_SIZE_DP << "];" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	double eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	double myResult = 0.0;" << std::endl << std::endl;
		stream_program_src << "	// Create registers for the data" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	double data_" << d << " = ptrData[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (fast ones, with cache)" << std::endl;
		stream_program_src << "	for(int j = 0; j < fastStorageSize; j+=" << OCL_DATAPREFETCH_SIZE_DP << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locLevel[(localIdx*" << dims << ")+"<< d << "] = ptrLevel[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "		locIndex[(localIdx*" << dims << ")+"<< d << "] = ptrIndex[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locAlpha[localIdx] = ptrAlpha[j+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_DATAPREFETCH_SIZE_DP << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;
		stream_program_src << "			curSupport = locAlpha[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			eval = ((locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << "));" << std::endl;
			stream_program_src << "			index_calc = eval - (locIndex[(k*" << dims << ")+" << d << "]);" << std::endl;
			stream_program_src << "			abs = fabs(index_calc);" << std::endl;
			stream_program_src << "			last = 1.0 - abs;" << std::endl;
			stream_program_src << "			localSupport = fmax(last, 0.0);" << std::endl;
			stream_program_src << "			curSupport *= localSupport;" << std::endl << std::endl;
		}
		stream_program_src << "			myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (slow ones, without cache)" << std::endl;
		stream_program_src << "	for(int m = fastStorageSize; m < storageSize; m++)" << std::endl;
		stream_program_src << "	{" << std::endl;
		stream_program_src << "		curSupport = ptrAlpha[m];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		eval = ((ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << "));" << std::endl;
			stream_program_src << "		index_calc = eval - (ptrIndex[(m*" << dims << ")+" << d << "]);" << std::endl;
			stream_program_src << "		abs = fabs(index_calc);" << std::endl;
			stream_program_src << "		last = 1.0 - abs;" << std::endl;
			stream_program_src << "		localSupport = fmax(last, 0.0);" << std::endl;
			stream_program_src << "		curSupport *= localSupport;" << std::endl << std::endl;
		}
		stream_program_src << "		myResult += curSupport;" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

	    // setting the program
	    const char* kernel_src = program_src.c_str();
	    program_multDP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
	    if (err != CL_SUCCESS)
		{
	    	std::cout << "OCL Error: Failed to create program! Error Code: " << err << std::endl;
	    	return 0.0;
		}

	    // compiling the program
	    err = clBuildProgram(program_multDP, 0, NULL,  "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math", NULL, NULL);

	    if (err != CL_SUCCESS)
	    {
	    	std::cout << "OCL Error: OpenCL Build Error. Error Code: " << err << std::endl;

	    	size_t len;
	    	char buffer[2048];

	    	// get the build log
	    	clGetProgramBuildInfo(program_multDP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

	    	std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
	    	return 0.0;
	    }

	    // creating the kernels
	    for (size_t i = 0; i < num_devices; i++)
	    {
			kernel_multDP[i] = clCreateKernel(program_multDP, "multOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "OCL Error: Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
	    }
	}

	if (isFirstTimeMultDP && isFirstTimeMultTransDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrLevel, NULL);
			clIndexDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*result_size, ptrData, NULL);
		}
		isVeryFirstTimeDP = false;
	}

	cl_mem clAlpha[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];

	for(size_t i = 0; i < num_devices; i++)
	{
		clAlpha[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*storageSize, ptrAlpha, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(double)*gpu_partition, NULL, NULL);
	}

	size_t global = gpu_partition/num_devices;
    size_t local = OCL_DATAPREFETCH_SIZE_DP;
    size_t offset = 0;

	size_t oclStorageSize = storageSize/OCL_DATAPREFETCH_SIZE_DP;
    oclStorageSize *= OCL_DATAPREFETCH_SIZE_DP;

    cl_uint clFastStorageSize = (cl_uint)(oclStorageSize);
    cl_uint clStorageSize = (cl_uint)(storageSize);
    cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

    for (size_t i = 0; i < num_devices; i++)
    {
    	clOffsets[i] = (cl_uint)offset;
		// set kernel arguments
		if ( clSetKernelArg(kernel_multDP[i], 0, sizeof(cl_mem), &clAlpha[i]) ||
				clSetKernelArg(kernel_multDP[i], 1, sizeof(cl_mem), &clDataDP[i]) ||
				clSetKernelArg(kernel_multDP[i], 2, sizeof(cl_mem), &clLevelDP[i]) ||
				clSetKernelArg(kernel_multDP[i], 3, sizeof(cl_mem), &clIndexDP[i]) ||
				clSetKernelArg(kernel_multDP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multDP[i], 5, sizeof(cl_uint), &clFastStorageSize) ||
				clSetKernelArg(kernel_multDP[i], 6, sizeof(cl_uint), &clStorageSize) ||
				clSetKernelArg(kernel_multDP[i], 7, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to create kernel Args!" << std::endl;
			return 0.0;
		}
		offset += global;
    }

    cl_event clTimings[MAX_OCL_DEVICE_COUNT];
    cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

    // enqueue kernel
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multDP[i], 1, NULL, &global, &local, 0, NULL, &(clTimings[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
    }

    // read data back
    offset = 0;
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(double)*offset, sizeof(double)*global, &(ptrResult[offset]), 0, NULL, &(GPUDone[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue read buffer command (multTrans)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
    }

    // sync GPUs
    clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
    for (size_t i = 0; i < num_devices; i++)
	{
    	double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

    // clean up
    for (size_t i = 0; i < num_devices; i++)
    {
    	clReleaseMemObject(clAlpha[i]);
    	clReleaseMemObject(clResult[i]);
    }

    isFirstTimeMultDP = false;

	return time;
}

double OCLKernels::multTransSPOCL(float* ptrSource, float* ptrData, float* ptrLevel, float* ptrIndex, float* ptrGlobalResult, size_t sourceSize, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultTransSP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "__kernel void multTransSPOCL(__global float* ptrSource," << std::endl;
		stream_program_src << "						__global float* ptrData," << std::endl;
		stream_program_src << "						__global float* ptrLevel," << std::endl;
		stream_program_src << "						__global float* ptrIndex," << std::endl;
		stream_program_src << "						__global float* ptrResult," << std::endl;
		stream_program_src << "						uint sourceSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	float eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	float myResult = 0.0f;" << std::endl << std::endl;
		stream_program_src << "	__local float locData[" << dims*OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << "];" << std::endl;
		stream_program_src << "	__local float locSource[" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << "];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	float level_" << d << " = ptrLevel[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "	float index_" << d << " = ptrIndex[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points" << std::endl;
		stream_program_src << "	for(int i = 0; i < sourceSize; i+=" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locData[(localIdx*" << dims << ")+" << d << "] = ptrData[((i+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locSource[localIdx] = ptrSource[i+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;

		stream_program_src << "			curSupport = locSource[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			eval = ((level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]));" << std::endl;
			stream_program_src << "			index_calc = eval - (index_" << d << ");" << std::endl;
			stream_program_src << "			abs = fabs(index_calc);" << std::endl;
			stream_program_src << "			last = 1.0f - abs;" << std::endl;
			stream_program_src << "			localSupport = fmax(last, 0.0f);" << std::endl;
			stream_program_src << "			curSupport *= localSupport;" << std::endl;
		}
		stream_program_src << std::endl << "		myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

	    // setting the program
	    const char* kernel_src = program_src.c_str();
	    program_multTransSP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
	    if (err != CL_SUCCESS)
		{
	    	std::cout << "OCL Error: Failed to create program! Error Code: " << err << std::endl;
	    	return 0.0;
		}

	    // compiling the program
	    err = clBuildProgram(program_multTransSP, 0, NULL, "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math -cl-single-precision-constant", NULL, NULL);
	    //err = clBuildProgram(program_multTransSP, 0, NULL, "-cl-opt-disable", NULL, NULL);
	    //err = clBuildProgram(program_multTransSP, 0, NULL, NULL, NULL, NULL);
	    if (err != CL_SUCCESS)
	    {
	    	std::cout << "OCL Error: OpenCL Build Error. Error Code: " << err << std::endl;

	    	size_t len;
	    	char buffer[2048];

	    	// get the build log
	    	clGetProgramBuildInfo(program_multTransSP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

	    	std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
	    	return 0.0;
	    }

	    // creating the kernel
	    for (size_t i = 0; i < num_devices; i++)
	    {
			kernel_multTransSP[i] = clCreateKernel(program_multTransSP, "multTransSPOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "OCL Error: Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
	    }
	}

	if (isFirstTimeMultSP && isFirstTimeMultTransSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrLevel, NULL);
			clIndexSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*sourceSize, ptrData, NULL);
		}
		isVeryFirstTimeSP = false;
	}

	cl_mem clSource[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];
	for (size_t i = 0; i < num_devices; i++)
	{
		clSource[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*sourceSize, ptrSource, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*gpu_partition, NULL, NULL);
	}

    cl_uint clSourceSize = (cl_uint)sourceSize;
    cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

    // determine best fit
	size_t numWGs = gpu_partition/OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP;
    size_t global = numWGs*(OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/num_devices);
    size_t local = OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2;
    size_t offset = 0;

    // if there is not enough workload for GPUs
    if (global == 0)
    {
    	return 0.0;
    }

    // set kernel arguments
    for (size_t i = 0; i < num_devices; i++)
    {
    	clOffsets[i] = (cl_uint)offset;
		if ( clSetKernelArg(kernel_multTransSP[i], 0, sizeof(cl_mem), &clSource[i]) ||
				clSetKernelArg(kernel_multTransSP[i], 1, sizeof(cl_mem), &clDataSP[i]) ||
				clSetKernelArg(kernel_multTransSP[i], 2, sizeof(cl_mem), &clLevelSP[i]) ||
				clSetKernelArg(kernel_multTransSP[i], 3, sizeof(cl_mem), &clIndexSP[i]) ||
				clSetKernelArg(kernel_multTransSP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multTransSP[i], 5, sizeof(cl_uint), &clSourceSize) ||
				clSetKernelArg(kernel_multTransSP[i], 6, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to create kernel Args for kernel " << i << "!" << std::endl;
			return 0.0;
		}

		offset += global;
    }

    cl_event clTimings[MAX_OCL_DEVICE_COUNT];
    cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

    // enqueue kernels
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multTransSP[i], 1, NULL, &global, &local, 0, NULL, &clTimings[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
    }

	// read data back
    offset = 0;
    for (size_t i = 0; i < num_devices; i++)
    {
    	err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(float)*offset, sizeof(float)*global, &ptrGlobalResult[offset], 0, NULL, &GPUDone[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue read buffer command (mult)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
    }

    // sync GPUs
    clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
    for (size_t i = 0; i < num_devices; i++)
	{
    	double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

    // clean up
    for (size_t i = 0; i < num_devices; i++)
    {
    	clReleaseMemObject(clSource[i]);
    	clReleaseMemObject(clResult[i]);
    }

    isFirstTimeMultTransSP = false;

	return time;
}

double OCLKernels::multSPOCL(float* ptrAlpha, float* ptrData, float* ptrLevel, float* ptrIndex, float* ptrResult, size_t result_size, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultSP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "__kernel void multSPOCL(__global float* ptrAlpha," << std::endl;
		stream_program_src << "						__global float* ptrData," << std::endl;
		stream_program_src << "						__global float* ptrLevel," << std::endl;
		stream_program_src << "						__global float* ptrIndex," << std::endl;
		stream_program_src << "						__global float* ptrResult," << std::endl;
		stream_program_src << "						uint fastStorageSize," << std::endl;
		stream_program_src << "						uint storageSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	__local float locLevel[" << dims * OCL_DATAPREFETCH_SIZE_SP << "];" << std::endl;
		stream_program_src << "	__local float locIndex[" << dims * OCL_DATAPREFETCH_SIZE_SP << "];" << std::endl;
		stream_program_src << "	__local float locAlpha[" << OCL_DATAPREFETCH_SIZE_SP << "];" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	float eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	float myResult = 0.0f;" << std::endl << std::endl;
		stream_program_src << "	// Create registers for the data" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	float data_" << d << " = ptrData[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (fast ones, with cache)" << std::endl;
		stream_program_src << "	for(int j = 0; j < fastStorageSize; j+=" << OCL_DATAPREFETCH_SIZE_SP << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locLevel[(localIdx*" << dims << ")+"<< d << "] = ptrLevel[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "		locIndex[(localIdx*" << dims << ")+"<< d << "] = ptrIndex[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locAlpha[localIdx] = ptrAlpha[j+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_DATAPREFETCH_SIZE_SP << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;
		stream_program_src << "			curSupport = locAlpha[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			eval = ((locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << "));" << std::endl;
			stream_program_src << "			index_calc = eval - (locIndex[(k*" << dims << ")+" << d << "]);" << std::endl;
			stream_program_src << "			abs = fabs(index_calc);" << std::endl;
			stream_program_src << "			last = 1.0f - abs;" << std::endl;
			stream_program_src << "			localSupport = fmax(last, 0.0f);" << std::endl;
			stream_program_src << "			curSupport *= localSupport;" << std::endl << std::endl;
		}
		stream_program_src << "			myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (slow ones, without cache)" << std::endl;
		stream_program_src << "	for(int m = fastStorageSize; m < storageSize; m++)" << std::endl;
		stream_program_src << "	{" << std::endl;
		stream_program_src << "		curSupport = ptrAlpha[m];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		eval = ((ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << "));" << std::endl;
			stream_program_src << "		index_calc = eval - (ptrIndex[(m*" << dims << ")+" << d << "]);" << std::endl;
			stream_program_src << "		abs = fabs(index_calc);" << std::endl;
			stream_program_src << "		last = 1.0f - abs;" << std::endl;
			stream_program_src << "		localSupport = fmax(last, 0.0f);" << std::endl;
			stream_program_src << "		curSupport *= localSupport;" << std::endl << std::endl;
		}
		stream_program_src << "		myResult += curSupport;" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

	    // setting the program
	    const char* kernel_src = program_src.c_str();
	    program_multSP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
	    if (err != CL_SUCCESS)
		{
	    	std::cout << "OCL Error: Failed to create program! Error Code: " << err << std::endl;
	    	return 0.0;
		}

	    // compiling the program
	    err = clBuildProgram(program_multSP, 0, NULL,  "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math -cl-single-precision-constant", NULL, NULL);
	    //err = clBuildProgram(program_multSP, 0, NULL, "-cl-opt-disable", NULL, NULL);
	    //err = clBuildProgram(program_multSP, 0, NULL, NULL, NULL, NULL);
	    if (err != CL_SUCCESS)
	    {
	    	std::cout << "OCL Error: OpenCL Build Error. Error Code: " << err << std::endl;

	    	size_t len;
	    	char buffer[2048];

	    	// get the build log
	    	clGetProgramBuildInfo(program_multSP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

	    	std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
	    	return 0.0;
	    }

	    // creating the kernels
	    for (size_t i = 0; i < num_devices; i++)
	    {
			kernel_multSP[i] = clCreateKernel(program_multSP, "multSPOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "OCL Error: Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
	    }
	}

	if (isFirstTimeMultSP && isFirstTimeMultTransSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrLevel, NULL);
			clIndexSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*result_size, ptrData, NULL);
		}
		isVeryFirstTimeSP = false;
	}

	cl_mem clAlpha[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];

	for(size_t i = 0; i < num_devices; i++)
	{
		clAlpha[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*storageSize, ptrAlpha, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*gpu_partition, NULL, NULL);
	}

	size_t global = gpu_partition/num_devices;
    size_t local = OCL_DATAPREFETCH_SIZE_SP;
    size_t offset = 0;

	size_t oclStorageSize = storageSize/OCL_DATAPREFETCH_SIZE_SP;
    oclStorageSize *= OCL_DATAPREFETCH_SIZE_SP;

    cl_uint clFastStorageSize = (cl_uint)(oclStorageSize);
    cl_uint clStorageSize = (cl_uint)(storageSize);
    cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

    for (size_t i = 0; i < num_devices; i++)
    {
    	clOffsets[i] = (cl_uint)offset;
		// set kernel arguments
		if ( clSetKernelArg(kernel_multSP[i], 0, sizeof(cl_mem), &clAlpha[i]) ||
				clSetKernelArg(kernel_multSP[i], 1, sizeof(cl_mem), &clDataSP[i]) ||
				clSetKernelArg(kernel_multSP[i], 2, sizeof(cl_mem), &clLevelSP[i]) ||
				clSetKernelArg(kernel_multSP[i], 3, sizeof(cl_mem), &clIndexSP[i]) ||
				clSetKernelArg(kernel_multSP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multSP[i], 5, sizeof(cl_uint), &clFastStorageSize) ||
				clSetKernelArg(kernel_multSP[i], 6, sizeof(cl_uint), &clStorageSize) ||
				clSetKernelArg(kernel_multSP[i], 7, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to create kernel Args!" << std::endl;
			return 0.0;
		}
		offset += global;
    }

    cl_event clTimings[MAX_OCL_DEVICE_COUNT];
    cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

    // enqueue kernel
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multSP[i], 1, NULL, &global, &local, 0, NULL, &(clTimings[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
    }

    // read data back
    offset = 0;
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(float)*offset, sizeof(float)*global, &(ptrResult[offset]), 0, NULL, &(GPUDone[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to enqueue read buffer command (multTrans)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
    }

    // sync GPUs
    clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
    for (size_t i = 0; i < num_devices; i++)
	{
    	double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "OCL Error: Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

    // clean up
    for (size_t i = 0; i < num_devices; i++)
    {
    	clReleaseMemObject(clAlpha[i]);
    	clReleaseMemObject(clResult[i]);
    }

    isFirstTimeMultSP = false;

	return time;
}

double OCLKernels::multTransModOCL(double* ptrSource, double* ptrData, double* ptrLevel, double* ptrIndex, double* ptrGlobalResult, size_t sourceSize, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultTransDP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable" << std::endl << std::endl;
		stream_program_src << "__kernel void multTransModOCL(__global double* ptrSource," << std::endl;
		stream_program_src << "						__global double* ptrData," << std::endl;
		stream_program_src << "						__global double* ptrLevel," << std::endl;
		stream_program_src << "						__global double* ptrIndex," << std::endl;
		stream_program_src << "						__global double* ptrResult," << std::endl;
		stream_program_src << "						uint sourceSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	double eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	double myResult = 0.0f;" << std::endl << std::endl;
		stream_program_src << "	__local double locData[" << dims*OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << "];" << std::endl;
		stream_program_src << "	__local double locSource[" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << "];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	double level_" << d << " = ptrLevel[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "	double index_" << d << " = ptrIndex[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points" << std::endl;
		stream_program_src << "	for(int i = 0; i < sourceSize; i+=" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locData[(localIdx*" << dims << ")+" << d << "] = ptrData[((i+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locSource[localIdx] = ptrSource[i+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2 << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;

		stream_program_src << "			curSupport = locSource[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			if ((level_" << d << ") == 2.0)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= 1.0;" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((index_" << d << ") == 1.0)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(2.0 - ( (level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]) ), 0.0) ;" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((index_" << d << ") == ((level_" << d << ") - 1.0) )" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(( (level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]) ) - (index_" << d << ") + 1.0, 0.0);" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else " << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(1.0 - fabs( ( (level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]) ) - (index_" << d << ") ), 0.0);" << std::endl;
			stream_program_src << "			}" << std::endl;
		}
		stream_program_src << std::endl << "		myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

		// setting the program
		const char* kernel_src = program_src.c_str();
		program_multTransModDP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to create program! Error Code: " << err << std::endl;
			return 0.0;
		}

		// compiling the program
		err = clBuildProgram(program_multTransModDP, 0, NULL, "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math", NULL, NULL);

		if (err != CL_SUCCESS)
		{
			std::cout << "OpenCL Build Error. Error Code: " << err << std::endl;

			size_t len;
			char buffer[2048];

			// get the build log
			clGetProgramBuildInfo(program_multTransModDP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

			std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
			return 0.0;
		}

		// creating the kernel
		for (size_t i = 0; i < num_devices; i++)
		{
			kernel_multTransModDP[i] = clCreateKernel(program_multTransModDP, "multTransModOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
		}
	}

	if (isFirstTimeMultModDP && isFirstTimeMultTransModDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrLevel, NULL);
			clIndexDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*sourceSize, ptrData, NULL);
		}
		isVeryFirstTimeDP = false;
	}

	cl_mem clSource[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];
	for (size_t i = 0; i < num_devices; i++)
	{
		clSource[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*sourceSize, ptrSource, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(double)*gpu_partition, NULL, NULL);
	}

	cl_uint clSourceSize = (cl_uint)sourceSize;
	cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

	// determine best fit
	size_t numWGs = gpu_partition/OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP;
	size_t global = numWGs*(OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/num_devices);
	size_t local = OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_DP/2;
	size_t offset = 0;

	// if there is not enough workload for GPUs
	if (global == 0)
	{
		return 0.0;
	}

	// set kernel arguments
	for (size_t i = 0; i < num_devices; i++)
	{
		clOffsets[i] = (cl_uint)offset;
		if ( clSetKernelArg(kernel_multTransModDP[i], 0, sizeof(cl_mem), &clSource[i]) ||
				clSetKernelArg(kernel_multTransModDP[i], 1, sizeof(cl_mem), &clDataDP[i]) ||
				clSetKernelArg(kernel_multTransModDP[i], 2, sizeof(cl_mem), &clLevelDP[i]) ||
				clSetKernelArg(kernel_multTransModDP[i], 3, sizeof(cl_mem), &clIndexDP[i]) ||
				clSetKernelArg(kernel_multTransModDP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multTransModDP[i], 5, sizeof(cl_uint), &clSourceSize) ||
				clSetKernelArg(kernel_multTransModDP[i], 6, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "err" << std::endl;
			std::cout << "Failed to create kernel Args for kernel " << i << "!" << std::endl;
			return 0.0;
		}

		offset += global;
	}

	cl_event clTimings[MAX_OCL_DEVICE_COUNT];
	cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

	// enqueue kernels
	for (size_t i = 0; i < num_devices; i++)
	{
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multTransModDP[i], 1, NULL, &global, &local, 0, NULL, &clTimings[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
	}

	// read data back
	offset = 0;
	for (size_t i = 0; i < num_devices; i++)
	{
		err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(double)*offset, sizeof(double)*global, &ptrGlobalResult[offset], 0, NULL, &GPUDone[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue read buffer command (mult)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
	}

	// sync GPUs
	clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
	for (size_t i = 0; i < num_devices; i++)
	{
		double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

	// clean up
	for (size_t i = 0; i < num_devices; i++)
	{
		clReleaseMemObject(clSource[i]);
		clReleaseMemObject(clResult[i]);
	}

	isFirstTimeMultTransModDP = false;

	return time;
}

double OCLKernels::multModOCL(double* ptrAlpha, double* ptrData, double* ptrLevel, double* ptrIndex, double* ptrResult, size_t result_size, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultModDP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable" << std::endl << std::endl;
		stream_program_src << "__kernel void multModOCL(__global double* ptrAlpha," << std::endl;
		stream_program_src << "						__global double* ptrData," << std::endl;
		stream_program_src << "						__global double* ptrLevel," << std::endl;
		stream_program_src << "						__global double* ptrIndex," << std::endl;
		stream_program_src << "						__global double* ptrResult," << std::endl;
		stream_program_src << "						uint fastStorageSize," << std::endl;
		stream_program_src << "						uint storageSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	__local double locLevel[" << dims * OCL_DATAPREFETCH_SIZE_DP << "];" << std::endl;
		stream_program_src << "	__local double locIndex[" << dims * OCL_DATAPREFETCH_SIZE_DP << "];" << std::endl;
		stream_program_src << "	__local double locAlpha[" << OCL_DATAPREFETCH_SIZE_DP << "];" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	double eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	double myResult = 0.0;" << std::endl << std::endl;
		stream_program_src << "	// Create registers for the data" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	double data_" << d << " = ptrData[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (fast ones, with cache)" << std::endl;
		stream_program_src << "	for(int j = 0; j < fastStorageSize; j+=" << OCL_DATAPREFETCH_SIZE_DP << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locLevel[(localIdx*" << dims << ")+"<< d << "] = ptrLevel[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "		locIndex[(localIdx*" << dims << ")+"<< d << "] = ptrIndex[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locAlpha[localIdx] = ptrAlpha[j+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_DATAPREFETCH_SIZE_DP << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;
		stream_program_src << "			curSupport = locAlpha[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			if ((locLevel[(k*" << dims << ")+" << d << "]) == 2.0)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= 1.0;" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((locIndex[(k*" << dims << ")+" << d << "]) == 1.0)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(2.0 - ( (locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << ") ), 0.0) ;" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((locIndex[(k*" << dims << ")+" << d << "]) == ((locLevel[(k*" << dims << ")+" << d << "]) - 1.0) )" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(( (locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (locIndex[(k*" << dims << ")+" << d << "]) + 1.0, 0.0);" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else " << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(1.0 - fabs( ( (locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (locIndex[(k*" << dims << ")+" << d << "]) ), 0.0);" << std::endl;
			stream_program_src << "			}" << std::endl;
		}
		stream_program_src << "			myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (slow ones, without cache)" << std::endl;
		stream_program_src << "	for(int m = fastStorageSize; m < storageSize; m++)" << std::endl;
		stream_program_src << "	{" << std::endl;
		stream_program_src << "		curSupport = ptrAlpha[m];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		if ((ptrLevel[(m*" << dims << ")+" << d << "]) == 2.0)" << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= 1.0;" << std::endl;
			stream_program_src << "		}" << std::endl;
			stream_program_src << "		else if ((ptrIndex[(m*" << dims << ")+" << d << "]) == 1.0)" << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= max(2.0 - ( (ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << ") ), 0.0) ;" << std::endl;
			stream_program_src << "		}" << std::endl;
			stream_program_src << "		else if ((ptrIndex[(m*" << dims << ")+" << d << "]) == ((ptrLevel[(m*" << dims << ")+" << d << "]) - 1.0) )" << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= max(( (ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (ptrIndex[(m*" << dims << ")+" << d << "]) + 1.0, 0.0);" << std::endl;
			stream_program_src << "		}" << std::endl;
			stream_program_src << "		else " << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= max(1.0 - fabs( ( (ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (ptrIndex[(m*" << dims << ")+" << d << "]) ), 0.0);" << std::endl;
			stream_program_src << "		}" << std::endl;
		}
		stream_program_src << "		myResult += curSupport;" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

	    // setting the program
	    const char* kernel_src = program_src.c_str();
	    program_multModDP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
	    if (err != CL_SUCCESS)
		{
	    	std::cout << "Failed to create program! Error Code: " << err << std::endl;
	    	return 0.0;
		}

	    // compiling the program
	    err = clBuildProgram(program_multModDP, 0, NULL,  "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math", NULL, NULL);

	    if (err != CL_SUCCESS)
	    {
	    	std::cout << "OpenCL Build Error. Error Code: " << err << std::endl;

	    	size_t len;
	    	char buffer[2048];

	    	// get the build log
	    	clGetProgramBuildInfo(program_multModDP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

	    	std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
	    	return 0.0;
	    }

	    // creating the kernels
	    for (size_t i = 0; i < num_devices; i++)
	    {
			kernel_multModDP[i] = clCreateKernel(program_multModDP, "multModOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
	    }
	}

	if (isFirstTimeMultModDP && isFirstTimeMultTransModDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrLevel, NULL);
			clIndexDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataDP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*dims*result_size, ptrData, NULL);
		}
		isVeryFirstTimeDP = false;
	}

	cl_mem clAlpha[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];

	for(size_t i = 0; i < num_devices; i++)
	{
		clAlpha[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(double)*storageSize, ptrAlpha, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(double)*gpu_partition, NULL, NULL);
	}

	size_t global = gpu_partition/num_devices;
    size_t local = OCL_DATAPREFETCH_SIZE_DP;
    size_t offset = 0;

	size_t oclStorageSize = storageSize/OCL_DATAPREFETCH_SIZE_DP;
    oclStorageSize *= OCL_DATAPREFETCH_SIZE_DP;

    cl_uint clFastStorageSize = (cl_uint)(oclStorageSize);
    cl_uint clStorageSize = (cl_uint)(storageSize);
    cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

    for (size_t i = 0; i < num_devices; i++)
    {
    	clOffsets[i] = (cl_uint)offset;
		// set kernel arguments
		if ( clSetKernelArg(kernel_multModDP[i], 0, sizeof(cl_mem), &clAlpha[i]) ||
				clSetKernelArg(kernel_multModDP[i], 1, sizeof(cl_mem), &clDataDP[i]) ||
				clSetKernelArg(kernel_multModDP[i], 2, sizeof(cl_mem), &clLevelDP[i]) ||
				clSetKernelArg(kernel_multModDP[i], 3, sizeof(cl_mem), &clIndexDP[i]) ||
				clSetKernelArg(kernel_multModDP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multModDP[i], 5, sizeof(cl_uint), &clFastStorageSize) ||
				clSetKernelArg(kernel_multModDP[i], 6, sizeof(cl_uint), &clStorageSize) ||
				clSetKernelArg(kernel_multModDP[i], 7, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "Failed to create kernel Args!" << std::endl;
			return 0.0;
		}
		offset += global;
    }

    cl_event clTimings[MAX_OCL_DEVICE_COUNT];
    cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

    // enqueue kernel
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multModDP[i], 1, NULL, &global, &local, 0, NULL, &(clTimings[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
    }

    // read data back
    offset = 0;
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(double)*offset, sizeof(double)*global, &(ptrResult[offset]), 0, NULL, &(GPUDone[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue read buffer command (multTrans)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
    }

    // sync GPUs
    clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
    for (size_t i = 0; i < num_devices; i++)
	{
    	double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

    // clean up
    for (size_t i = 0; i < num_devices; i++)
    {
    	clReleaseMemObject(clAlpha[i]);
    	clReleaseMemObject(clResult[i]);
    }

    isFirstTimeMultModDP = false;

	return time;
}

double OCLKernels::multTransModSPOCL(float* ptrSource, float* ptrData, float* ptrLevel, float* ptrIndex, float* ptrGlobalResult, size_t sourceSize, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultTransModSP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "__kernel void multTransModSPOCL(__global float* ptrSource," << std::endl;
		stream_program_src << "						__global float* ptrData," << std::endl;
		stream_program_src << "						__global float* ptrLevel," << std::endl;
		stream_program_src << "						__global float* ptrIndex," << std::endl;
		stream_program_src << "						__global float* ptrResult," << std::endl;
		stream_program_src << "						uint sourceSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	float eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	float myResult = 0.0f;" << std::endl << std::endl;
		stream_program_src << "	__local float locData[" << dims*OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << "];" << std::endl;
		stream_program_src << "	__local float locSource[" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << "];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	float level_" << d << " = ptrLevel[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "	float index_" << d << " = ptrIndex[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points" << std::endl;
		stream_program_src << "	for(int i = 0; i < sourceSize; i+=" << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locData[(localIdx*" << dims << ")+" << d << "] = ptrData[((i+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locSource[localIdx] = ptrSource[i+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2 << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;

		stream_program_src << "			curSupport = locSource[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			if ((level_" << d << ") == 2.0f)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= 1.0f;" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((index_" << d << ") == 1.0f)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(2.0f - ( (level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]) ), 0.0f);" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((index_" << d << ") == ((level_" << d << ") - 1.0f))" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(( (level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]) ) - (index_" << d << ") + 1.0f, 0.0f);" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else " << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(1.0 - fabs( ( (level_" << d << ") * (locData[(k*" << dims << ")+" << d << "]) ) - (index_" << d << ") ), 0.0f);" << std::endl;
			stream_program_src << "			}" << std::endl;
		}
		stream_program_src << std::endl << "		myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

		// setting the program
		const char* kernel_src = program_src.c_str();
		program_multTransModSP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to create program! Error Code: " << err << std::endl;
			return 0.0;
		}

		// compiling the program
		err = clBuildProgram(program_multTransModSP, 0, NULL, "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math", NULL, NULL);

		if (err != CL_SUCCESS)
		{
			std::cout << "OpenCL Build Error. Error Code: " << err << std::endl;

			size_t len;
			char buffer[2048];

			// get the build log
			clGetProgramBuildInfo(program_multTransModSP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

			std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
			return 0.0;
		}

		// creating the kernel
		for (size_t i = 0; i < num_devices; i++)
		{
			kernel_multTransModSP[i] = clCreateKernel(program_multTransModSP, "multTransModSPOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
		}
	}

	if (isFirstTimeMultModSP && isFirstTimeMultTransModSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrLevel, NULL);
			clIndexSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*sourceSize, ptrData, NULL);
		}
		isVeryFirstTimeSP = false;
	}

	cl_mem clSource[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];
	for (size_t i = 0; i < num_devices; i++)
	{
		clSource[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*sourceSize, ptrSource, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*gpu_partition, NULL, NULL);
	}

	cl_uint clSourceSize = (cl_uint)sourceSize;
	cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

	// determine best fit
	size_t numWGs = gpu_partition/OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP;
	size_t global = numWGs*(OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/num_devices);
	size_t local = OCL_MULT_N_DATAPREFETCH_BLOCKSIZE_SP/2;
	size_t offset = 0;

	// if there is not enough workload for GPUs
	if (global == 0)
	{
		return 0.0;
	}

	// set kernel arguments
	for (size_t i = 0; i < num_devices; i++)
	{
		clOffsets[i] = (cl_uint)offset;
		if ( clSetKernelArg(kernel_multTransModSP[i], 0, sizeof(cl_mem), &clSource[i]) ||
				clSetKernelArg(kernel_multTransModSP[i], 1, sizeof(cl_mem), &clDataSP[i]) ||
				clSetKernelArg(kernel_multTransModSP[i], 2, sizeof(cl_mem), &clLevelSP[i]) ||
				clSetKernelArg(kernel_multTransModSP[i], 3, sizeof(cl_mem), &clIndexSP[i]) ||
				clSetKernelArg(kernel_multTransModSP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multTransModSP[i], 5, sizeof(cl_uint), &clSourceSize) ||
				clSetKernelArg(kernel_multTransModSP[i], 6, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "err  " << num_devices <<  "  " << (clSetKernelArg(kernel_multTransModSP[i], 3, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS) << std::endl;
			std::cout << "Failed to create kernel Args for kernel " << i << "!" << std::endl;
			return 0.0;
		}

		offset += global;
	}

	cl_event clTimings[MAX_OCL_DEVICE_COUNT];
	cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

	// enqueue kernels
	for (size_t i = 0; i < num_devices; i++)
	{
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multTransModSP[i], 1, NULL, &global, &local, 0, NULL, &clTimings[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
	}

	// read data back
	offset = 0;
	for (size_t i = 0; i < num_devices; i++)
	{
		err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(float)*offset, sizeof(float)*global, &ptrGlobalResult[offset], 0, NULL, &GPUDone[i]);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue read buffer command (mult)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
	}

	// sync GPUs
	clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
	for (size_t i = 0; i < num_devices; i++)
	{
		double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

	// clean up
	for (size_t i = 0; i < num_devices; i++)
	{
		clReleaseMemObject(clSource[i]);
		clReleaseMemObject(clResult[i]);
	}

	isFirstTimeMultTransModSP = false;

	return time;
}

double OCLKernels::multModSPOCL(float* ptrAlpha, float* ptrData, float* ptrLevel, float* ptrIndex, float* ptrResult, size_t result_size, size_t storageSize, size_t dims, size_t gpu_partition)
{
	double time = 0.0;

	if (isFirstTimeMultModSP)
	{
		std::stringstream stream_program_src;

		stream_program_src << "__kernel void multModSPOCL(__global float* ptrAlpha," << std::endl;
		stream_program_src << "						__global float* ptrData," << std::endl;
		stream_program_src << "						__global float* ptrLevel," << std::endl;
		stream_program_src << "						__global float* ptrIndex," << std::endl;
		stream_program_src << "						__global float* ptrResult," << std::endl;
		stream_program_src << "						uint fastStorageSize," << std::endl;
		stream_program_src << "						uint storageSize," << std::endl;
		stream_program_src << "						uint offset)" << std::endl;
		stream_program_src << "{" << std::endl;
		stream_program_src << "	int globalIdx = get_global_id(0);" << std::endl;
		stream_program_src << "	int localIdx = get_local_id(0);" << std::endl;
		stream_program_src << "	globalIdx = globalIdx + offset;" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	__local float locLevel[" << dims * OCL_DATAPREFETCH_SIZE_SP << "];" << std::endl;
		stream_program_src << "	__local float locIndex[" << dims * OCL_DATAPREFETCH_SIZE_SP << "];" << std::endl;
		stream_program_src << "	__local float locAlpha[" << OCL_DATAPREFETCH_SIZE_SP << "];" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	float eval, index_calc, abs, last, localSupport, curSupport;" << std::endl << std::endl;
		stream_program_src << "	float myResult = 0.0;" << std::endl << std::endl;
		stream_program_src << "	// Create registers for the data" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "	float data_" << d << " = ptrData[(globalIdx*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (fast ones, with cache)" << std::endl;
		stream_program_src << "	for(int j = 0; j < fastStorageSize; j+=" << OCL_DATAPREFETCH_SIZE_SP << ")" << std::endl;
		stream_program_src << "	{" << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		locLevel[(localIdx*" << dims << ")+"<< d << "] = ptrLevel[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
			stream_program_src << "		locIndex[(localIdx*" << dims << ")+"<< d << "] = ptrIndex[((j+localIdx)*" << dims << ")+" << d << "];" << std::endl;
		}
		stream_program_src << "		locAlpha[localIdx] = ptrAlpha[j+localIdx];" << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		for(int k = 0; k < " << OCL_DATAPREFETCH_SIZE_SP << "; k++)" << std::endl;
		stream_program_src << "		{" << std::endl;
		stream_program_src << "			curSupport = locAlpha[k];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "			if ((locLevel[(k*" << dims << ")+" << d << "]) == 2.0f)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= 1.0f;" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((locIndex[(k*" << dims << ")+" << d << "]) == 1.0f)" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(2.0f - ( (locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << ") ), 0.0f) ;" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else if ((locIndex[(k*" << dims << ")+" << d << "]) == ((locLevel[(k*" << dims << ")+" << d << "]) - 1.0f) )" << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(( (locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (locIndex[(k*" << dims << ")+" << d << "]) + 1.0f, 0.0f);" << std::endl;
			stream_program_src << "			}" << std::endl;
			stream_program_src << "			else " << std::endl;
			stream_program_src << "			{" << std::endl;
			stream_program_src << "				curSupport *= max(1.0f - fabs( ( (locLevel[(k*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (locIndex[(k*" << dims << ")+" << d << "]) ), 0.0f);" << std::endl;
			stream_program_src << "			}" << std::endl;
		}
		stream_program_src << "			myResult += curSupport;" << std::endl;
		stream_program_src << "		}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "		barrier(CLK_LOCAL_MEM_FENCE);" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	// Iterate over all grid points (slow ones, without cache)" << std::endl;
		stream_program_src << "	for(int m = fastStorageSize; m < storageSize; m++)" << std::endl;
		stream_program_src << "	{" << std::endl;
		stream_program_src << "		curSupport = ptrAlpha[m];" << std::endl << std::endl;
		for (size_t d = 0; d < dims; d++)
		{
			stream_program_src << "		if ((ptrLevel[(m*" << dims << ")+" << d << "]) == 2.0f)" << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= 1.0f;" << std::endl;
			stream_program_src << "		}" << std::endl;
			stream_program_src << "		else if ((ptrIndex[(m*" << dims << ")+" << d << "]) == 1.0f)" << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= max(2.0f - ( (ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << ") ), 0.0f) ;" << std::endl;
			stream_program_src << "		}" << std::endl;
			stream_program_src << "		else if ((ptrIndex[(m*" << dims << ")+" << d << "]) == ((ptrLevel[(m*" << dims << ")+" << d << "]) - 1.0f) )" << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= max(( (ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (ptrIndex[(m*" << dims << ")+" << d << "]) + 1.0f, 0.0f);" << std::endl;
			stream_program_src << "		}" << std::endl;
			stream_program_src << "		else " << std::endl;
			stream_program_src << "		{" << std::endl;
			stream_program_src << "			curSupport *= max(1.0 - fabs( ( (ptrLevel[(m*" << dims << ")+" << d << "]) * (data_" << d << ") ) - (ptrIndex[(m*" << dims << ")+" << d << "]) ), 0.0f);" << std::endl;
			stream_program_src << "		}" << std::endl;
		}
		stream_program_src << "		myResult += curSupport;" << std::endl;
		stream_program_src << "	}" << std::endl;
		stream_program_src << std::endl;
		stream_program_src << "	ptrResult[globalIdx] = myResult;" << std::endl;
		stream_program_src << "}" << std::endl;

		std::string program_src = stream_program_src.str();

		//std::cout << program_src << std::endl;

	    // setting the program
	    const char* kernel_src = program_src.c_str();
	    program_multModSP = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &err);
	    if (err != CL_SUCCESS)
		{
	    	std::cout << "Failed to create program! Error Code: " << err << std::endl;
	    	return 0.0;
		}

	    // compiling the program
	    err = clBuildProgram(program_multModSP, 0, NULL,  "-cl-finite-math-only -cl-strict-aliasing -cl-fast-relaxed-math", NULL, NULL);

	    if (err != CL_SUCCESS)
	    {
	    	std::cout << "OpenCL Build Error. Error Code: " << err << std::endl;

	    	size_t len;
	    	char buffer[2048];

	    	// get the build log
	    	clGetProgramBuildInfo(program_multModSP, device_ids[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

	    	std::cout << "--- Build Log ---" << std::endl << buffer << std::endl;
	    	return 0.0;
	    }

	    // creating the kernels
	    for (size_t i = 0; i < num_devices; i++)
	    {
			kernel_multModSP[i] = clCreateKernel(program_multModSP, "multModSPOCL", &err);
			if (err != CL_SUCCESS)
			{
				std::cout << "Failed to create kernel! Error Code: " << err << std::endl;
				return 0.0;
			}
	    }
	}

	if (isFirstTimeMultModSP && isFirstTimeMultTransModSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clLevelSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrLevel, NULL);
			clIndexSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*storageSize, ptrIndex, NULL);
		}
	}

	if (isVeryFirstTimeSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clDataSP[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*dims*result_size, ptrData, NULL);
		}
		isVeryFirstTimeSP = false;
	}

	cl_mem clAlpha[MAX_OCL_DEVICE_COUNT], clResult[MAX_OCL_DEVICE_COUNT];

	for(size_t i = 0; i < num_devices; i++)
	{
		clAlpha[i] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*storageSize, ptrAlpha, NULL);
		clResult[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*gpu_partition, NULL, NULL);
	}

	size_t global = gpu_partition/num_devices;
    size_t local = OCL_DATAPREFETCH_SIZE_SP;
    size_t offset = 0;

	size_t oclStorageSize = storageSize/OCL_DATAPREFETCH_SIZE_SP;
    oclStorageSize *= OCL_DATAPREFETCH_SIZE_SP;

    cl_uint clFastStorageSize = (cl_uint)(oclStorageSize);
    cl_uint clStorageSize = (cl_uint)(storageSize);
    cl_uint clOffsets[MAX_OCL_DEVICE_COUNT];

    for (size_t i = 0; i < num_devices; i++)
    {
    	clOffsets[i] = (cl_uint)offset;
		// set kernel arguments
		if ( clSetKernelArg(kernel_multModSP[i], 0, sizeof(cl_mem), &clAlpha[i]) ||
				clSetKernelArg(kernel_multModSP[i], 1, sizeof(cl_mem), &clDataSP[i]) ||
				clSetKernelArg(kernel_multModSP[i], 2, sizeof(cl_mem), &clLevelSP[i]) ||
				clSetKernelArg(kernel_multModSP[i], 3, sizeof(cl_mem), &clIndexSP[i]) ||
				clSetKernelArg(kernel_multModSP[i], 4, sizeof(cl_mem), &clResult[i]) ||
				clSetKernelArg(kernel_multModSP[i], 5, sizeof(cl_uint), &clFastStorageSize) ||
				clSetKernelArg(kernel_multModSP[i], 6, sizeof(cl_uint), &clStorageSize) ||
				clSetKernelArg(kernel_multModSP[i], 7, sizeof(cl_uint), &clOffsets[i]) != CL_SUCCESS)
		{
			std::cout << "Failed to create kernel Args!" << std::endl;
			return 0.0;
		}
		offset += global;
    }

    cl_event clTimings[MAX_OCL_DEVICE_COUNT];
    cl_event GPUDone[MAX_OCL_DEVICE_COUNT];

    // enqueue kernel
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueNDRangeKernel(command_queue[i], kernel_multModSP[i], 1, NULL, &global, &local, 0, NULL, &(clTimings[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue kernel command! Error Code: " << err << std::endl;
			return 0.0;
		}
    }

    // read data back
    offset = 0;
    for (size_t i = 0; i < num_devices; i++)
    {
		err = clEnqueueReadBuffer(command_queue[i], clResult[i], CL_FALSE, sizeof(float)*offset, sizeof(float)*global, &(ptrResult[offset]), 0, NULL, &(GPUDone[i]));
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to enqueue read buffer command (multTrans)! Error Code: " << err << std::endl;
			return 0.0;
		}
		offset += global;
    }

    // sync GPUs
    clWaitForEvents(num_devices, GPUDone);

	// determine kernel execution time
    for (size_t i = 0; i < num_devices; i++)
	{
    	double tmpTime;
		cl_ulong startTime, endTime;
		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &startTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read start-time from command queue! Error Code: " << err << std::endl;
		}

		err = clGetEventProfilingInfo(clTimings[i], CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
		if (err != CL_SUCCESS)
		{
			std::cout << "Failed to read end-time from command queue! Error Code: " << err << std::endl;
		}

		tmpTime = (double)(endTime - startTime);
		tmpTime *= 1e-9;

		if (tmpTime > time)
		{
			time = tmpTime;
		}
	}

    // clean up
    for (size_t i = 0; i < num_devices; i++)
    {
    	clReleaseMemObject(clAlpha[i]);
    	clReleaseMemObject(clResult[i]);
    }

    isFirstTimeMultModSP = false;

	return time;
}

void OCLKernels::resetKernels()
{
	// linear float
	if (!isFirstTimeMultSP)
	{
	    clReleaseProgram(program_multSP);
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseKernel(kernel_multSP[i]);
	    }

	    isFirstTimeMultSP = true;
	}

	if (!isFirstTimeMultTransSP)
	{
	    clReleaseProgram(program_multTransSP);
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clLevelSP[i]);
	    	clReleaseMemObject(clIndexSP[i]);
	    	clReleaseKernel(kernel_multTransSP[i]);
	    }

	    isFirstTimeMultTransSP = true;
	}

	// linear double
	if (!isFirstTimeMultDP)
	{
	    clReleaseProgram(program_multDP);
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseKernel(kernel_multDP[i]);
	    }

	    isFirstTimeMultDP = true;
	}

	if (!isFirstTimeMultTransDP)
	{
	    clReleaseProgram(program_multTransDP);
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clLevelDP[i]);
	    	clReleaseMemObject(clIndexDP[i]);
	    	clReleaseKernel(kernel_multTransDP[i]);
	    }

	    isFirstTimeMultTransDP = true;
	}

	// modlinear double
	if (!isFirstTimeMultModDP)
	{
		clReleaseProgram(program_multModDP);
		for (size_t i = 0; i < num_devices; i++)
		{
			clReleaseKernel(kernel_multModDP[i]);
		}

		isFirstTimeMultModDP = true;
	}

	if (!isFirstTimeMultTransModDP)
	{
	    clReleaseProgram(program_multTransModDP);
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clLevelDP[i]);
	    	clReleaseMemObject(clIndexDP[i]);
	    	clReleaseKernel(kernel_multTransModDP[i]);
	    }

	    isFirstTimeMultTransModDP = true;
	}

	// modlinear float
    if (!isFirstTimeMultModSP)
    {
        clReleaseProgram(program_multModSP);
        for (size_t i = 0; i < num_devices; i++)
        {
            clReleaseKernel(kernel_multModSP[i]);
        }

        isFirstTimeMultModSP = true;
    }

	if (!isFirstTimeMultTransModSP)
	{
	    clReleaseProgram(program_multTransModSP);
	    for (size_t i = 0; i < num_devices; i++)
	    {
	    	clReleaseMemObject(clLevelSP[i]);
	    	clReleaseMemObject(clIndexSP[i]);
	    	clReleaseKernel(kernel_multTransModSP[i]);
	    }

	    isFirstTimeMultTransModSP = true;
	}
}

void OCLKernels::resetData()
{
	if (!isVeryFirstTimeSP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clReleaseMemObject(clDataSP[i]);
		}

		isVeryFirstTimeSP = true;
	}

	if (!isVeryFirstTimeDP)
	{
		for (size_t i = 0; i < num_devices; i++)
		{
			clReleaseMemObject(clDataDP[i]);
		}

		isVeryFirstTimeDP = true;
	}
}

}

}
