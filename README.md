# DepthGen for After Effects

[English](#english) · [日本語](#日本語) · [中文](#中文) · [한국어](#한국어)

---

## English

DepthGen is an MIT-licensed Adobe After Effects effect which converts its
source layer into an AI-estimated **relative** depth map. It uses the
Apache-2.0 Depth Anything V2 Small model through ONNX Runtime. White means
nearer apparent depth by default; black means farther apparent depth.

It is intended for compositing, depth mattes, and depth-aware effects. A
single image cannot establish real-world distance, so DepthGen does not claim
metric depth or temporal tracking.

### Features

- Deterministic per-frame SmartFX rendering compatible with Multi-Frame
  Rendering and arbitrary frame order.
- 8/16/32-bpc depth output with source-alpha preservation.
- Fast (392 px), Balanced (518 px), High (700 px), and custom inference size.
- Robust near/far percentile mapping, contrast, depth inversion, sRGB/linear
  input selection, and alpha-aware levels.
- Windows DirectML and macOS Core ML when compiled into the supplied ONNX
  Runtime; deterministic CPU fallback if accelerated session initialisation or
  inference fails.
- English, Japanese, Simplified Chinese, and Korean Effect Controls.

### Installation

Extract a verified release so that `DepthGen.aex` (Windows) or
`DepthGen.plugin` (macOS) remains beside its `Resources/Models` directory and
the bundled ONNX Runtime libraries. Copy the complete directory into the
After Effects plug-ins folder, then restart After Effects. Do not move only the
plug-in binary: the model is an intentional external resource.

Windows:

```text
C:\Program Files\Adobe\Adobe After Effects <version>\Support Files\Plug-ins\
```

macOS:

```text
/Applications/Adobe After Effects <version>/Plug-ins/
```

### Controls

| Control | Meaning |
| --- | --- |
| Quality | Fast 392px, Balanced 518px (default), High 700px, or Custom. The labelled value is the full-resolution inference short edge; lower preview resolutions scale it automatically. |
| Far Clip / Near Clip | Robust percentiles mapped to black / white. Defaults are 2% and 98%. |
| Contrast | Post-normalisation contrast; `1.0` is neutral. |
| Invert Depth | Reverses the white-near convention. |
| Advanced | Group containing Custom Short Edge, Input Transfer (`Assume sRGB` / `Linear to sRGB`), Use Alpha for Levels, Alpha Threshold, and Output Alpha (`Preserve Source Alpha` / `Opaque`). |

### Build, model, and licences

See [docs/BUILD.md](docs/BUILD.md), [docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md),
and [docs/BENCHMARKS.md](docs/BENCHMARKS.md).
The source repository deliberately excludes model weights and runtime binaries.
`THIRD_PARTY_NOTICES.md` identifies every redistributed component and its
licence. Only Depth Anything V2 Small is supported; non-commercial model
variants are intentionally excluded.

### Limitations

DepthGen independently estimates each frame. This is correct for AE's
arbitrary-order renderer but does not eliminate temporal flicker in difficult
shots. Use keyframed levels or a downstream temporal workflow where stability
is more important than deterministic independent frames.

---

## 日本語

DepthGen は、ソースレイヤーから **相対深度** マップを生成する MIT ライセンスの
After Effects エフェクトです。Apache-2.0 の Depth Anything V2 Small と ONNX
Runtime を使用します。初期設定では白が近景、黒が遠景です。

単一画像だけから実距離を保証することはできないため、用途はコンポジット、深度
マット、深度対応エフェクトです。メートル単位の距離や時間追跡は提供しません。

### 主な機能

- 任意順・Multi-Frame Rendering 対応の決定論的なフレーム単位 SmartFX レンダー
- 8/16/32-bpc 出力とソース alpha の保持
- Fast 392px、Balanced 518px（既定）、High 700px、Custom の推論品質
- 遠景／近景 percentile、コントラスト、反転、sRGB／リニア入力、alpha-aware levels
- 対応ランタイムでは Windows DirectML、macOS Core ML を優先し、失敗時は CPU へフォールバック
- 英語、日本語、簡体字中国語、韓国語の UI

### インストール

配布物はバイナリだけでなく `Resources/Models` と ONNX Runtime ライブラリを含めた
ディレクトリ全体で配置してください。Windows は `DepthGen.aex`、macOS は
`DepthGen.plugin` をそのリソースとともに AE の Plug-ins フォルダへコピーし、AE を
再起動します。プラグインバイナリだけを移動しないでください。モデルは意図的な
外部リソースです。

Windows:

```text
C:\Program Files\Adobe\Adobe After Effects <version>\Support Files\Plug-ins\
```

macOS:

```text
/Applications/Adobe After Effects <version>/Plug-ins/
```

### 操作

| パラメータ | 説明 |
| --- | --- |
| 品質 | 高速 (392 px)、標準 (518 px)（既定）、高品質 (700 px)、またはカスタム。ラベルのピクセル数はフル解像度の推論短辺で、プレビュー解像度を下げると自動的に縮小されます。 |
| 遠景クリップ / 近景クリップ | 黒／白へ割り当てる robust percentile。既定は 2% と 98%。 |
| コントラスト | 正規化後のコントラスト。`1.0` が中立。 |
| 深度を反転 | 白近景の向きを反転します。 |
| 詳細設定 | カスタム短辺、入力トランスファー（`sRGB として扱う` / `リニアから sRGB`）、レベルにアルファを使用、アルファしきい値、出力アルファ（`ソースアルファを保持` / `不透明`）を格納するグループです。 |

### ビルド、モデル、ライセンス

[docs/BUILD.md](docs/BUILD.md)、[docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md)、
[docs/BENCHMARKS.md](docs/BENCHMARKS.md) を参照してください。
ソースリポジトリにはモデル重みとランタイムバイナリを含めていません。
`THIRD_PARTY_NOTICES.md` に再配布コンポーネントとライセンスを記載しています。
Depth Anything V2 Small のみをサポートし、非商用モデル variant は意図的に除外しています。

### 制限事項

DepthGen は各フレームを独立に推定します。AE の任意順レンダラーには正しい挙動ですが、
難しいショットでは時間方向のちらつきを完全には除去しません。安定性が独立フレームの
決定論性より重要な場合は、キーフレーム付き levels や下流の時間処理を使用してください。

---

## 中文

DepthGen 是一个 MIT 许可的 After Effects 效果，可从源图层生成 **相对深度** 图。
它使用 Apache-2.0 的 Depth Anything V2 Small 和 ONNX Runtime。默认情况下白色表示
较近、黑色表示较远。

它适用于合成、深度遮罩和深度感知效果，并不保证真实世界的米制距离或时间跟踪。

### 功能

- 与任意帧顺序和 Multi-Frame Rendering 兼容的确定性逐帧 SmartFX 渲染
- 8/16/32-bpc 输出并保留源 Alpha
- Fast 392px、Balanced 518px（默认）、High 700px 和自定义推理尺寸
- 远近百分位映射、对比度、深度反转、sRGB/线性输入和 Alpha-aware levels
- 已编译支持时优先 Windows DirectML 或 macOS Core ML，失败时回退 CPU
- 英语、日语、简体中文和韩语控件

### 安装

请完整复制发行目录，包括 `Resources/Models` 和 ONNX Runtime 库，而不是只复制
`DepthGen.aex` 或 `DepthGen.plugin`。将整个目录复制到 After Effects 插件文件夹后
重启 After Effects。不要只移动插件二进制文件：模型是有意设计的外部资源。

Windows:

```text
C:\Program Files\Adobe\Adobe After Effects <version>\Support Files\Plug-ins\
```

macOS:

```text
/Applications/Adobe After Effects <version>/Plug-ins/
```

### 控件

| 控件 | 含义 |
| --- | --- |
| 质量 | 快速 (392 px)、均衡 (518 px)（默认）、高质量 (700 px) 或自定义。标注的像素数是全分辨率推理短边；降低预览分辨率时会自动缩放。 |
| 远景裁剪 / 近景裁剪 | 映射到黑/白的 robust 百分位。默认 2% 和 98%。 |
| 对比度 | 归一化后的对比度；`1.0` 为中性。 |
| 反转深度 | 反转白近黑远的默认方向。 |
| 高级 | 包含自定义短边、输入传递函数（`假定 sRGB` / `线性转 sRGB`）、使用 Alpha 计算层级、Alpha 阈值、输出 Alpha（`保留源 Alpha` / `不透明`）的分组。 |

### 构建、模型与许可

请参阅 [docs/BUILD.md](docs/BUILD.md)、[docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md)
和 [docs/BENCHMARKS.md](docs/BENCHMARKS.md)。
源代码仓库有意不包含模型权重和运行时二进制文件。
`THIRD_PARTY_NOTICES.md` 列出所有再分发组件及其许可。
仅支持 Depth Anything V2 Small；非商用模型变体被有意排除。

### 限制

DepthGen 独立估计每一帧。这对 AE 的任意顺序渲染器是正确的，但不能完全消除困难
镜头中的时间闪烁。当稳定性比独立帧的确定性更重要时，请使用关键帧 levels 或下游
时间处理流程。

---

## 한국어

DepthGen은 소스 레이어에서 **상대 깊이** 맵을 만드는 MIT 라이선스 After Effects
효과입니다. Apache-2.0 Depth Anything V2 Small과 ONNX Runtime을 사용하며, 기본값은
흰색이 가까움, 검은색이 멂을 의미합니다.

합성, 깊이 매트 및 깊이 인식 효과를 위한 도구이며 실제 거리나 시간 추적을 보장하지
않습니다.

### 기능

- 임의 프레임 순서와 Multi-Frame Rendering에 호환되는 결정적 프레임별 SmartFX 렌더링
- 소스 알파를 보존하는 8/16/32-bpc 출력
- Fast 392px, Balanced 518px(기본), High 700px 및 사용자 지정 추론 크기
- 원근 percentile, 대비, 깊이 반전, sRGB/선형 입력 및 alpha-aware levels
- 컴파일된 런타임에서 Windows DirectML 또는 macOS Core ML을 우선 사용하고 실패 시 CPU로 대체
- 영어, 일본어, 중국어 간체, 한국어 컨트롤

### 설치

배포 디렉터리는 `Resources/Models` 및 ONNX Runtime 라이브러리까지 모두 유지해야 합니다.
`DepthGen.aex` 또는 `DepthGen.plugin`만 이동하지 마십시오. 전체 디렉터리를 After Effects
플러그인 폴더에 복사한 뒤 After Effects를 다시 시작하십시오. 모델은 의도적인 외부
리소스입니다.

Windows:

```text
C:\Program Files\Adobe\Adobe After Effects <version>\Support Files\Plug-ins\
```

macOS:

```text
/Applications/Adobe After Effects <version>/Plug-ins/
```

### 컨트롤

| 컨트롤 | 의미 |
| --- | --- |
| 품질 | 빠름 (392 px), 균형 (518 px)(기본), 고품질 (700 px) 또는 사용자 지정. 표시된 픽셀 수는 전체 해상도 추론 짧은 변이며, 미리보기 해상도를 낮추면 자동으로 축소됩니다. |
| 원거리 클립 / 근거리 클립 | 검정/흰색에 매핑되는 robust percentile. 기본값 2%와 98%. |
| 대비 | 정규화 후 대비. `1.0`이 중립입니다. |
| 깊이 반전 | 흰색-가까움 방향을 반전합니다. |
| 고급 | 사용자 지정 짧은 변, 입력 전달 함수(`sRGB로 간주` / `선형에서 sRGB`), 레벨에 알파 사용, 알파 임계값, 출력 알파(`소스 알파 유지` / `불투명`)를 담는 그룹입니다. |

### 빌드, 모델 및 라이선스

[docs/BUILD.md](docs/BUILD.md), [docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md),
[docs/BENCHMARKS.md](docs/BENCHMARKS.md)를 참조하십시오.
소스 저장소에는 모델 가중치와 런타임 바이너리를 포함하지 않습니다.
`THIRD_PARTY_NOTICES.md`에 재배포 구성 요소와 라이선스를 기재했습니다.
Depth Anything V2 Small만 지원하며, 비상업용 모델 변형은 의도적으로 제외했습니다.

### 제한 사항

DepthGen은 각 프레임을 독립적으로 추정합니다. AE의 임의 순서 렌더러에는 올바른
동작이지만, 어려운 샷에서 시간적 깜빡임을 완전히 제거하지는 않습니다. 안정성이
독립 프레임의 결정론보다 중요할 때는 키프레임 levels 또는 하류 시간 처리 워크플로를
사용하십시오.
