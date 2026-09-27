# Intel N100 v4: ADM buffer reuse and gain-one clipping

## English

**Version status: v4.** The additional screen-recording and camera tests changed no libvmaf patch or FFmpeg binary. They extend validation of the existing v4 build; a v5 label would require a new implementation. For native-4K single comparisons, v4 increased processing throughput over the installed original FFmpeg by **15.08% (10-bit)** and **25.43% (8-bit)**. Relative to 4K-to-1080p comparisons, native 4K raised v4 peak RSS **3.32×** and **3.26×**; the installed binary rose **4.59×** and **13.36×**. v4 peak RSS stayed near its 300-frame level in separate 900-frame runs, but a multi-hour source has not been tested. The 4K-to-1080p scaling path was slower with v4, so the native-4K result should not be generalized to scaled comparisons.

Experimental v4 combines the [v2 N100 pipeline and ADM changes](intel_n100_vmaf_v2_test_list.ko.md), the [v3 shared-reference graph and Motion cache](intel_n100_vmaf_v3_reference_sharing.md), [ADM buffer reuse](../../tools/experimental/n100/v4-adm-buffer-reuse.patch) inspired by [Netflix/vmaf issue #959](https://github.com/Netflix/vmaf/issues/959), and [gain-one ADM decouple clipping](../../tools/experimental/n100/v4-adm-gain1-clip.patch) inspired by [issue #952](https://github.com/Netflix/vmaf/issues/952). Apply both v4 patches after the v2 ADM patch; the v3 Motion patch is independent. This is an isolated proof of concept, not an upstream release or a production installation.

The #959 patch reuses reference DWT H/V/D storage for the CSF intermediate after the last reference read. It leaves ADM arithmetic and recorded scores unchanged. The #952 patch replaces the lookup/division based decouple calculation with AVX2 min/max clipping **only when `adm_enhn_gain_limit=1.0`**; other settings retain the prior path. Fixed-point rounding differs, so the #952 output is close to the installed result but **not identical**.

Two 1920×1080 8-bit input sets were tested. A is 24000/1001 fps, 5,921 frames, about 4 min 7 s; B is 60000/1001 fps, 11,574 frames, about 3 min 13 s. Each reference was compared with three encode qualities using the VMAF v1 model with CAMBI, chroma SpEED, integer ADM and integer Motion. Media filenames and machine-specific paths are omitted.

Measurements used an Intel N100, process affinity to two CPU cores, VAAPI decoding, one decoder thread per input, and one VMAF worker per concurrent v4 filter. The installed FFmpeg ran three comparisons sequentially with two VMAF workers per comparison. Three-run medians include decoding, VMAF, JSON output and process exit. Each comparison scored 300 frames (about 12.5 s of A or 5.0 s of B content).

| Three 300-frame comparisons | Installed FFmpeg | v4 with #959 only | v4 with #959 + #952 | Time saved vs installed |
|---|---:|---:|---:|---:|
| A | 42.225 s | 26.379 s | **23.210 s** | **45.03%** |
| B | 42.123 s | 26.245 s | **22.997 s** | **45.41%** |

Adding #952 to the #959-only build reduced the paired median time by 12.01% on A and 12.37% on B. The #959-only build matched the installed metrics at JSON precision. With #952, the maximum per-frame VMAF difference over the 300-frame windows was 0.002280 points on A and 0.001991 points on B. This is an absolute difference on the 0–100 VMAF scale, not a percentage. It does not accumulate from frame to frame.

Both v4 configurations passed 22/22 Meson tests. Full-length runs of the combined #959 + #952 build completed all three outputs: A took 443.516 s with peak RSS 413,320 KB; B took 868.321 s with peak RSS 420,644 KB. Against an existing v2 full-length oracle for the middle quality, the largest per-frame VMAF differences were 0.002454 and 0.002151 points. The pooled mean differences were +0.001816 and +0.001401 points. The other two qualities matched the #952-only full-length outputs, but have no installed full-length oracle. There is no paired full-length installed-to-v4 speed result; the 45% figure applies to the 300-frame windows.

The incremental #959 memory effect was measured both alone and with #952. Against v3, #959 alone lowered peak RSS from 417,244 to 395,840 KB on A and from 419,784 to 398,596 KB on B over 300 frames, with unchanged scores and no speed gain. In full-length #952 runs, adding #959 lowered peak RSS from 437,076 to 413,320 KB on A and from 444,748 to 420,644 KB on B; all three outputs matched #952 alone at JSON precision. The #952 score shift was validated only on the model and input types described here.

### Additional media test cases

Three approximately 500 MB sources were copied into ignored local build artifacts and fully re-encoded with the N100 Intel VAAPI HEVC encoder at QP 28 (video only). No source media or private paths are stored in this repository. The working disk had about 774 GB free before copying and about 772 GB free after the encodes.

| Case | Source type | Source size | Video | Duration / frames | Encoded size | Frame-count check |
|---|---|---:|---|---:|---:|---|
| C | Screen recording | 480 MB | H.264, 2560×1516, 8-bit, 25 fps | 2117.28 s / 52,932 | 214 MB | matched |
| D | Handheld camera | 500 MB | HEVC, 3840×2160, 10-bit, 60000/1001 fps | 34.58 s / 2,073 | 180 MB | matched |
| E | Action camera | 462 MB | HEVC, 3840×2160, 8-bit, 30000/1001 fps | 82.02 s / 2,458 | 599 MB | matched |

All VMAF comparisons used VAAPI decoding, one decoder thread per input, process affinity to two N100 CPU cores, two VMAF workers, the v1 model with CAMBI, and one comparison per process. Both inputs were scaled and padded to 1920×1080 using bicubic scaling; D retained 10-bit samples and the other cases used 8-bit samples. The installed FFmpeg and v4 used the same graph apart from v4 picture wrapping. C was checked in three 300-frame windows starting 600, 1800, and 2105 seconds into the 35-minute recording; the last window reaches the end of the file. D and E completed full-length VMAF checks on both binaries. Scores below are absolute points on the 0–100 scale.

| Case / checked interval | Installed mean | v4 mean | Mean difference | Maximum per-frame difference |
|---|---:|---:|---:|---:|
| C / 600 s, 300 frames | 96.909374 | 96.911096 | +0.001722 | 0.002081 |
| C / 1800 s, 300 frames | 96.996443 | 96.998133 | +0.001690 | 0.002148 |
| C / 2105 s, final 300 frames | 97.039922 | 97.041584 | +0.001662 | 0.002044 |
| D / all 2,073 frames | 99.996103 | 99.996137 | +0.000034 | 0.001969 |
| E / all 2,458 frames | 99.998084 | 99.998104 | +0.000020 | 0.001653 |

Alternating three-run 300-frame medians measured decoding, scaling, VMAF, JSON output, and process exit after encoding had finished. The final C window was checked once per binary for correctness and is excluded from the speed table. These are **single-comparison** results and do not exercise the v3 three-output reference sharing that produced the earlier 45% figure.

| Case / offset | Installed | v4 | v4 elapsed-time change |
|---|---:|---:|---:|
| C / 600 s | 17.434 s | 15.540 s | −10.86% |
| C / 1800 s | 16.532 s | 16.069 s | −2.80% |
| D / 5 s | 28.610 s | 34.389 s | **+20.20%** |
| E / 10 s | 22.911 s | 26.218 s | **+14.43%** |

The camera encodes are so close to their sources that pooled VMAF nearly saturates at 100, which limits how much these scores test quality sensitivity. On these 4K-to-1080p single-comparison paths, v4 is slower than the installed FFmpeg; use the installed path for this workload until its conversion and scaling costs are investigated. Encoding time was excluded from the comparison because the three encodes overlapped and contended for the GPU.

#### Native 4K comparison

To isolate the scaling cost, D and E were also compared directly at 3840×2160 with no scale or pad filter. The same two-core affinity, VAAPI decoding, VMAF v1 model with CAMBI, bit depth, and single-comparison setup were retained; CAMBI dimensions were set to 3840×2160. The 300-frame windows began at 5 s for D and 10 s for E. Times and peak RSS are medians of three alternating runs per binary.

| Case | Installed / v4 time | v4 time saved | Installed / v4 processing speed | v4 speed gain | Installed / v4 peak RSS | v4 memory saved |
|---|---:|---:|---:|---:|---:|---:|
| D, 10-bit | 58.174 / 50.551 s | **13.10%** | 5.16 / 5.93 fps | **15.08%** | 2,218,668 / 1,439,020 KB | **35.14%** |
| E, 8-bit | 46.110 / 36.763 s | **20.27%** | 6.51 / 8.16 fps | **25.43%** | 4,437,980 / 1,037,340 KB | **76.63%** |

Native-4K pooled mean VMAF was 98.484649 / 98.485770 (installed / v4) for D and 99.422393 / 99.423107 for E. The maximum per-frame differences were 0.001886 and 0.001832 points. “Time saved” and “speed gain” are different measures: the latter compares frames per second against the installed original FFmpeg.

For the same 300-frame camera windows, peak RSS rose when the 1920×1080 scaling output was replaced with native 4K. These are per-process three-run medians; the native-4K measurements used the same source and encode pairs as the scaled measurements.

| Case / binary | 4K→1080p peak RSS | Native-4K peak RSS | Increase |
|---|---:|---:|---:|
| D / installed | 483,044 KB | 2,218,668 KB | **4.59×** (+359.31%) |
| D / v4 | 433,012 KB | 1,439,020 KB | **3.32×** (+232.33%) |
| E / installed | 332,236 KB | 4,437,980 KB | **13.36×** (+1235.79%) |
| E / v4 | 317,864 KB | 1,037,340 KB | **3.26×** (+226.35%) |

Unlike the 4K-to-1080p path, native 4K was faster with v4 and did not saturate the VMAF score at 100. A separate 900-frame v4 run completed on each case: D took 145.235 s with peak RSS 1,456,544 KB; E took 103.569 s with peak RSS 1,036,136 KB. These peaks were close to the 300-frame measurements. Installed FFmpeg was not run for 900 frames because its 300-frame 8-bit runs already used 4.3–4.6 GB; no full-length native-4K speed or memory claim is made.

## 한국어

**버전은 v4다.** 화면 녹화·카메라 테스트를 추가했지만 libvmaf 패치와 FFmpeg 바이너리는 바꾸지 않았다. 기존 v4의 검증 범위를 넓힌 것이며 새 구현이 없으므로 v5로 부르지 않는다. 원본 4K 단일 비교에서 v4의 처리 fps는 설치된 원본 FFmpeg 대비 **10비트 15.08%**, **8비트 25.43%** 높았다. 4K→1080p 비교 대비 원본 4K 계산의 v4 최대 RSS는 각각 **3.32배**, **3.26배**, 설치본은 **4.59배**, **13.36배**로 늘었다. 별도의 v4 900프레임 테스트에서 최대 RSS는 300프레임 수준과 비슷했지만 수 시간 영상은 검증하지 않았다. 4K→1080p 축소 경로에서는 v4가 느렸으므로 원본 4K 결과를 축소 비교에 적용하지 않는다.

실험용 v4는 [v2 N100 경로·ADM 변경](intel_n100_vmaf_v2_test_list.ko.md), [v3 원본 공유 그래프·Motion 캐시](intel_n100_vmaf_v3_reference_sharing.md), [Netflix/vmaf 이슈 #959](https://github.com/Netflix/vmaf/issues/959)를 참고한 [ADM 버퍼 재사용](../../tools/experimental/n100/v4-adm-buffer-reuse.patch), [이슈 #952](https://github.com/Netflix/vmaf/issues/952)를 참고한 [gain=1 ADM decouple 단순화](../../tools/experimental/n100/v4-adm-gain1-clip.patch)를 결합한다. 두 v4 패치는 v2 ADM 패치 뒤에 적용하며 v3 Motion 패치와는 독립적이다. upstream 릴리스나 운영 설치본이 아닌 격리된 시제품이다.

#959 패치는 원본 DWT H/V/D 계수를 마지막으로 읽은 뒤 그 저장 공간을 CSF 중간 결과에 재사용한다. ADM 연산식과 기록된 점수는 유지한다. #952 패치는 **`adm_enhn_gain_limit=1.0`일 때만** 기존 lookup·나눗셈 기반 decouple 계산을 AVX2 min/max clipping으로 바꾼다. 다른 설정은 기존 경로를 쓴다. 고정소수점 반올림이 달라져 설치본과 점수가 가깝지만 **완전히 같지는 않다**.

입력 A/B는 1920×1080 8-bit 영상이다. A는 24000/1001fps·5,921프레임·약 4분 7초, B는 60000/1001fps·11,574프레임·약 3분 13초다. 각각 같은 원본과 세 인코딩 품질을 비교했다. VMAF v1 모델의 CAMBI, chroma SpEED, integer ADM, integer Motion을 계산했다. 영상명과 장비 경로는 기록하지 않는다.

N100에서 프로세스를 CPU 두 코어에 고정하고 VAAPI 디코딩, 입력별 디코더 1스레드, 동시 v4 필터별 VMAF 작업자 1개를 사용했다. 설치 FFmpeg는 세 비교를 순차 실행하며 비교별 VMAF 작업자 2개를 썼다. 각 비교는 300프레임(A 영상 약 12.5초, B 영상 약 5.0초)이고 3회 반복 중앙값이다. 시간에는 디코딩·VMAF·JSON 기록·프로세스 종료가 포함된다.

| 300프레임 × 3개 비교 | 설치 FFmpeg | #959만 포함한 v4 | #959 + #952 v4 | 설치본 대비 시간 단축 |
|---|---:|---:|---:|---:|
| A | 42.225초 | 26.379초 | **23.210초** | **45.03%** |
| B | 42.123초 | 26.245초 | **22.997초** | **45.41%** |

#959만 포함한 빌드에 #952를 더하면 같은 교차 측정의 중앙값 시간이 A 12.01%, B 12.37% 줄었다. #959만 포함한 빌드는 설치본과 JSON 출력 정밀도에서 점수가 같았다. #952까지 포함하면 300프레임 구간의 프레임별 VMAF 최대 차이는 A 0.002280점, B 0.001991점이다. 이는 0~100점 척도의 **절대 점수 차이**이며 백분율이 아니다. 프레임마다 누적되지 않는다.

두 v4 빌드는 Meson 테스트 22/22를 통과했다. #959+#952 결합 빌드는 세 품질 모두 전편을 완료했다. A는 443.516초·최대 RSS 413,320KB, B는 868.321초·420,644KB였다. 기존 v2 중간 품질 전편 결과와 비교한 프레임별 VMAF 최대 차이는 각각 0.002454점, 0.002151점이었다. 영상 평균 차이는 +0.001816점, +0.001401점이었다. 다른 두 품질은 #952 단독 전편 출력과 일치했지만 설치본 전편 기준 출력은 없다. 설치본 대비 전편 속도는 직접 교차 측정하지 않았으므로 **45% 단축은 300프레임 구간에 한정한다**.

#959의 추가 메모리 효과도 단독과 #952 결합으로 분리해 측정했다. 300프레임에서 #959 단독은 v3 대비 최대 RSS를 A 417,244→395,840KB, B 419,784→398,596KB로 줄였고 점수는 같았으며 속도 이득은 없었다. #952 전편에 #959를 추가하면 최대 RSS가 A 437,076→413,320KB, B 444,748→420,644KB로 줄었고 세 품질의 기록된 출력은 #952 단독과 일치했다. #952의 작은 점수 변화는 여기 적힌 모델과 입력 종류에서만 검증했다.

### 추가 영상 테스트케이스

약 500MB인 원본 세 개를 무시되는 로컬 빌드 공간에 복사한 뒤 N100의 Intel VAAPI HEVC 인코더로 QP 28 전편 재인코딩했다(영상 스트림만). 원본 영상과 개인 경로는 저장소에 넣지 않았다. 작업 디스크 여유는 복사 전 약 774GB, 인코딩 후 약 772GB였다.

| 구분 | 원본 종류 | 원본 크기 | 영상 | 길이 / 프레임 | 인코딩본 크기 | 프레임 수 |
|---|---|---:|---|---:|---:|---|
| C | 화면 녹화 | 480MB | H.264, 2560×1516, 8비트, 25fps | 2117.28초 / 52,932 | 214MB | 일치 |
| D | 휴대용 카메라 | 500MB | HEVC, 3840×2160, 10비트, 60000/1001fps | 34.58초 / 2,073 | 180MB | 일치 |
| E | 액션 카메라 | 462MB | HEVC, 3840×2160, 8비트, 30000/1001fps | 82.02초 / 2,458 | 599MB | 일치 |

VMAF 비교에는 VAAPI 디코딩, 입력별 디코더 1스레드, N100 CPU 두 코어 고정, VMAF 작업자 2개, CAMBI를 포함한 v1 모델, 프로세스당 비교 1개를 사용했다. 두 입력 모두 bicubic 방식으로 1920×1080에 축소·패딩했다. D는 10비트, 나머지는 8비트를 유지했다. 설치본과 v4는 v4의 picture wrapping 외에 같은 그래프를 사용했다. C는 35분 영상의 600초·1800초·2105초 지점에서 각각 300프레임을 검사했으며 마지막 구간은 파일 끝까지 포함한다. D/E는 두 바이너리에서 전편을 계산했다. 점수 차이는 0~100점 척도의 절대값이다.

| 구분 / 확인 구간 | 설치본 평균 | v4 평균 | 평균 차이 | 프레임별 최대 차이 |
|---|---:|---:|---:|---:|
| C / 600초, 300프레임 | 96.909374 | 96.911096 | +0.001722 | 0.002081 |
| C / 1800초, 300프레임 | 96.996443 | 96.998133 | +0.001690 | 0.002148 |
| C / 2105초, 마지막 300프레임 | 97.039922 | 97.041584 | +0.001662 | 0.002044 |
| D / 전편 2,073프레임 | 99.996103 | 99.996137 | +0.000034 | 0.001969 |
| E / 전편 2,458프레임 | 99.998084 | 99.998104 | +0.000020 | 0.001653 |

인코딩 작업이 모두 끝난 뒤 설치본과 v4를 번갈아 3회씩 실행한 300프레임 중앙값이다. 시간에는 디코딩·축소·VMAF·JSON 출력·프로세스 종료가 포함된다. C의 마지막 구간은 정상 동작만 각 1회 확인해 속도 표에서 제외했다. 이는 **단일 비교** 결과이므로 앞의 45% 단축에 기여한 v3의 3개 출력 원본 공유 경로는 사용하지 않았다.

| 구분 / 시작 지점 | 설치본 | v4 | v4 소요 시간 변화 |
|---|---:|---:|---:|
| C / 600초 | 17.434초 | 15.540초 | −10.86% |
| C / 1800초 | 16.532초 | 16.069초 | −2.80% |
| D / 5초 | 28.610초 | 34.389초 | **+20.20%** |
| E / 10초 | 22.911초 | 26.218초 | **+14.43%** |

카메라 인코딩본은 원본에 매우 가까워 평균 VMAF가 거의 100점에 포화된다. 따라서 이 점수만으로 화질 민감도를 충분히 검증했다고 볼 수 없다. 이번 4K→1080p 단일 비교에서는 v4가 설치본보다 느리므로 변환·축소 비용을 개선하기 전에는 설치본 경로가 적합하다. 세 인코딩이 동시에 GPU를 사용했으므로 인코딩 시간은 성능 비교에서 제외했다.

#### 원본 4K 해상도 비교

축소 비용의 영향을 분리하기 위해 D/E를 3840×2160 그대로 scale·pad 필터 없이 비교했다. CPU 두 코어 고정, VAAPI 디코딩, CAMBI를 포함한 VMAF v1 모델, 비트 깊이, 단일 비교 조건은 유지하고 CAMBI 크기만 3840×2160으로 맞췄다. D는 5초, E는 10초 지점의 300프레임이며 설치본과 v4를 번갈아 각 3회 실행한 시간·최대 RSS 중앙값이다.

| 구분 | 설치본 / v4 시간 | v4 시간 단축 | 설치본 / v4 처리 속도 | v4 속도 향상 | 설치본 / v4 최대 RSS | v4 메모리 절감 |
|---|---:|---:|---:|---:|---:|---:|
| D, 10비트 | 58.174 / 50.551초 | **13.10%** | 5.16 / 5.93fps | **15.08%** | 2,218,668 / 1,439,020KB | **35.14%** |
| E, 8비트 | 46.110 / 36.763초 | **20.27%** | 6.51 / 8.16fps | **25.43%** | 4,437,980 / 1,037,340KB | **76.63%** |

원본 4K 평균 VMAF는 D에서 설치본/v4 98.484649/98.485770점, E에서 99.422393/99.423107점이었다. 프레임별 최대 차이는 각각 0.001886점, 0.001832점이다. 표의 '시간 단축'과 '속도 향상'은 다른 값이며, 후자는 설치된 원본 FFmpeg 대비 초당 처리 프레임 증가율이다.

같은 카메라 구간의 300프레임에서 1920×1080으로 축소할 때보다 원본 4K로 계산할 때 최대 RSS가 얼마나 늘어나는지도 실행별 메모리 측정의 3회 중앙값으로 확인했다.

| 구분 / 바이너리 | 4K→1080p 최대 RSS | 원본 4K 최대 RSS | 증가량 |
|---|---:|---:|---:|
| D / 설치본 | 483,044KB | 2,218,668KB | **4.59배** (+359.31%) |
| D / v4 | 433,012KB | 1,439,020KB | **3.32배** (+232.33%) |
| E / 설치본 | 332,236KB | 4,437,980KB | **13.36배** (+1235.79%) |
| E / v4 | 317,864KB | 1,037,340KB | **3.26배** (+226.35%) |

4K→1080p와 달리 원본 4K 비교에서는 v4가 빨랐고 VMAF가 100점에 포화되지 않았다. 별도로 v4에서 각각 900프레임을 연속 처리했다. D는 145.235초·최대 RSS 1,456,544KB, E는 103.569초·1,036,136KB로 300프레임 결과와 비슷했다. 설치본은 8비트 300프레임만으로도 4.3~4.6GB를 사용해 900프레임을 실행하지 않았다. 따라서 원본 4K 전편에 대한 속도·메모리 주장은 하지 않는다.
