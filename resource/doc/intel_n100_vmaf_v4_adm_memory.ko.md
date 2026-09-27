# Intel N100 v4: ADM buffer reuse and gain-one clipping

## English

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

## 한국어

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
