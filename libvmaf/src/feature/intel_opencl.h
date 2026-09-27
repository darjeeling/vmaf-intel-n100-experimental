/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef VMAF_INTEL_OPENCL_H
#define VMAF_INTEL_OPENCL_H
#include <stddef.h>
#include <stdint.h>
typedef struct VmafIntelOcl VmafIntelOcl;
/* create: 0 success (including disabled, *ctx == NULL), negative failure.
 * compute: 1 GPU result ready, 0 CPU fallback required, negative strict failure.
 * Instances are per feature extractor and must not be used concurrently.
 */
int vmaf_intel_ocl_create(VmafIntelOcl **ctx, const char *feature,
                         unsigned w, unsigned h, unsigned bpc);
void vmaf_intel_ocl_destroy(VmafIntelOcl **ctx);
int vmaf_intel_ocl_motion(VmafIntelOcl *ctx, const uint8_t *previous,
                          ptrdiff_t previous_stride, const uint8_t *current,
                          ptrdiff_t current_stride, uint64_t *sad);
int vmaf_intel_ocl_adm(VmafIntelOcl *ctx, const uint8_t *ref, ptrdiff_t ref_stride,
                       const uint8_t *dis, ptrdiff_t dis_stride);
/* Only call after adm returns 1. Band order is a,v,h,d; stride is in elements. */
void vmaf_intel_ocl_adm_copy16(const VmafIntelOcl *ctx, unsigned frame,
                              int16_t *a, int16_t *v, int16_t *h, int16_t *d,
                              size_t stride);
void vmaf_intel_ocl_adm_copy32(const VmafIntelOcl *ctx, unsigned frame, unsigned level,
                              int32_t *a, int32_t *v, int32_t *h, int32_t *d,
                              size_t stride);
#endif
