# Intel N100 v3 reference-sharing experiment

## English

This experimental v3 builds on the [v2 N100 pipeline](intel_n100_vmaf_v2_test_list.ko.md) and investigates [Netflix/vmaf issue #727](https://github.com/Netflix/vmaf/issues/727) and its linked [PR #726](https://github.com/Netflix/vmaf/pull/726): sharing reference work when one source is compared with several encodes. It is a local proof of concept, not an implementation of the proposed public API.

The FFmpeg filter graph decodes the reference once with VAAPI, maps it to CPU memory, and splits it to three libvmaf filters. The [v3 Motion cache patch](../../tools/experimental/n100/v3-reference-motion-cache.patch) optionally reuses the reference-only Motion SAD result across those filters. It is enabled by `VMAF_N100_SHARED_MOTION=1`; otherwise the normal computation remains. The filters still repeat other reference feature calculations, including ADM and SpEED. The cache uses process-global state, frame indices and picture addresses, and has a 20,000-frame limit. It must remain confined to this controlled single-reference graph: pointer reuse, concurrent graphs, or longer streams require a proper ownership-aware cache/API before production use.

Measurements used an Intel N100, process affinity to two CPU cores, VAAPI decoding, one decoder thread per input, one VMAF worker per concurrent filter, and the VMAF v1 model with CAMBI, chroma SpEED, integer ADM and integer Motion. The baseline was the originally installed FFmpeg running three comparisons sequentially, with two VMAF workers per comparison. Both 1920×1080 8-bit input sets had three encode qualities. Each 300-frame comparison was repeated three times; wall-clock medians include decode, VMAF, JSON output and process exit. The inputs are called A and B to avoid publishing local media identifiers.

| 3 × 300-frame comparisons | Installed, sequential | v3, shared graph | Time saved | Throughput gain |
|---|---:|---:|---:|---:|
| Input A | 42.345 s | 26.333 s | **37.81%** | **60.80%** |
| Input B | 41.987 s | 26.370 s | **37.19%** | **59.22%** |

All recorded per-frame metrics and pooled metrics matched the installed baseline at JSON output precision. The Motion cache computed 299 transitions and reused 598 per 300-frame, three-output batch. Against the v2 build's *sequential* three comparisons, the one-worker shared graph saved 7.09% and 7.39% on A and B in an earlier three-repeat run. This isolates the incremental benefit from the larger installed-to-v3 pipeline improvement.

Full-length shared-graph checks processed 5,921 and 11,574 frames per comparison. They took 504.864 s and 989.685 s, with peak RSS 434,172 KB and 444,600 KB respectively. The middle-quality output matched an existing v2 full-length oracle on every frame and pooled metric. The other two qualities matched sequential output over 300- and 900-frame windows, but do not have full-length sequential oracles. There is no full-length installed-to-v3 speed comparison; the 37–38% result applies to the 300-frame windows.

## 한국어

이 v3 실험은 [N100 v2 경로](intel_n100_vmaf_v2_test_list.ko.md)에 [Netflix/vmaf 이슈 #727](https://github.com/Netflix/vmaf/issues/727) 및 연결된 [PR #726](https://github.com/Netflix/vmaf/pull/726)의 원본 영상 공유 아이디어를 적용한 시제품이다. 제안된 공개 API를 구현한 것은 아니다.

FFmpeg 필터 그래프에서 원본을 VAAPI로 한 번 디코딩하고 CPU 메모리로 매핑한 뒤 세 libvmaf 필터에 분기한다. [v3 Motion 캐시 패치](../../tools/experimental/n100/v3-reference-motion-cache.patch)는 `VMAF_N100_SHARED_MOTION=1`일 때 원본에만 의존하는 Motion SAD 결과를 공유한다. ADM·SpEED 등의 원본 계산은 여전히 각 필터에서 반복한다. 캐시는 프로세스 전역 상태, 프레임 번호, picture 주소에 의존하고 20,000프레임까지만 사용한다. 주소 재사용·동시 그래프·더 긴 입력까지 안전하게 다루려면 수명 관리가 있는 정식 캐시 또는 API가 필요하다. 따라서 현재 패치는 통제된 단일 원본 그래프에서만 사용하는 실험 코드다.

N100에서 프로세스를 CPU 두 코어에 고정하고 VAAPI 디코딩, 입력별 디코더 1스레드, 동시 필터별 VMAF 작업자 1개를 사용했다. 기존 설치본은 세 비교를 순차 실행하며 비교별 VMAF 작업자 2개를 사용했다. 두 입력 A/B는 1920×1080 8-bit 영상이며 각각 세 인코딩 품질을 비교했다. VMAF v1 모델의 CAMBI, chroma SpEED, integer ADM, integer Motion을 계산했다. 각 비교는 300프레임씩 세 번 반복해 중앙값을 구했다. 시간에는 디코딩, 계산, JSON 기록, 종료가 포함된다.

| 300프레임 × 3개 비교 | 기존 설치본 순차 실행 | v3 공유 그래프 | 시간 단축 | 처리율 증가 |
|---|---:|---:|---:|---:|
| 입력 A | 42.345초 | 26.333초 | **37.81%** | **60.80%** |
| 입력 B | 41.987초 | 26.370초 | **37.19%** | **59.22%** |

기록된 프레임별 지표와 pooled 지표는 설치본과 JSON 출력 정밀도에서 모두 일치했다. 300프레임·세 출력의 Motion 전이 299개를 계산하고 598번 재사용했다. 이전 3회 반복 측정에서 v2의 *순차* 세 비교 대비 v3 공유 그래프는 A/B 각각 7.09%/7.39% 시간을 줄였다.

전편 공유 그래프는 비교당 5,921프레임과 11,574프레임을 처리했다. 각각 504.864초와 989.685초가 걸렸고 최대 RSS는 434,172KB와 444,600KB였다. 중간 품질 출력은 기존 v2 전편 결과와 모든 프레임·pooled 지표가 일치했다. 나머지 두 품질은 300·900프레임 구간에서 순차 실행 결과와 일치했지만 전편 순차 기준 결과는 없다. 설치본 대비 전편 속도는 직접 측정하지 않았으므로 37~38% 단축은 300프레임 구간에 한정한다.
