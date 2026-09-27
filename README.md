# VMAF - Video Multi-Method Assessment Fusion

> **Experimental Intel N100 test (English)** — This repository includes an
> isolated FFmpeg/libvmaf pipeline experiment; it is not an upstream VMAF
> release or a production installation. On the tested 1920×1080, 8-bit
> H.264/HEVC pair, the fastest configuration reduced end-to-end comparison
> time by **30.7%** versus the installed FFmpeg: **8.354 s → 5.786 s** for
> 300 frames (three-run median, **1.44× throughput**). Reported VMAF metrics
> matched at JSON output precision. The changes combine VAAPI decode/direct
> mapping, native 8-bit input, SpEED row skipping, and AVFrame input import;
> VMAF feature computation stays on the CPU. See the
> [experiment README](tools/experimental/n100/README.md) and
> [validation summary](resource/doc/intel_n100_experimental_binary_validation.ko.md).
>
> **Intel N100 테스트 (한국어)** — 이 저장소에는 격리된 FFmpeg/libvmaf 파이프라인
> 실험이 포함돼 있다. upstream VMAF 릴리스나 운영 설치본의 성능 주장이 아니다.
> 시험한 1920×1080 8-bit H.264/HEVC 영상에서 300프레임 비교 시간의 3회
> 중앙값은 설치 FFmpeg **8.354초 → 실험본 5.786초**로 **30.7% 단축**됐고,
> 처리율은 **1.44배**였다. 기록된 VMAF 값은 JSON 출력 정밀도에서 일치했다.
> 변경점은 VAAPI 디코딩·직접 매핑, planar 8-bit 입력, SpEED 행 계산 생략,
> AVFrame 입력 복사 생략이며 VMAF feature 계산은 CPU에서 수행한다. 범위와
> 제한은 [실험 README](tools/experimental/n100/README.md)와
> [검증 요약](resource/doc/intel_n100_experimental_binary_validation.ko.md)에 있다.

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
