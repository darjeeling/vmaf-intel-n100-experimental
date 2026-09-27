# VMAF - Video Multi-Method Assessment Fusion

> **Experimental Intel N100 v4 test (English)** — With two CPU cores and VAAPI
> decoding, v4 combines shared-reference work, [ADM buffer reuse from issue
> #959](tools/experimental/n100/v4-adm-buffer-reuse.patch), and [gain-one ADM
> clipping from issue #952](tools/experimental/n100/v4-adm-gain1-clip.patch).
> The additional media results validate the same v4 binary; no v5 code
> or binary has been created.
> Three 300-frame comparisons took **42.225 → 23.210 s (45.03% less time)**
> and **42.123 → 22.997 s (45.41% less time)** versus the installed FFmpeg
> (three-run medians). Input A: 1920×1080 8-bit, 24000/1001 fps, about
> 4 min 7 s; input B: 1920×1080 8-bit, 60000/1001 fps, about 3 min 13 s.
> **Scores are not identical:** maximum per-frame VMAF differences over the
> full inputs were **0.002454** and **0.002151 points**, with pooled mean
> differences **+0.001816** and **+0.001401 points** against an existing v2
> full-length oracle for one quality. These differences do not accumulate.
> See the [v4 report](resource/doc/intel_n100_vmaf_v4_adm_memory.ko.md) for
> conditions, memory measurements, and validation limits. On additional 4K
> camera cases, v4 was slower in single-comparison 4K-to-1080p paths, but
> native-4K comparisons took **13.10% and 20.27% less time** than the
> installed original FFmpeg (**15.08% and 25.43% more fps**; 300-frame,
> three-run medians). Native 4K raised v4 peak RSS **3.32× and 3.26×** over
> its 4K-to-1080p path; see the report for installed-binary memory growth.
> The 45% result does not apply to those workloads.
>
> **Intel N100 v4 테스트 (한국어)** — CPU 두 코어와 VAAPI 디코딩 조건에서
> [이슈 #959의 ADM 버퍼 재사용](tools/experimental/n100/v4-adm-buffer-reuse.patch)과
> [이슈 #952의 gain=1 ADM 단순화](tools/experimental/n100/v4-adm-gain1-clip.patch)를
> 원본 공유 경로에 결합했다. 추가 영상 결과도 같은 v4 바이너리를
> 검증한 것이며 v5 코드는 없다. 설치 FFmpeg 대비 300프레임×3개 비교 시간은
> **42.225→23.210초(45.03% 단축)**, **42.123→22.997초(45.41% 단축)**였다
> (3회 중앙값). A는 1920×1080 8-bit·24000/1001fps·약 4분 7초,
> B는 1920×1080 8-bit·60000/1001fps·약 3분 13초다.
> **점수는 완전히 같지 않다.** 기존 v2의 중간 품질 전편 결과와 비교한
> 프레임별 VMAF 최대 차이는 **0.002454점**, **0.002151점**, 영상 평균 차이는
> **+0.001816점**, **+0.001401점**이었다. 프레임별 차이는 누적되지 않는다.
> 4K 카메라 단일 비교에서 4K→1080p 축소 경로는 v4가 느렸지만,
> 원본 4K 해상도에서는 설치된 원본 FFmpeg보다 **시간이 13.10%·20.27% 줄고**
> **처리 fps는 15.08%·25.43% 늘었다**(300프레임, 3회 중앙값).
> 원본 4K 계산 시 v4 최대 RSS는 4K→1080p 경로보다 **3.32배·3.26배**로 늘었다.
> 앞의 45% 단축은 해당 작업에 적용되지 않는다. 조건·메모리·추가 테스트는
> [v4 보고서](resource/doc/intel_n100_vmaf_v4_adm_memory.ko.md)에 있다.

> **Experimental Intel N100 v3 test (English)** — With two CPU cores and VAAPI
> decoding, sharing one reference decode and reference Motion work across three
> concurrent VMAF comparisons cut total time versus the originally installed
> FFmpeg by **37.81%** and **37.19%** on two 1920×1080 8-bit input sets
> (3 × 300 frames, three-run medians). Recorded metrics matched at JSON output
> precision. Full-length checks showed bounded memory use, but full-length
> installed-to-v3 speed was not measured. This is a controlled proof of concept;
> see the [v3 report](resource/doc/intel_n100_vmaf_v3_reference_sharing.md)
> and [experimental Motion patch](tools/experimental/n100/v3-reference-motion-cache.patch).
>
> **Intel N100 v3 테스트 (한국어)** — CPU 두 코어와 VAAPI 디코딩을 사용하면서 원본
> 디코딩 및 원본 Motion 계산을 세 비교에 공유했다. 기존 설치 FFmpeg의 순차 세 비교
> 대비 전체 시간이 두 1920×1080 8-bit 입력에서 각각 **37.81%**, **37.19%**
> 줄었다(각 300프레임, 3회 중앙값). 기록된 지표는 JSON 출력 정밀도에서 일치했다.
> 전편에서는 메모리 사용이 일정 범위에 머무는지 확인했으며 설치본 대비 전편 속도는
> 측정하지 않았다. 통제된 시제품의 조건과 제한은 [v3 보고서](resource/doc/intel_n100_vmaf_v3_reference_sharing.md)와
> [Motion 실험 패치](tools/experimental/n100/v3-reference-motion-cache.patch)에 있다.

> **Experimental Intel N100 v2 test (English)** — This isolated FFmpeg/libvmaf
> experiment is not an upstream release or a production installation. With each
> process pinned to two CPU cores, VMAF using two threads, and one decoder
> thread per input, the v2 build reduced end-to-end comparison time versus the
> installed FFmpeg by **33.67%** and **33.44%** on two 1920×1080, 8-bit inputs:
> **14.179 → 9.405 s** and **13.977 → 9.303 s** per 300 frames (five-run
> medians). This is about **1.51× throughput**. Full-length runs were
> **5.45%** and **5.94%** faster than the previous optimized build (one run
> each). Recorded VMAF metrics matched at JSON output precision. v2 combines
> VAAPI decode/direct mapping, native 8-bit input, SpEED row skipping,
> AVFrame input import, and upstream ADM correctness/AVX2 changes; VMAF
> feature computation stays on the CPU. See the [v2 test report](resource/doc/intel_n100_vmaf_v2_test_list.ko.md),
> [combined ADM patch](tools/experimental/n100/v2-adm-combined.patch), and
> [pipeline experiment](tools/experimental/n100/README.md).
>
> **Intel N100 v2 테스트 (한국어)** — 격리된 FFmpeg/libvmaf 실험이며 upstream
> 릴리스나 운영 설치본의 성능 주장이 아니다. 프로세스를 CPU 두 개에 고정하고
> VMAF 2스레드·입력별 디코더 1스레드로 맞춘 뒤, 설치 FFmpeg와 직접 비교했다.
> 1920×1080 8-bit 입력 두 종류의 300프레임 비교 시간은 5회 중앙값으로
> **14.179→9.405초(33.67% 단축)**, **13.977→9.303초(33.44% 단축)**였다.
> 처리율은 약 **1.51배**다. 전편에서는 이전 최적화 빌드 대비 각각
> **5.45%**, **5.94%** 빨랐으며 각 1회 측정이다. 기록된 VMAF 값은 JSON
> 출력 정밀도에서 일치했다. v2는 VAAPI 디코딩·직접 매핑, 8-bit 입력,
> SpEED 행 계산 생략, AVFrame 복사 생략, ADM 정확도·AVX2 변경을 결합했다.
> feature 계산은 CPU에서 수행한다. 자세한 조건과 제한은
> [v2 테스트 보고서](resource/doc/intel_n100_vmaf_v2_test_list.ko.md),
> [ADM 결합 패치](tools/experimental/n100/v2-adm-combined.patch),
> [파이프라인 실험](tools/experimental/n100/README.md)에 있다.

[![libvmaf](https://github.com/Netflix/vmaf/actions/workflows/libvmaf.yml/badge.svg)](https://github.com/Netflix/vmaf/actions/workflows/libvmaf.yml)
[![Windows](https://github.com/Netflix/vmaf/actions/workflows/windows.yml/badge.svg)](https://github.com/Netflix/vmaf/actions/workflows/windows.yml)
[![ffmpeg](https://github.com/Netflix/vmaf/actions/workflows/ffmpeg.yml/badge.svg)](https://github.com/Netflix/vmaf/actions/workflows/ffmpeg.yml)
[![Docker](https://github.com/Netflix/vmaf/actions/workflows/docker.yml/badge.svg)](https://github.com/Netflix/vmaf/actions/workflows/docker.yml)

VMAF is an [Emmy-winning](https://theemmys.tv/) perceptual video quality assessment algorithm developed by Netflix. This software package includes a stand-alone C library `libvmaf` and its wrapping Python library. The Python library also provides a set of tools that allows a user to train and test a custom VMAF model.

Read [this](https://medium.com/netflix-techblog/toward-a-practical-perceptual-video-quality-metric-653f208b9652) tech blog post for an overview, [this](https://medium.com/netflix-techblog/vmaf-the-journey-continues-44b51ee9ed12) post for the tips of best practices, and [this](https://netflixtechblog.com/toward-a-better-quality-metric-for-the-video-community-7ed94e752a30) post for our latest efforts on speed optimization, new API design and the introduction of a codec evaluation-friendly [NEG mode](resource/doc/models_v0.md#disabling-enhancement-gain-neg-mode).

Also included in `libvmaf` are implementations of several other metrics: PSNR, PSNR-HVS, SSIM, MS-SSIM and CIEDE2000.

![vmaf logo](resource/images/vmaf_logo.jpg)

## News

- (2026-06) We are releasing a new set of VMAF models (**v1**). See [tech blog](https://medium.com/netflix-techblog/vmaf-v1-good-is-not-good-enough-60d7e4244ea8) for more information on v1. See [models_v1.md](resource/doc/models_v1.md) for details and model selection guidance. The previous generation of models (v0) remains documented in [models_v0.md](resource/doc/models_v0.md). We are open to user feedback on VMAF v1 and will continue to improve and update the models over time.
- (2023-12-07) We are releasing `libvmaf v3.0.0`. It contains several optimizations and bug fixes, and a full removal of the APIs which were deprecated in `v2.0.0`.
- (2021-12-15) We have added to CAMBI the `full_ref` input parameter to allow running CAMBI as a full-reference metric, taking into account the banding that was already present on the source. Check out the [usage](resource/doc/cambi.md) page.
- (2021-12-1) We have added to CAMBI the `max_log_contrast` input parameter to allow to capture banding with higher contrasts than the default. We have also sped up CAMBI (e.g., around 4.5x for 4k). Check out the [usage](resource/doc/cambi.md) page.
- (2021-10-7) We are open-sourcing CAMBI (Contrast Aware Multiscale Banding Index) - Netflix's detector for banding (aka contouring) artifacts. Check out the [tech blog](https://netflixtechblog.medium.com/cambi-a-banding-artifact-detector-96777ae12fe2) for an overview and the [technical paper](resource/doc/papers/CAMBI_PCS2021.pdf) published in PCS 2021 (note that the paper describes an initial version of CAMBI that no longer matches the code exactly, but it is still a good introduction). Also check out the [usage](resource/doc/cambi.md) page.
- (2020-12-7) Check out our [latest tech blog](https://netflixtechblog.com/toward-a-better-quality-metric-for-the-video-community-7ed94e752a30) on speed optimization, new API design and the introduction of a codec evaluation-friendly NEG mode.
- (2020-12-3) We are releasing `libvmaf v2.0.0`. It has a new fixed-point and x86 SIMD-optimized (AVX2, AVX-512) implementation that achieves 2x speed up compared to the previous floating-point version. It also has a [new API](libvmaf/README.md) that is more flexible and extensible.
- (2020-7-13) We have created a [memo](https://docs.google.com/document/d/1dJczEhXO0MZjBSNyKmd3ARiCTdFVMNPBykH4_HMPoyY/edit?usp=sharing) to share our thoughts on VMAF's property in the presence of image enhancement operations, its impact on codec evaluation, and our solutions. Accordingly, we have added a new mode called [No Enhancement Gain (NEG)](resource/doc/models.md#disabling-enhancement-gain-neg-mode).
- (2020-2-27) We have changed VMAF's license from Apache 2.0 to [BSD+Patent](https://opensource.org/licenses/BSDplusPatent), a more permissive license compared to Apache that also includes an express patent grant.

## Documentation

There is an [overview of the documentation](resource/doc/index.md) with links to specific pages, covering FAQs, available models and features, software usage guides, and a list of resources.

## Usage

The software package offers a number of ways to interact with the VMAF implementation.

  - The command-line tool [`vmaf`](libvmaf/tools/README.md) provides a complete algorithm implementation, such that one can easily deploy VMAF in a production environment. Additionally, the `vmaf` tool provides a number of auxillary features such as PSNR, SSIM and MS-SSIM.
  - The [C library `libvmaf`](libvmaf/README.md) provides an interface to incorporate VMAF into your code, and tools to integrate other feature extractors into the library.
  - The [Python library](resource/doc/python.md) offers a full array of wrapper classes and scripts for software testing, VMAF model training and validation, dataset processing, data visualization, etc.
  - VMAF is now included as a filter in FFmpeg, and can be configured using: `./configure --enable-libvmaf`. Refer to the [Using VMAF with FFmpeg](resource/doc/ffmpeg.md) page.
  - [VMAF Dockerfile](Dockerfile) generates a docker image from the [Python library](resource/doc/python.md). Refer to [this](resource/doc/docker.md) document for detailed usage.
  - To build VMAF on Windows, follow [these](resource/doc/windows.md) instructions.
  - AOM CTC: [AOM]((http://aomedia.org/)) has specified vmaf to be the standard implementation metrics tool according to the AOM common test conditions (CTC). Refer to [this page](resource/doc/aom_ctc.md) for usage compliant with AOM CTC.

## Contribution Guide

Refer to the [contribution](CONTRIBUTING.md) page. Also refer to this [slide deck](https://docs.google.com/presentation/d/1Gr4-MvOXu9HUiH4nnqLGWupJYMeh6nl2MNz6Qy9153c/edit#slide=id.gc20398b4b7_0_132) for an overview contribution guide.
