/* Differential oracle for the isolated Gaussian/decimation experiment. */
#define _POSIX_C_SOURCE 200112L
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "convolution.h"

int vmaf_floorn(int n, int m) { return n / m * m; }
int vmaf_ceiln(int n, int m) { return (n + m - 1) / m * m; }

static float *alloc_floats(size_t n)
{
    void *p = NULL;
    if (posix_memalign(&p, 32, n * sizeof(float))) abort();
    memset(p, 0xa5, n * sizeof(float));
    return p;
}

static uint32_t random_state = UINT32_C(0x97abcde1);
static uint32_t next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

int main(void)
{
    const int sizes[][2] = {{80,80}, {81,83}, {95,97}, {127,129},
                           {960,540}, {961,541}, {1920,1080}};
    float filter[9], total = 0;
    for (int k = 0; k < 9; k++) {
        filter[k] = expf(-0.5f * (k - 4) * (k - 4) / (3.6f * 3.6f));
        total += filter[k];
    }
    for (int k = 0; k < 9; k++) filter[k] /= total;

    size_t checked = 0;
    int cases = 0;
    for (size_t shape = 0; shape < sizeof(sizes) / sizeof(*sizes); shape++) {
        const int w = sizes[shape][0], h = sizes[shape][1];
        for (int pad = 0; pad <= 16; pad += 8) {
            const int stride = vmaf_ceiln(w, 8) + pad;
            const int tmp_stride = vmaf_ceiln(w, 8);
            const size_t len = (size_t)stride * h;
            float *src = alloc_floats(len);
            float *old = alloc_floats(len);
            float *old_tmp = alloc_floats((size_t)tmp_stride * h);
            float *inplace = alloc_floats(len);
            float *output = alloc_floats(len);
            float *row = alloc_floats(tmp_stride);
            float *tmp = alloc_floats(tmp_stride);
            for (int pattern = 0; pattern < 4; pattern++) {
                for (size_t i = 0; i < len; i++) {
                    if (pattern == 0) src[i] = 0;
                    if (pattern == 1) src[i] = 255;
                    if (pattern == 2) src[i] = (int)(next_random() & 4095) / 16.f - 128.f;
                    if (pattern == 3) src[i] = (i % 17 == 0) ? 255.f : -128.f;
                }
                memcpy(inplace, src, len * sizeof(float));
                convolution_f32_avx_s(filter, 9, src, old, old_tmp, w, h,
                                     stride, stride);
                convolution_f32_avx_s_dec16(filter, src, output, row, tmp,
                                           w, h, stride, stride);
                convolution_f32_avx_s_dec16(filter, inplace, inplace, row, tmp,
                                           w, h, stride, stride);
                for (int y = 0; y < h / 16; y++) {
                    for (int x = 0; x < w / 16; x++) {
                        const float *want = old + y * 16 * stride + x * 16;
                        const float *got = output + y * stride + x;
                        const float *alias = inplace + y * stride + x;
                        if (memcmp(want, got, sizeof(float)) ||
                            memcmp(want, alias, sizeof(float))) {
                            fprintf(stderr, "mismatch %dx%d stride=%d pattern=%d at %d,%d: %.9g %.9g %.9g\n",
                                    w, h, stride, pattern, x, y, *want, *got, *alias);
                            return 1;
                        }
                        checked++;
                    }
                }
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < stride; x++) {
                        if (y < h / 16 && x < w / 16) continue;
                        if (memcmp(src + y * stride + x,
                                   inplace + y * stride + x, sizeof(float))) {
                            fprintf(stderr, "write outside compact output\n");
                            return 1;
                        }
                    }
                }
                cases++;
            }
            free(src); free(old); free(old_tmp); free(inplace); free(output);
            free(row); free(tmp);
        }
    }
    printf("{\"cases\":%d,\"sampled_coefficients\":%zu,\"bitwise_equal\":true,\"inplace_equal\":true}\n",
           cases, checked);
    return 0;
}
