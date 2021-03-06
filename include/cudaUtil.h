#ifndef __TUM3D__CUDA_UTIL_H__
#define __TUM3D__CUDA_UTIL_H__


#include <cstdlib>
#include <cstdio>
#include <iostream>

#include <cuda_runtime.h>


// I know. This should not exist. It was intended to debug gpu memory allocations. Feel free to remove this.
template<class T>
static __inline__ __host__ cudaError_t cudaMalloc2(T **devPtr, size_t size)
{
	//size_t memFree = 0;
	//size_t memTotal = 0;
	//cudaMemGetInfo(&memFree, &memTotal);

	//std::cout << "cudaMalloc: " << float(size) / 1024.0f << "KB" << "\tAvailable: " << float(memFree) / (1024.0f * 1024.0f) << "MB" << std::endl;

	return cudaMalloc(devPtr, size);
}


#define cudaSafeCall(err)       __cudaSafeCall      (err, __FILE__, __LINE__)
#define cudaSafeCallNoSync(err) __cudaSafeCallNoSync(err, __FILE__, __LINE__)
#define cudaCheckMsg(msg)       __cudaGetLastError  (msg, __FILE__, __LINE__)
#ifdef _DEBUG
#define cudaSafeKernelCall(call) {call; __cudaSafeKernelCall(#call, __FILE__, __LINE__);}
#else
#define cudaSafeKernelCall(call) call;
#endif

#ifdef _DEBUG
#define CHECK_ERROR(err) (cudaSuccess != err || cudaSuccess != (err = cudaDeviceSynchronize()))
#else
#define CHECK_ERROR(err) (cudaSuccess != err)
#endif

inline void __cudaSafeCall(cudaError err, const char* file, const int line)
{
	if(CHECK_ERROR(err)) {
		fprintf(stderr, "%s(%i) : cudaSafeCall() Runtime API error : %s.\n", file, line, cudaGetErrorString(err));
#ifdef _DEBUG
		__debugbreak();
#endif
	}
}

inline void __cudaSafeCallNoSync(cudaError err, const char* file, const int line)
{
	if(cudaSuccess != err) {
		fprintf(stderr, "%s(%i) : cudaSafeCallNoSync() Runtime API error : %s.\n", file, line, cudaGetErrorString(err));
#ifdef _DEBUG
		__debugbreak();
#endif
	}
}

inline void __cudaSafeKernelCall(const char* call, const char* file, const int line)
{
	cudaError_t err = cudaGetLastError();
	if (CHECK_ERROR(err)) {
		fprintf(stderr, "%s(%i) : cudaSafeKernelCall() Runtime API error : %s.\n kernel call: %s\n", file, line, cudaGetErrorString(err), call);
#ifdef _DEBUG
		__debugbreak();
#endif
	}
}

inline void __cudaGetLastError(const char* errorMessage, const char* file, const int line)
{
	cudaError_t err = cudaGetLastError();
	if(CHECK_ERROR(err)) {
		fprintf(stderr, "%s(%i) : cudaCheckMsg() CUDA error : %s : (%d) %s.\n", file, line, errorMessage, (int)err, cudaGetErrorString(err));
#ifdef _DEBUG
		__debugbreak();
#endif
	}
}

#undef CHECK_ERROR


#endif
