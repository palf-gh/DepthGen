# After Effects용 DepthGen

[English](README.md) · [日本語](README_ja.md) · [简体中文](README_zh_CN.md)

DepthGen은 소스 레이어에서 **상대 깊이** 맵을 만드는 MIT 라이선스 After Effects
효과입니다. Apache-2.0 Depth Anything V2 Small과 ONNX Runtime을 사용하며, 기본값은
흰색이 가까움, 검은색이 멂을 의미합니다.

합성, 깊이 매트 및 깊이 인식 효과를 위한 도구이며 실제 거리나 시간 추적을 보장하지
않습니다.

## 기능

- 임의 프레임 순서와 Multi-Frame Rendering에 호환되는 결정적 프레임별 SmartFX 렌더링
- 소스 알파를 보존하는 8/16/32-bpc 출력
- Fast 392px, Balanced 518px(기본), High 700px 및 사용자 지정 추론 크기
- 원근 percentile, 대비, 깊이 반전, sRGB/선형 입력 및 alpha-aware levels
- 컴파일된 런타임에서 Windows DirectML 또는 macOS Core ML을 우선 사용하고 실패 시 CPU로 대체
- 영어, 일본어, 중국어 간체, 한국어 컨트롤

## 설치와 조작

배포 디렉터리는 `Resources/Models` 및 ONNX Runtime 라이브러리까지 모두 유지해야 합니다.
`DepthGen.aex` 또는 `DepthGen.plugin`만 이동하지 마십시오. `Far Clip`/`Near Clip`의
기본값 2%/98%는 검정/흰색 범위를 지정하며, `Contrast=1.0`은 중립입니다.
`Invert Depth`는 흰색-가까움 방향을 반전합니다.

빌드, 모델 검증 및 제3자 라이선스는 [docs/BUILD.md](docs/BUILD.md),
[docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md),
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)를 참조하십시오.
