# Intel N100 VMAF experiments

## Summary (English)

On the tested Intel N100, the fastest experimental FFmpeg pipeline reduced
end-to-end time by **30.7%** versus the installed FFmpeg: **8.354 s to 5.786 s**
for the same 300 frames (median of three runs, **1.44× throughput**). Recorded
per-frame features, VMAF scores, and pooled scores matched at JSON output
precision. The comparison used a 1920×1080, 8-bit H.264 reference and HEVC
re-encode with the explicit VMAF v1 model.

The winning changes combine VAAPI decoding and direct NV12 mapping, native
8-bit planar input, a SpEED calculation that skips discarded Gaussian rows,
and AVFrame import that avoids the FFmpeg-to-libvmaf input pixel copy. VMAF
features run on the CPU; tested OpenCL Motion/ADM computation was slower.
These changes remain isolated patches, not a production installation.

## 요약 (한국어)

Intel N100에서 가장 빠른 실험용 FFmpeg 경로의 전체 비교 시간은 기존 설치본보다
**30.7% 짧았다**. 같은 300프레임을 세 번 실행한 중앙값이
**8.354초→5.786초**로 줄었고 처리율은 **1.44배**였다. 기록된 프레임별 feature·VMAF
점수와 pooled 값은 JSON 출력 정밀도에서 일치했다. 입력은 1920×1080 8-bit
H.264 원본과 HEVC 재인코딩본이며 VMAF v1 모델을 명시했다.

빠른 조합은 VAAPI 디코딩과 NV12 직접 매핑, planar 8-bit 입력, 버리는 Gaussian
행을 계산하지 않는 SpEED 수정, FFmpeg→libvmaf 픽셀 복사를 줄이는 AVFrame
import를 함께 사용한다. VMAF feature 계산은 CPU에서 수행했고, 실제 시험한
OpenCL Motion/ADM 계산은 더 느렸다. 변경은 운영 설치본이 아닌 실험용 패치로
보존했다.

This directory contains source patches and focused test programs. It does not
contain machine-specific paths, input filenames, benchmark logs, binaries, or
raw result files. The changes are experimental and are not integrated into the
regular FFmpeg or libvmaf source tree.

| Change | Files | Purpose |
|---|---|---|
| SpEED selected-row Gaussian | speed-fused-dec16.patch | Skip rows discarded by 16:1 decimation while preserving the existing arithmetic order. Enabled by VMAF_SPEED_FUSED_DEC16=1. |
| FFmpeg picture pool | ffmpeg-picture-pool.patch | Add an optional reusable input-picture pool. Off by default. |
| AVFrame picture import | picture-import.patch, ffmpeg-picture-wrap.patch | Retain refcounted input buffers and avoid the FFmpeg-to-libvmaf pixel copy on supported planar frames. Off by default. |
| VAAPI mapping | ffmpeg-hwmap-detach-sw.patch | Permit read-only VAAPI-to-NV12 mapping to feed the software conversion path. Off by default. |
| v3 shared-reference Motion | v3-reference-motion-cache.patch | Experimental process-local Motion SAD reuse for one reference split to three comparisons. Enabled by VMAF_N100_SHARED_MOTION=1; see the [v3 report](../../../resource/doc/intel_n100_vmaf_v3_reference_sharing.md) for limits and results. |
| v4 ADM buffer reuse ([issue #959](https://github.com/Netflix/vmaf/issues/959)) | v4-adm-buffer-reuse.patch | Apply after v2-adm-combined.patch alongside v3-reference-motion-cache.patch. Reuse reference DWT H/V/D storage for CSF after its final read without changing arithmetic. See the [v4 report](../../../resource/doc/intel_n100_vmaf_v4_adm_memory.ko.md). |

The direct mapping patch accepts only forward read mappings; it is not a
general writable mapping facility. Picture import is bounded to 1920x1080
planar 8-bit or 10-bit frames in the audited VMAF v1 CPU feature path.
Unsupported storage falls back to copying.

## Tests performed

- Differential SpEED test (test_dec16.c): 84 cases and 145,632 sampled
  coefficients matched the original float bit patterns. Address, undefined
  behavior, and leak sanitizers passed.
- Picture-import ownership test (test_import.c): pointer identity, independent
  references, callback lifetime and single release, worker-thread release,
  and invalid storage contracts passed at 8 and 10 bits under sanitizers.
- Existing libvmaf Meson suite: 21/21 tests passed in the isolated build.
- CAMBI row-pitch tests: 21 cases, including source pitch both smaller and
  larger than destination pitch, passed under sanitizers.
- Video comparison types: 1920x1080 8-bit H.264 reference versus HEVC
  re-encodes, 24000/1001 fps, three quality levels, 300-frame windows and
  full-file runs. Reported frame metrics and pooled scores matched the
  installed baseline at JSON output precision.
- N100 VAAPI decode/direct mapping, CPU feature computation, and actual
  OpenCL Motion/ADM computation were timed. The fastest tested combination
  retained GPU decoding and CPU feature computation.

The test description does not imply validation for other models, resolutions,
pixel formats, GPU drivers, or additional feature sets. Local build artifacts
and per-run evidence are intentionally not committed here.
