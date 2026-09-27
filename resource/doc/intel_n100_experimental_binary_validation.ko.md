# Intel N100 VMAF 실험 결과와 변경 사항

## 환경과 입력

Intel N100(4코어/4스레드, AVX2, Intel UHD GPU), Linux 6.8.0-124-generic에서 측정했다. 설치된 비교 기준은 FFmpeg n8.1.2, 실험용 실행 파일은 FFmpeg 8.1.2이다. 실험용 실행 파일은 수정된 libvmaf 공유 라이브러리를 절대 RPATH로 읽는다. 설치본은 변경하지 않았다.

| 항목 | 값 |
|---|---|
| 원본 | PyCon 영상의 H.264 원본 파일 |
| 재인코딩본 | 같은 영상의 HEVC Q20/Q22/Q24 파일 |
| 입력 특성 | 1920×1080, 24000/1001fps, 8-bit; 원본 H.264, 재인코딩 HEVC |
| VMAF 모델 | vmaf_v1.0.16_3d0h.json |
| CAMBI | `enc_width=1920:enc_height=1080:enc_bitdepth=8` 명시 |
| 병렬도 | 입력별 decoder 2스레드, VMAF 4스레드, FFmpeg filter complex 1스레드 |
| 가속기 | 두 입력 VAAPI decode와 직접 NV12 mapping; VMAF feature 계산은 CPU |

모델 SHA-256은 `e4cf8c147e1368b35497d772920bc92f98c1ad7853c1033d8a836947f427140e`다. 실험 바이너리와 라이브러리는 로컬 빌드 산출물이며 저장소에 포함하지 않는다.

## 현재 구현과 실행 설정

현재 저장소의 일반 `libvmaf` 소스 수정은 Intel OpenCL Motion/ADM PoC와 Meson 빌드 옵션이다. OpenCL 코드는 실험했지만 **최종 빠른 조합에서는 끈다**. 실제 빠른 조합의 별도 수정은 [실험용 패치 모음](../../tools/experimental/n100/README.md)에 보존되어 있고, 빌드 산출물은 추적하지 않는다.

| 변경 | 동작 | 최종 조합 |
|---|---|---|
| H06 SpEED Gaussian | 16배 decimation 뒤 버릴 행의 계산을 생략 | `VMAF_SPEED_FUSED_DEC16=1` |
| H08 FFmpeg hwmap | VAAPI NV12 read mapping 후 software 변환에 필요한 context 정리 | `hwmap=mode=read+direct:detach_sw=1` |
| H04 AVFrame import | FFmpeg→libvmaf 입력 picture 픽셀 복사를 생략하고 참조 수명 유지 | `picture_wrap=1` |
| 입력 설정 | 실제 8-bit 인코딩을 planar 8-bit로 평가 | `format=yuv420p` |

H04/H06/H08은 아직 저장소의 일반 소스나 설치본에 통합된 것이 아니다. 로컬 실험 바이너리 사용 명령은 별도 보관하는 한국어·영어 사용 가이드에 정리했다. 두 가이드는 저장소 커밋에 포함하지 않는다.

## 점수와 성능

같은 Q22 구간(60.06초부터 300프레임), 조건별 3회 중앙값이다. 디코딩, 초기화, VMAF, JSON 기록, 종료까지의 프로세스 벽시계 시간을 측정했다.

| 조건 | 중앙값 | 처리율 | 최대 기록 metric 차이 |
|---|---:|---:|---:|
| 설치 FFmpeg, 기존 10-bit 전처리 | 8.354초 | 35.91fps | 기준 |
| 실험 FFmpeg, 8-bit+H06+H08+H04 | 5.786초 | 51.85fps | 0 |

전체 시간은 **30.7% 감소**, 처리율은 1.444배였다. 각 최적화 실행에서 300쌍에 해당하는 picture 600개가 import됐고 복사·fallback은 0이었다. 모든 프레임의 기록된 15개 metric 및 pooled 값이 기준과 일치했다. 이는 JSON 출력 정밀도에서의 일치이며 모든 중간 부동소수점 상태가 bitwise 동일하다는 주장은 아니다.

PyCon Q20/Q22/Q24 전편 각 5,921프레임에서는 H04를 끈 최적화 조합이 설치 FFmpeg와 기록된 모든 metric 및 pooled 값에서 정확히 일치했다. H04를 켠 경우에는 Q20/Q22/Q24 각 300프레임에서 복사 경로 대비 4.0~4.5% 개선했고, 기록된 값은 일치했다. Q22 전편에서 같은 실험 바이너리의 H04 off/on 단일 실행은 141.887→136.445초(3.84% 단축), picture 11,842개 import 및 모든 기록된 값 일치를 확인했다. 서로 다른 실험군의 절대 시간이나 개선율은 더하지 않는다.

GPU 계산도 실제 실행했다. OpenCL Motion만 사용하면 CPU 계산 대비 6.3%, ADM은 19.5%, 둘 다 사용하면 29.0% 느렸다. 모든 출력값은 일치했고 GPU job 실행 로그도 확인했다. 그래서 최종 조합은 VAAPI decode/direct mapping만 GPU에 맡기고 feature 계산은 CPU에 둔다. 세부 로그·원본 파일명·로컬 경로가 담긴 측정 산출물은 저장소에 포함하지 않는다.

## 검증 경계

점수 일치 및 속도 수치는 위 N100과 PyCon 1080p 8-bit 입력·명시한 모델에 대한 결과다. 다른 코덱, 해상도, 색 형식, 모델, 추가 feature, GPU, 동시 부하에서 같은 속도나 import 성공을 보장하지 않는다. `picture_wrap=1`은 현재 1920×1080 planar 입력으로 제한되며, 지원하지 않는 저장소 형태는 복사 경로로 돌아갈 수 있다. 이 바이너리는 절대 RPATH를 가진 로컬 실험 산출물이므로 실행 파일만 다른 장비로 복사해 배포할 수 없다.
