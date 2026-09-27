/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Private subset of the Khronos OpenCL 1.2 C ABI, Linux LP64 only.
 * Not a replacement public OpenCL SDK. No OpenCL symbols are exported.
 * Prototypes correspond to KhronosGroup/OpenCL-Headers CL/cl.h.
 */
#ifndef VMAF_PRIVATE_OPENCL_ABI_H
#define VMAF_PRIVATE_OPENCL_ABI_H
#include <stddef.h>
#include <stdint.h>
typedef int32_t cl_int;
typedef uint32_t cl_uint, cl_bool;
typedef uint64_t cl_ulong, cl_bitfield, cl_device_type, cl_mem_flags, cl_command_queue_properties;
typedef intptr_t cl_context_properties;
typedef struct _cl_platform_id *cl_platform_id;
typedef struct _cl_device_id *cl_device_id;
typedef struct _cl_context *cl_context;
typedef struct _cl_command_queue *cl_command_queue;
typedef struct _cl_mem *cl_mem;
typedef struct _cl_program *cl_program;
typedef struct _cl_kernel *cl_kernel;
typedef struct _cl_event *cl_event;
#define CL_SUCCESS 0
#define CL_TRUE 1
#define CL_DEVICE_TYPE_GPU (1ULL << 2)
#define CL_DEVICE_VENDOR_ID 0x1001
#define CL_DEVICE_MAX_MEM_ALLOC_SIZE 0x1010
#define CL_DEVICE_ENDIAN_LITTLE 0x1026
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_HOST_UNIFIED_MEMORY 0x1035
#define CL_MEM_READ_WRITE (1ULL << 0)
#define CL_MEM_READ_ONLY (1ULL << 2)
#define CL_PROGRAM_BUILD_LOG 0x1183
#define CL_API_FUNCTIONS(X) \
 X(cl_int, GetPlatformIDs, (cl_uint, cl_platform_id *, cl_uint *)) \
 X(cl_int, GetDeviceIDs, (cl_platform_id, cl_device_type, cl_uint, cl_device_id *, cl_uint *)) \
 X(cl_int, GetDeviceInfo, (cl_device_id, cl_uint, size_t, void *, size_t *)) \
 X(cl_context, CreateContext, (const cl_context_properties *, cl_uint, const cl_device_id *, void (*)(const char *, const void *, size_t, void *), void *, cl_int *)) \
 X(cl_command_queue, CreateCommandQueue, (cl_context, cl_device_id, cl_command_queue_properties, cl_int *)) \
 X(cl_program, CreateProgramWithSource, (cl_context, cl_uint, const char **, const size_t *, cl_int *)) \
 X(cl_int, BuildProgram, (cl_program, cl_uint, const cl_device_id *, const char *, void (*)(cl_program, void *), void *)) \
 X(cl_int, GetProgramBuildInfo, (cl_program, cl_device_id, cl_uint, size_t, void *, size_t *)) \
 X(cl_kernel, CreateKernel, (cl_program, const char *, cl_int *)) \
 X(cl_mem, CreateBuffer, (cl_context, cl_mem_flags, size_t, void *, cl_int *)) \
 X(cl_int, SetKernelArg, (cl_kernel, cl_uint, size_t, const void *)) \
 X(cl_int, EnqueueWriteBufferRect, (cl_command_queue, cl_mem, cl_bool, const size_t *, const size_t *, const size_t *, size_t, size_t, size_t, size_t, const void *, cl_uint, const cl_event *, cl_event *)) \
 X(cl_int, EnqueueNDRangeKernel, (cl_command_queue, cl_kernel, cl_uint, const size_t *, const size_t *, const size_t *, cl_uint, const cl_event *, cl_event *)) \
 X(cl_int, EnqueueReadBuffer, (cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *)) \
 X(cl_int, Finish, (cl_command_queue)) \
 X(cl_int, ReleaseMemObject, (cl_mem)) \
 X(cl_int, ReleaseKernel, (cl_kernel)) \
 X(cl_int, ReleaseProgram, (cl_program)) \
 X(cl_int, ReleaseCommandQueue, (cl_command_queue)) \
 X(cl_int, ReleaseContext, (cl_context))
typedef struct VmafClApi {
#define DECLARE_CL(ret, name, args) ret (*name) args;
    CL_API_FUNCTIONS(DECLARE_CL)
#undef DECLARE_CL
} VmafClApi;
#endif
