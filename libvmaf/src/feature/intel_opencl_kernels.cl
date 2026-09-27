/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental integer OpenCL backend for Netflix VMAF v1.
 * Constants, boundary conditions and rounding follow Netflix/vmaf
 * 86da14d0306a138fd3f01319860b905169746516. See NOTICE.
 * No float, FP64, fast-math, atomics, subgroups or work-group barriers.
 */
__constant int motion_filter[5] = {3571, 16004, 26386, 16004, 3571};
__constant int adm_lo[4] = {15826, 27411, 7345, -4240};
__constant int adm_hi[4] = {-4240, -7345, 27411, -15826};

static int mirror_motion(int p, int n)
{
    return p < 0 ? -p : (p >= n ? 2 * n - p - 2 : p);
}
static int mirror_adm(int p, int n)
{
    /* Deliberately different right edge from motion. */
    return p < 0 ? -p : (p >= n ? 2 * n - p - 1 : p);
}
static int read_sample(__global const uchar *src, ulong p, uint bpc)
{
    return bpc == 8 ? (int)src[p] : (int)((__global const ushort *)src)[p];
}
__kernel void motion_vertical(__global const uchar *src,
                              __global int *tmp, uint w, uint h, uint bpc)
{
    uint x = (uint)get_global_id(0), y = (uint)get_global_id(1);
    if (x >= w || y >= h) return;
    ulong pixels = (ulong)w * h;
    long accum = 0;
    for (int k = 0; k < 5; k++) {
        ulong p = (ulong)mirror_motion((int)y - 2 + k, (int)h) * w + x;
        int diff = read_sample(src, p, bpc) - read_sample(src, pixels + p, bpc);
        accum += (long)motion_filter[k] * diff;
    }
    tmp[(ulong)y * w + x] = (int)((accum + ((long)1 << (bpc - 1))) >> bpc);
}
__kernel void motion_horizontal(__global const int *tmp,
                                __global uint *values, uint w, uint h)
{
    uint x = (uint)get_global_id(0), y = (uint)get_global_id(1);
    if (x >= w || y >= h) return;
    long accum = 0;
    for (int k = 0; k < 5; k++) {
        int col = mirror_motion((int)x - 2 + k, (int)w);
        accum += (long)motion_filter[k] * tmp[(ulong)y * w + col];
    }
    int val = (int)((accum + 32768) >> 16);
    values[(ulong)y * w + x] = (uint)(val < 0 ? -val : val);
}
__kernel void motion_reduce(__global const uint *values,
                            __global ulong *partials, ulong n)
{
    ulong group = get_global_id(0), start = group * 256;
    if (start >= n) return;
    ulong sum = 0;
    for (ulong p = start; p < start + 256 && p < n; p++) sum += values[p];
    partials[group] = sum;
}

/* Each dispatch includes both reference and distorted images along dimension 1.
 * tmp layout: [frame][vertical low/high][y][x].
 * pyramid layout: [frame][level][a,v,h,d][y][x], tightly packed per level.
 * The low-pass band of level 0 is sign-extended from int16 to int32, not shifted.
 */
__kernel void adm_vertical0(__global const uchar *src, __global int *tmp,
                            uint w, uint h, uint bpc)
{
    uint x = (uint)get_global_id(0), gy = (uint)get_global_id(1);
    uint oh = (h + 1) / 2, frame = gy / oh, y = gy % oh;
    if (x >= w || frame >= 2) return;
    long lo = -(long)46342 * ((long)1 << (bpc - 1)), hi = 0;
    for (int k = 0; k < 4; k++) {
        int row = mirror_adm(2 * (int)y - 1 + k, (int)h);
        int v = read_sample(src, (ulong)frame * w * h + (ulong)row * w + x, bpc);
        lo += (long)adm_lo[k] * v;
        hi += (long)adm_hi[k] * v;
    }
    long rnd = (long)1 << (bpc - 1);
    ulong plane = (ulong)w * oh, p = (ulong)frame * 2 * plane + (ulong)y * w + x;
    tmp[p]         = (int)(short)((lo + rnd) >> bpc);
    tmp[p + plane] = (int)(short)((hi + rnd) >> bpc);
}
__kernel void adm_vertical_s(__global const int *pyramid, __global int *tmp,
                             uint w, uint h, uint level,
                             ulong previous_offset, ulong frame_elements)
{
    uint x = (uint)get_global_id(0), gy = (uint)get_global_id(1);
    uint oh = (h + 1) / 2, frame = gy / oh, y = gy % oh;
    if (x >= w || frame >= 2) return;
    long lo = 0, hi = 0;
    ulong base = (ulong)frame * frame_elements + previous_offset;
    for (int k = 0; k < 4; k++) {
        int row = mirror_adm(2 * (int)y - 1 + k, (int)h);
        int v = pyramid[base + (ulong)row * w + x];
        lo += (long)adm_lo[k] * v;
        hi += (long)adm_hi[k] * v;
    }
    uint shift = level == 1 ? 0 : 16;
    long rnd = level == 1 ? 0 : 32768;
    ulong plane = (ulong)w * oh, p = (ulong)frame * 2 * plane + (ulong)y * w + x;
    tmp[p]         = (int)((lo + rnd) >> shift);
    tmp[p + plane] = (int)((hi + rnd) >> shift);
}
__kernel void adm_horizontal(__global const int *tmp, __global int *pyramid,
                             uint w, uint h, uint level,
                             ulong offset, ulong frame_elements)
{
    uint x = (uint)get_global_id(0), gy = (uint)get_global_id(1);
    uint ow = (w + 1) / 2, oh = (h + 1) / 2, frame = gy / oh, y = gy % oh;
    if (x >= ow || frame >= 2) return;
    ulong tp = (ulong)w * oh, base = (ulong)frame * 2 * tp + (ulong)y * w;
    long a = 0, v = 0, hh = 0, d = 0;
    for (int k = 0; k < 4; k++) {
        int col = mirror_adm(2 * (int)x - 1 + k, (int)w);
        int l = tmp[base + col], hi = tmp[base + tp + col];
        a += (long)adm_lo[k] * l;  v += (long)adm_hi[k] * l;
        hh += (long)adm_lo[k] * hi; d += (long)adm_hi[k] * hi;
    }
    uint shift = (level == 0 || level == 2) ? 16 : 15;
    long rnd = (long)1 << (shift - 1);
    int av = (int)((a + rnd) >> shift), vv = (int)((v + rnd) >> shift);
    int hv = (int)((hh + rnd) >> shift), dv = (int)((d + rnd) >> shift);
    if (level == 0) { av = (short)av; vv = (short)vv; hv = (short)hv; dv = (short)dv; }
    ulong plane = (ulong)ow * oh;
    ulong p = (ulong)frame * frame_elements + offset + (ulong)y * ow + x;
    pyramid[p] = av; pyramid[p + plane] = vv;
    pyramid[p + 2 * plane] = hv; pyramid[p + 3 * plane] = dv;
}
