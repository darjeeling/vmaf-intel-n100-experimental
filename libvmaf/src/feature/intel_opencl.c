/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental Linux/Intel OpenCL transport for selected VMAF v1 operations.
 * The queue is in-order and every API call completes host-buffer use before
 * returning. No cl_mem/VAAPI interoperability or zero-copy is claimed.
 */
#define _POSIX_C_SOURCE 200809L
#include "intel_opencl.h"
#include "opencl_abi.h"
#include "intel_opencl_source.h"
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
_Static_assert(sizeof(void *) == 8, "Experimental backend requires Linux LP64");
_Static_assert(sizeof(long) == 8, "OpenCL long must match a 64-bit host long");

struct VmafIntelOcl {
    VmafClApi cl;
    void *loader;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernels[3];
    cl_mem input, temp, output, partials;
    unsigned w, h, bpc, widths[4], heights[4];
    size_t pixels, input_bytes, offsets[4], elements, output_bytes, partial_count;
    void *host;
    int strict, failed, is_adm;
    uint64_t jobs, upload_bytes, download_bytes, elapsed_ns;
};
static uint64_t now_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}
static const char *feature_name(const VmafIntelOcl *s) { return s->is_adm ? "adm" : "motion"; }
static int fail_run(VmafIntelOcl *s, const char *step, int error) {
    if (!s->failed) fprintf(stderr, "[vmaf-intel-opencl] feature=%s failed step=%s code=%d action=%s\n",
                           feature_name(s), step, error, s->strict ? "error" : "cpu-fallback");
    s->failed = 1;
    if (s->queue) s->cl.Finish(s->queue);
    return s->strict ? -EIO : 0;
}
static int selected_feature(const char *name) {
    const char *value = getenv("VMAF_INTEL_OPENCL_FEATURES");
    if (!value) return 1;
    /* Explicit finite choices catch typos rather than silently skipping GPU use. */
    if (!strcmp(value,"motion,adm") || !strcmp(value,"adm,motion")) return 1;
    if (!strcmp(value,"motion") || !strcmp(value,"adm")) return !strcmp(value,name);
    return -EINVAL;
}
void vmaf_intel_ocl_destroy(VmafIntelOcl **ctx) {
    if (!ctx || !*ctx) return;
    VmafIntelOcl *s = *ctx;
    if (s->queue) s->cl.Finish(s->queue);
    if (s->input) s->cl.ReleaseMemObject(s->input);
    if (s->temp) s->cl.ReleaseMemObject(s->temp);
    if (s->output) s->cl.ReleaseMemObject(s->output);
    if (s->partials) s->cl.ReleaseMemObject(s->partials);
    for (unsigned i=0;i<3;i++) if(s->kernels[i]) s->cl.ReleaseKernel(s->kernels[i]);
    if(s->program) s->cl.ReleaseProgram(s->program);
    if(s->queue) s->cl.ReleaseCommandQueue(s->queue);
    if(s->context) s->cl.ReleaseContext(s->context);
    if(s->jobs) fprintf(stderr,"[vmaf-intel-opencl] feature=%s jobs=%"PRIu64" upload_bytes=%"PRIu64
                        " download_bytes=%"PRIu64" work_ms=%.3f\n",feature_name(s),s->jobs,
                        s->upload_bytes,s->download_bytes,s->elapsed_ns/1000000.0);
    free(s->host);
    if(s->loader) dlclose(s->loader);
    free(s);*ctx=NULL;
}
static int choose_device(VmafIntelOcl *s) {
    unsigned long requested=0;
    const char *v=getenv("VMAF_INTEL_OPENCL_DEVICE");
    if(v) {
        if(*v<'0' || *v>'9') return -EINVAL;
        char *end;errno=0;requested=strtoul(v,&end,10);
        if(errno || *end || requested>1024) return -EINVAL;
    }
    cl_uint np=0;
    cl_int err=s->cl.GetPlatformIDs(0,NULL,&np);
    if(err || !np || np>64) return -ENODEV;
    cl_platform_id *platforms=calloc(np,sizeof(*platforms));
    if(!platforms) return -ENOMEM;
    err=s->cl.GetPlatformIDs(np,platforms,NULL);
    unsigned long index=0;
    if(!err) for(cl_uint p=0;p<np && !s->device;p++) {
        cl_uint nd=0;
        if(s->cl.GetDeviceIDs(platforms[p],CL_DEVICE_TYPE_GPU,0,NULL,&nd) || !nd || nd>128) continue;
        cl_device_id *devices=calloc(nd,sizeof(*devices));
        if(!devices) {free(platforms);return -ENOMEM;}
        if(!s->cl.GetDeviceIDs(platforms[p],CL_DEVICE_TYPE_GPU,nd,devices,NULL))
            for(cl_uint d=0;d<nd;d++) {
                cl_uint vendor=0;
                if(s->cl.GetDeviceInfo(devices[d],CL_DEVICE_VENDOR_ID,sizeof(vendor),&vendor,NULL)) continue;
                if(vendor==0x8086 && index++==requested) {s->device=devices[d];break;}
            }
        free(devices);
    }
    free(platforms);return s->device ? 0 : -ENODEV;
}
static cl_mem make_buffer(VmafIntelOcl *s, size_t bytes, cl_mem_flags flags,
                          cl_ulong max_alloc, cl_int *err) {
    if(bytes>max_alloc) {*err=-ENOMEM;return NULL;}
    return s->cl.CreateBuffer(s->context,flags,bytes,NULL,err);
}
int vmaf_intel_ocl_create(VmafIntelOcl **out, const char *feature,
                         unsigned w, unsigned h, unsigned bpc) {
    if(!out || !feature) return -EINVAL;
    *out=NULL;
    if(strcmp(feature,"adm") && strcmp(feature,"motion")) return -EINVAL;
    const char *mode=getenv("VMAF_INTEL_OPENCL");
    if(!mode || !strcmp(mode,"off")) return 0;
    int strict=!strcmp(mode,"required");
    if(!strict && strcmp(mode,"auto")) return -EINVAL;
    int use=selected_feature(feature);
    if(use<=0) return use;
    int is_adm=!strcmp(feature,"adm");
    if((bpc!=8 && bpc!=10 && bpc!=12) || w<(is_adm?48u:3u) || h<(is_adm?48u:3u) || w>8192 || h>4320) {
        fprintf(stderr,"[vmaf-intel-opencl] unsupported geometry/bpc: %ux%u/%u feature=%s\n",w,h,bpc,feature);
        return strict ? -ENOTSUP : 0;
    }
    const uint16_t endian=1;
    if(*(const uint8_t *)&endian!=1) return -ENOTSUP;
    VmafIntelOcl *s=calloc(1,sizeof(*s));
    if(!s) return -ENOMEM;
    s->strict=strict;s->is_adm=is_adm;s->w=w;s->h=h;s->bpc=bpc;
    s->pixels=(size_t)w*h;s->input_bytes=2*s->pixels*(bpc==8?1:2);
    s->partial_count=(s->pixels+255)/256;
    unsigned iw=w,ih=h;
    for(unsigned i=0;i<4;i++) {
        s->offsets[i]=s->elements;iw=(iw+1)/2;ih=(ih+1)/2;
        s->widths[i]=iw;s->heights[i]=ih;s->elements+=(size_t)4*iw*ih;
    }
    s->output_bytes=is_adm ? 2*s->elements*sizeof(int32_t) : s->pixels*sizeof(uint32_t);
    cl_int err=-ENODEV;
    const char *step="load OpenCL loader";
    s->loader=dlopen("libOpenCL.so.1",RTLD_NOW|RTLD_LOCAL);
    if(!s->loader) goto fail;
#define LOAD_CL(ret,name,args) do { \
        void *symbol=dlsym(s->loader,"cl" #name); \
        _Static_assert(sizeof(s->cl.name)==sizeof(symbol),"POSIX function pointers required"); \
        if(!symbol) {step="resolve cl" #name;goto fail;} \
        memcpy(&s->cl.name,&symbol,sizeof(symbol)); \
    } while(0);
    CL_API_FUNCTIONS(LOAD_CL)
#undef LOAD_CL
    step="choose Intel GPU";
    err=choose_device(s);if(err) goto fail;
    cl_bool little=0,unified=0;
    cl_ulong max_alloc=0;
    err=s->cl.GetDeviceInfo(s->device,CL_DEVICE_ENDIAN_LITTLE,sizeof(little),&little,NULL);
    if(err || !little) {err=-ENOTSUP;goto fail;}
    err=s->cl.GetDeviceInfo(s->device,CL_DEVICE_MAX_MEM_ALLOC_SIZE,sizeof(max_alloc),&max_alloc,NULL);
    if(err) goto fail;
    (void)s->cl.GetDeviceInfo(s->device,CL_DEVICE_HOST_UNIFIED_MEMORY,sizeof(unified),&unified,NULL);
    char name[256]={0};
    (void)s->cl.GetDeviceInfo(s->device,CL_DEVICE_NAME,sizeof(name)-1,name,NULL);
    step="create context";
    s->context=s->cl.CreateContext(NULL,1,&s->device,NULL,NULL,&err);if(!s->context || err)goto fail;
    step="create queue";
    s->queue=s->cl.CreateCommandQueue(s->context,s->device,0,&err);if(!s->queue || err)goto fail;
    const char *source=vmaf_intel_opencl_source;size_t source_len=sizeof(vmaf_intel_opencl_source)-1;
    step="create program";
    s->program=s->cl.CreateProgramWithSource(s->context,1,&source,&source_len,&err);if(!s->program || err)goto fail;
    step="build OpenCL C 1.2 program";
    err=s->cl.BuildProgram(s->program,1,&s->device,"-cl-std=CL1.2",NULL,NULL);
    if(err) {
        size_t n=0;
        s->cl.GetProgramBuildInfo(s->program,s->device,CL_PROGRAM_BUILD_LOG,0,NULL,&n);
        if(n && n<1024*1024) {
            char *log=calloc(n+1,1);
            if(log) {s->cl.GetProgramBuildInfo(s->program,s->device,CL_PROGRAM_BUILD_LOG,n,log,NULL);
                fprintf(stderr,"[vmaf-intel-opencl] build log: %s\n",log);free(log);}
        }
        goto fail;
    }
    const char *motion_names[]={"motion_vertical","motion_horizontal","motion_reduce"};
    const char *adm_names[]={"adm_vertical0","adm_vertical_s","adm_horizontal"};
    for(unsigned i=0;i<3;i++) {
        step=is_adm?adm_names[i]:motion_names[i];
        s->kernels[i]=s->cl.CreateKernel(s->program,step,&err);if(!s->kernels[i] || err)goto fail;
    }
    step="allocate persistent buffers";
    s->input=make_buffer(s,s->input_bytes,CL_MEM_READ_ONLY,max_alloc,&err);if(!s->input || err)goto fail;
    size_t tmp_bytes=is_adm ? (size_t)4*w*((h+1)/2)*sizeof(int32_t) : s->pixels*sizeof(int32_t);
    s->temp=make_buffer(s,tmp_bytes,CL_MEM_READ_WRITE,max_alloc,&err);if(!s->temp || err)goto fail;
    s->output=make_buffer(s,s->output_bytes,CL_MEM_READ_WRITE,max_alloc,&err);if(!s->output || err)goto fail;
    size_t host_bytes=is_adm?s->output_bytes:s->partial_count*sizeof(cl_ulong);
    if(!is_adm) {s->partials=make_buffer(s,host_bytes,CL_MEM_READ_WRITE,max_alloc,&err);if(!s->partials || err)goto fail;}
    s->host=malloc(host_bytes);if(!s->host){err=-ENOMEM;goto fail;}
    fprintf(stderr,"[vmaf-intel-opencl] enabled feature=%s device=%s unified_memory=%u mode=%s size=%ux%u bpc=%u\n",
            feature,name,(unsigned)unified,mode,w,h,bpc);
    *out=s;return 0;
fail:
    fprintf(stderr,"[vmaf-intel-opencl] init feature=%s step=%s code=%d action=%s\n",feature,step,err,strict?"error":"cpu-fallback");
    vmaf_intel_ocl_destroy(&s);
    /* Bad selector/configuration is a user error, even in auto mode. */
    return strict || err==-EINVAL ? (err<0?err:-EIO) : 0;
}
static int upload_pair(VmafIntelOcl *s,const uint8_t *a,ptrdiff_t as,
                        const uint8_t *b,ptrdiff_t bs) {
    size_t row=(size_t)s->w*(s->bpc==8?1:2);
    if(!a || !b || as<(ptrdiff_t)row || bs<(ptrdiff_t)row) return -EINVAL;
    if((uint64_t)as>SIZE_MAX/s->h || (uint64_t)bs>SIZE_MAX/s->h) return -EINVAL;
    const uint8_t *ptr[2]={a,b};size_t strides[2]={(size_t)as,(size_t)bs};
    size_t host_origin[3]={0,0,0},region[3]={row,s->h,1};
    for(unsigned i=0;i<2;i++) {
        size_t origin[3]={0,0,i};
        cl_int err=s->cl.EnqueueWriteBufferRect(s->queue,s->input,CL_TRUE,origin,host_origin,region,
            row,row*s->h,strides[i],strides[i]*s->h,ptr[i],0,NULL,NULL);
        if(err)return err;
    }
    s->upload_bytes+=s->input_bytes;return 0;
}
static int arg(VmafIntelOcl *s,cl_kernel k,unsigned i,size_t n,const void *value) {
    return s->cl.SetKernelArg(k,i,n,value);
}
#define SETARG(i,v) do { err=arg(s,k,(i),sizeof(v),&(v));if(err)goto error; } while(0)
static int launch(VmafIntelOcl *s,cl_kernel k,unsigned dims,size_t x,size_t y) {
    const size_t global[2]={x,y};
    return s->cl.EnqueueNDRangeKernel(s->queue,k,dims,NULL,global,NULL,0,NULL,NULL);
}
int vmaf_intel_ocl_motion(VmafIntelOcl *s,const uint8_t *prev,ptrdiff_t ps,
                          const uint8_t *cur,ptrdiff_t cs,uint64_t *sad) {
    if(!s)return 0;
    if(s->failed)return s->strict?-EIO:0;
    if(s->is_adm || !sad)return -EINVAL;
    uint64_t start=now_ns();
    int err=upload_pair(s,prev,ps,cur,cs);if(err)goto error;
    cl_kernel k=s->kernels[0];
    SETARG(0,s->input);SETARG(1,s->temp);SETARG(2,s->w);SETARG(3,s->h);SETARG(4,s->bpc);
    err=launch(s,k,2,s->w,s->h);if(err)goto error;
    k=s->kernels[1];
    SETARG(0,s->temp);SETARG(1,s->output);SETARG(2,s->w);SETARG(3,s->h);
    err=launch(s,k,2,s->w,s->h);if(err)goto error;
    k=s->kernels[2];cl_ulong count=s->pixels;
    SETARG(0,s->output);SETARG(1,s->partials);SETARG(2,count);
    err=launch(s,k,1,s->partial_count,1);if(err)goto error;
    size_t bytes=s->partial_count*sizeof(cl_ulong);
    err=s->cl.EnqueueReadBuffer(s->queue,s->partials,CL_TRUE,0,bytes,s->host,0,NULL,NULL);if(err)goto error;
    const cl_ulong *p=s->host;uint64_t total=0;
    for(size_t i=0;i<s->partial_count;i++)total+=p[i];
    *sad=total;s->download_bytes+=bytes;s->jobs++;s->elapsed_ns+=now_ns()-start;return 1;
error:return fail_run(s,"motion",err);
}
int vmaf_intel_ocl_adm(VmafIntelOcl *s,const uint8_t *ref,ptrdiff_t rs,
                       const uint8_t *dis,ptrdiff_t ds) {
    if(!s)return 0;
    if(s->failed)return s->strict?-EIO:0;
    if(!s->is_adm)return -EINVAL;
    uint64_t start=now_ns();
    int err=upload_pair(s,ref,rs,dis,ds);if(err)goto error;
    cl_uint w=s->w,h=s->h;
    cl_ulong frame=s->elements;
    for(cl_uint level=0;level<4;level++) {
        cl_kernel k=s->kernels[level?1:0];
        if(!level) {
            SETARG(0,s->input);SETARG(1,s->temp);SETARG(2,w);SETARG(3,h);SETARG(4,s->bpc);
        }else {
            cl_ulong previous=s->offsets[level-1];
            SETARG(0,s->output);SETARG(1,s->temp);SETARG(2,w);SETARG(3,h);SETARG(4,level);SETARG(5,previous);SETARG(6,frame);
        }
        err=launch(s,k,2,w,2*((h+1)/2));if(err)goto error;
        k=s->kernels[2];cl_ulong offset=s->offsets[level];
        SETARG(0,s->temp);SETARG(1,s->output);SETARG(2,w);SETARG(3,h);SETARG(4,level);SETARG(5,offset);SETARG(6,frame);
        err=launch(s,k,2,(w+1)/2,2*((h+1)/2));if(err)goto error;
        w=(w+1)/2;h=(h+1)/2;
    }
    err=s->cl.EnqueueReadBuffer(s->queue,s->output,CL_TRUE,0,s->output_bytes,s->host,0,NULL,NULL);if(err)goto error;
    s->download_bytes+=s->output_bytes;s->jobs++;s->elapsed_ns+=now_ns()-start;return 1;
error:return fail_run(s,"adm",err);
}
#undef SETARG
void vmaf_intel_ocl_adm_copy16(const VmafIntelOcl *s,unsigned frame,
                              int16_t *a,int16_t *v,int16_t *h,int16_t *d,size_t stride) {
    assert(s && s->is_adm && !s->failed && s->jobs && frame<2 && stride>=s->widths[0]);
    int16_t *dst[4]={a,v,h,d};size_t w=s->widths[0],height=s->heights[0],plane=w*height;
    const int32_t *src=(const int32_t *)s->host+frame*s->elements;
    for(unsigned b=0;b<4;b++)for(size_t y=0;y<height;y++)for(size_t x=0;x<w;x++)
        dst[b][y*stride+x]=(int16_t)src[b*plane+y*w+x];
}
void vmaf_intel_ocl_adm_copy32(const VmafIntelOcl *s,unsigned frame,unsigned level,
                              int32_t *a,int32_t *v,int32_t *h,int32_t *d,size_t stride) {
    assert(s && s->is_adm && !s->failed && s->jobs && frame<2 && level<4 && stride>=s->widths[level]);
    int32_t *dst[4]={a,v,h,d};size_t w=s->widths[level],height=s->heights[level],plane=w*height;
    const int32_t *src=(const int32_t *)s->host+frame*s->elements+s->offsets[level];
    for(unsigned b=0;b<4;b++)for(size_t y=0;y<height;y++)
        memcpy(dst[b]+y*stride,src+b*plane+y*w,w*sizeof(int32_t));
}
