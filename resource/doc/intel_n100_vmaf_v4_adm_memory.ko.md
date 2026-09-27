# Intel N100 v4: ADM 버퍼 재사용

## English

Experimental v4 combines the [v2 ADM and pipeline changes](intel_n100_vmaf_v2_test_list.ko.md), the [v3 shared-reference graph and Motion cache](intel_n100_vmaf_v3_reference_sharing.md), and an [ADM buffer reuse patch](../../tools/experimental/n100/v4-adm-buffer-reuse.patch) inspired by [Netflix/vmaf issue #959](https://github.com/Netflix/vmaf/issues/959). This is a narrower solution than the issue's proposed arithmetic rewrite: once the reference DWT H/V/D coefficients have been consumed, their storage holds the CSF intermediate. The calculation and scores remain unchanged. Apply the v4 patch after the v2 ADM patch; it can be combined with the v3 Motion patch.

The tests ran on an Intel N100 with process affinity to two CPU cores, VAAPI decoding, one decoder thread per input, and one VMAF worker per concurrent filter. Each of two 1920×1080 8-bit input sets used three encode qualities and the VMAF v1 model with CAMBI, chroma SpEED, integer ADM and integer Motion. Three-run medians include decode, VMAF, JSON output and process exit. Media names and machine-specific paths are omitted.

| 3 × 300-frame comparisons, v3 → v4 | Input A | Input B |
|---|---:|---:|
| Wall time | 26.047 → 26.290 s | 26.085 → 26.206 s |
| Change in time | 0.93% slower | 0.46% slower |
| Peak RSS | 417,244 → 395,840 KB | 419,784 → 398,596 KB |

All recorded frame and pooled metrics matched v3 at JSON output precision; 22/22 Meson tests passed. v4 is a memory improvement, not a demonstrated speed improvement over v3.

In a direct, separate three-repeat comparison against the originally installed FFmpeg, with the same two-core affinity and VMAF v1 model, the median total times for three 300-frame comparisons were:

| Input | Installed, sequential | v4, shared graph | Time saved |
|---|---:|---:|---:|
| A | 42.225 s | 26.379 s | **37.53%** |
| B | 42.123 s | 26.245 s | **37.70%** |

Every recorded frame and pooled metric matched the installed baseline at JSON output precision. The installed baseline ran the three comparisons sequentially with two VMAF workers per comparison; v4 ran three concurrent filters with one VMAF worker each. Both processes were pinned to two CPU cores. These results apply to the 300-frame windows, not a full-length installed-to-v4 comparison.

The buffer reuse was also checked over full-length inputs *in a separate build containing the experimental #952 calculation change*: for 5,921 and 11,574 frames per comparison, peak RSS fell from 437,076 to 413,320 KB and from 444,748 to 420,644 KB. The single-run times changed 443.381 → 443.516 s and 871.345 → 868.321 s. All three outputs matched the corresponding #952-only build. This establishes the incremental memory effect over long inputs; the #952 score-changing code is **not included in v4**.

## 한국어

실험용 v4는 [v2 ADM·파이프라인 변경](intel_n100_vmaf_v2_test_list.ko.md), [v3 원본 공유 그래프·Motion 캐시](intel_n100_vmaf_v3_reference_sharing.md), [Netflix/vmaf 이슈 #959](https://github.com/Netflix/vmaf/issues/959)를 참고한 [ADM 버퍼 재사용 패치](../../tools/experimental/n100/v4-adm-buffer-reuse.patch)를 결합한다. 이슈의 계산식 변경보다 좁은 방식으로, 원본 DWT H/V/D 계수를 마지막으로 읽은 뒤 그 저장 공간을 CSF 중간 결과에 사용한다. 연산식과 점수는 바뀌지 않는다. v4 패치는 v2 ADM 패치 뒤에 적용하며 v3 Motion 패치와 함께 쓸 수 있다.

N100에서 프로세스를 CPU 두 코어에 고정하고 VAAPI 디코딩, 입력별 디코더 1스레드, 동시 필터별 VMAF 작업자 1개를 사용했다. 두 1920×1080 8-bit 입력은 각각 세 인코딩 품질을 비교했다. VMAF v1 모델의 CAMBI, chroma SpEED, integer ADM, integer Motion을 사용했다. 각 300프레임 비교를 3회 반복한 중앙값에 디코딩·계산·JSON 기록·종료 시간이 포함된다. 영상 이름과 장비 경로는 기록하지 않는다.

| 300프레임 × 3개 비교, v3 → v4 | 입력 A | 입력 B |
|---|---:|---:|
| 전체 시간 | 26.047 → 26.290초 | 26.085 → 26.206초 |
| 시간 변화 | 0.93% 증가 | 0.46% 증가 |
| 최대 RSS | 417,244 → 395,840KB | 419,784 → 398,596KB |

기록된 프레임·pooled 지표는 모두 v3와 JSON 출력 정밀도에서 일치했고 Meson 테스트 22/22를 통과했다. v4는 v3 대비 속도 개선이 아니라 메모리 절감 변경이다.

설치된 오리지널 FFmpeg와 별도로 직접 교차 실행한 300프레임×3개 비교의 3회 중앙값은 다음과 같다. 두 프로세스 모두 CPU 두 코어에 고정하고 같은 VMAF v1 모델을 사용했다.

| 입력 | 설치본 순차 실행 | v4 공유 그래프 | 시간 단축 |
|---|---:|---:|---:|
| A | 42.225초 | 26.379초 | **37.53%** |
| B | 42.123초 | 26.245초 | **37.70%** |

기록된 프레임별·pooled 지표는 설치본과 JSON 출력 정밀도에서 모두 일치했다. 설치본은 세 비교를 순차 실행하며 비교당 VMAF 작업자 2개, v4는 동시 필터 세 개에 각각 작업자 1개를 사용했다. 이 수치는 300프레임 구간의 결과이며 설치본 대비 전편 속도는 직접 측정하지 않았다.

별도 #952 계산 변경이 포함된 빌드에서도 버퍼 재사용을 전편으로 확인했다. 비교당 5,921프레임과 11,574프레임에서 최대 RSS는 각각 437,076→413,320KB, 444,748→420,644KB로 줄었다. 각 1회 시간은 443.381→443.516초, 871.345→868.321초였고 세 품질의 출력은 해당 #952 단독 빌드와 일치했다. 이 결과는 긴 입력에서 #959의 추가 메모리 효과를 확인한 것이다. **점수가 달라지는 #952 코드는 v4에 포함하지 않았다.**
