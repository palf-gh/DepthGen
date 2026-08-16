# Benchmark protocol and development baseline

[English](#english) · [日本語](#日本語) · [中文](#中文) · [한국어](#한국어)

---

## English

Run `depthgen_benchmark` with the verified model for every public release. It
prints a CSV header and a single machine-readable row after a warm-up phase.
Use 31 measured samples for the release record and retain the raw CSV, the
model SHA-256, operating-system build, processor, GPU and driver, ONNX Runtime
version, and DepthGen revision.

```text
provider,source_width,source_height,short_edge,inference_width,inference_height,warm_up_runs,measured_runs,p50_ms,p95_ms
```

The required source dimensions are 1920×1080. Run Fast, Balanced, and High on
Windows DirectML, macOS Core ML, and CPU fallback. A supported GPU must be
faster than CPU on the same machine before a public release can be signed.

### ZipDepth development baseline — 2026-08-16

Model: `zipdepth_base_npu_dynamic.onnx`
(`0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59`).
Every row uses three warm-up runs. Timings cover model inference only, not
After Effects checkout, sampling, upsampling, level mapping, or output writes.

Windows: AMD Ryzen 9 3950X, NVIDIA GeForce RTX 3080 Ti, Microsoft ONNX Runtime
DirectML 1.17.3.

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| DirectML | 1920×1080 Fast | 928×512 | 31 | 4.856 ms | 6.133 ms |
| DirectML | 1920×1080 Balanced | 1376×768 | 31 | 9.803 ms | 10.902 ms |
| DirectML | 1920×1080 High | 1920×1088 | 31 | 18.772 ms | 20.187 ms |
| DirectML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 66.774 ms | 68.058 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 232.177 ms | 248.822 ms |

macOS: MacBook Pro, Apple M1, 16 GB, ONNX Runtime Core ML 1.17.3.

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML | 1920×1080 Fast | 928×512 | 31 | 57.106 ms | 64.411 ms |
| Core ML | 1920×1080 Balanced | 1376×768 | 31 | 123.735 ms | 136.330 ms |
| Core ML | 1920×1080 High | 1920×1088 | 31 | 246.623 ms | 478.115 ms |
| Core ML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2707.446 ms | 2772.560 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 397.556 ms | 441.676 ms |

### Optional CUDA build

Same Windows host, ONNX Runtime 1.27.0 `win-x64-gpu_cuda13`, CUDA 13.1, and
cuDNN 9.13.1. CUDA keeps the ONNX graph dynamic; concretising its free
dimensions was unstable across repeated ORT 1.27.0 CUDA executions.

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| CUDA | 1920×1080 Fast | 928×512 | 31 | 6.402 ms | 8.034 ms |
| CUDA | 1920×1080 Balanced | 1376×768 | 31 | 12.447 ms | 14.713 ms |
| CUDA | 1920×1080 High | 1920×1088 | 31 | 21.702 ms | 23.923 ms |
| CUDA | 3840×2160 Custom 2160 | 3840×2176 | 7 | 68.990 ms | 70.557 ms |

CUDA parity passed all four release shapes (worst MAE 0.037%, maximum 0.36%).
DirectML remains the default retail path because it is smaller and faster on
this GPU at every measured 1080p preset.

### Optional macOS runtime variants

Same Apple M1 host with ONNX Runtime 1.22.0 universal2. `NeuralNetwork` is
the default Core ML format; `MLProgram CPU+GPU` is the closest supported
GPU-directed route and excludes the Neural Engine.

| Provider / format | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Fast | 928×512 | 31 | 49.666 ms | 56.652 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Balanced | 1376×768 | 31 | 108.808 ms | 123.950 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 High | 1920×1088 | 31 | 230.971 ms | 253.245 ms |
| Core ML 1.22 / NeuralNetwork | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2992.628 ms | 3282.407 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Fast | 928×512 | 31 | 110.066 ms | 118.898 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Balanced | 1376×768 | 31 | 253.990 ms | 266.143 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 High | 1920×1088 | 31 | 511.669 ms | 577.960 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2270.519 ms | 2458.447 ms |

MLProgram improves only the UHD export case on this M1 and remains opt-in. It
is not a direct MPS execution provider; using PyTorch MPS would require a
separate runtime and model payload.

The dynamic ONNX free dimensions are overridden with the current inference
size before session creation for Core ML and DirectML. This produces correct
Core ML output and lets each provider compile a concrete graph. The session is
reused while the inference size remains unchanged. CUDA is the exception above.

ONNX Runtime 1.17.3's DirectML graph optimiser produces incorrect output when
this model's height reaches 1024 pixels. DepthGen disables graph optimisation
only for those DirectML shapes; provider parity then matches CPU while FHD and
UHD performance remain suitable for the corresponding quality modes.

Provider parity covers 928×512 (Fast), 1376×768 (Balanced), 1920×1088 (High),
and 3840×2176 (UHD Custom maximum). Core ML's normalised-map tolerance is MAE
2% and maximum 11%. Custom 2160 is an export-quality option on the tested M1,
not an interactive preset. This is development validation, not an After
Effects host-performance certificate; 8/16/32-bpc, UI/Undo, and MFR host
validation remain release-gate work.

---

## 日本語

公開リリースごとに、検証済みモデルで `depthgen_benchmark` を実行してください。
ウォームアップ後に CSV ヘッダーと機械可読な 1 行を出力します。リリース記録には
31 回の計測サンプルを使い、生 CSV、モデル SHA-256、OS ビルド、プロセッサー、
GPU とドライバー、ONNX Runtime 版、DepthGen リビジョンを保管します。

```text
provider,source_width,source_height,short_edge,inference_width,inference_height,warm_up_runs,measured_runs,p50_ms,p95_ms
```

必要なソース寸法は 1920×1080 です。Windows DirectML、macOS Core ML、CPU
フォールバックで Fast、Balanced、High を実行します。公開リリースに署名する前に、
同一マシンで対応 GPU が CPU より速くなければなりません。

### ZipDepth 開発ベースライン — 2026-08-16

モデル: `zipdepth_base_npu_dynamic.onnx`
(`0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59`)。
各行はウォームアップ 3 回です。計測はモデル推論のみで、After Effects の
チェックアウト、サンプリング、アップサンプリング、レベル写像、出力書き込みは
含みません。

Windows: AMD Ryzen 9 3950X、NVIDIA GeForce RTX 3080 Ti、Microsoft ONNX Runtime
DirectML 1.17.3。

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| DirectML | 1920×1080 Fast | 928×512 | 31 | 4.856 ms | 6.133 ms |
| DirectML | 1920×1080 Balanced | 1376×768 | 31 | 9.803 ms | 10.902 ms |
| DirectML | 1920×1080 High | 1920×1088 | 31 | 18.772 ms | 20.187 ms |
| DirectML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 66.774 ms | 68.058 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 232.177 ms | 248.822 ms |

macOS: MacBook Pro、Apple M1、16 GB、ONNX Runtime Core ML 1.17.3。

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML | 1920×1080 Fast | 928×512 | 31 | 57.106 ms | 64.411 ms |
| Core ML | 1920×1080 Balanced | 1376×768 | 31 | 123.735 ms | 136.330 ms |
| Core ML | 1920×1080 High | 1920×1088 | 31 | 246.623 ms | 478.115 ms |
| Core ML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2707.446 ms | 2772.560 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 397.556 ms | 441.676 ms |

### 任意の CUDA ビルド

同一 Windows ホスト、ONNX Runtime 1.27.0 `win-x64-gpu_cuda13`、CUDA 13.1、
cuDNN 9.13.1。CUDA は ONNX グラフを動的のままにします。自由次元の具体化は
ORT 1.27.0 CUDA の繰り返し実行で不安定でした。

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| CUDA | 1920×1080 Fast | 928×512 | 31 | 6.402 ms | 8.034 ms |
| CUDA | 1920×1080 Balanced | 1376×768 | 31 | 12.447 ms | 14.713 ms |
| CUDA | 1920×1080 High | 1920×1088 | 31 | 21.702 ms | 23.923 ms |
| CUDA | 3840×2160 Custom 2160 | 3840×2176 | 7 | 68.990 ms | 70.557 ms |

CUDA の一致はリリース 4 形状すべてを通過しました（最悪 MAE 0.037%、最大
0.36%）。この GPU では計測したすべての 1080p プリセットで DirectML の方が
小さく速いため、既定の製品パスは DirectML のままです。

### 任意の macOS ランタイム変種

同一 Apple M1 ホスト、ONNX Runtime 1.22.0 universal2。`NeuralNetwork` が既定の
Core ML 形式です。`MLProgram CPU+GPU` は最も近い GPU 指向経路で、Neural Engine
を除外します。

| Provider / format | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Fast | 928×512 | 31 | 49.666 ms | 56.652 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Balanced | 1376×768 | 31 | 108.808 ms | 123.950 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 High | 1920×1088 | 31 | 230.971 ms | 253.245 ms |
| Core ML 1.22 / NeuralNetwork | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2992.628 ms | 3282.407 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Fast | 928×512 | 31 | 110.066 ms | 118.898 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Balanced | 1376×768 | 31 | 253.990 ms | 266.143 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 High | 1920×1088 | 31 | 511.669 ms | 577.960 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2270.519 ms | 2458.447 ms |

MLProgram はこの M1 では UHD 書き出しの場合だけ改善し、オプトインのままです。
直接の MPS 実行プロバイダーではなく、PyTorch MPS を使うには別のランタイムと
モデル本体が必要です。

動的 ONNX の自由次元は、Core ML と DirectML ではセッション作成前に現在の推論
サイズで上書きします。これにより正しい Core ML 出力が得られ、各プロバイダーが
具体的なグラフをコンパイルできます。推論サイズが変わらない間はセッションを
再利用します。上記の CUDA だけが例外です。

ONNX Runtime 1.17.3 の DirectML グラフ最適化は、このモデルの高さが 1024
ピクセルに達すると誤った出力を出します。DepthGen はその DirectML 形状でのみ
グラフ最適化を無効にします。プロバイダー一致は CPU と揃い、FHD と UHD の性能は
対応する品質モードに適したままです。

プロバイダー一致は 928×512（Fast）、1376×768（Balanced）、1920×1088（High）、
3840×2176（UHD Custom 上限）をカバーします。Core ML の正規化マップ許容差は
MAE 2%、最大 11% です。Custom 2160 は検証した M1 では書き出し品質の選択肢であり、
対話的プリセットではありません。これは開発検証であり、After Effects ホスト性能の
証明書ではありません。8/16/32-bpc、UI/Undo、MFR ホスト検証はリリースゲートの
作業です。

---

## 中文

每次公开发布都请用已验证模型运行 `depthgen_benchmark`。预热后会打印 CSV
表头和一行机器可读记录。发布记录使用 31 次测量样本，并保留原始 CSV、模型
SHA-256、操作系统内部版本、处理器、GPU 与驱动、ONNX Runtime 版本以及
DepthGen 修订。

```text
provider,source_width,source_height,short_edge,inference_width,inference_height,warm_up_runs,measured_runs,p50_ms,p95_ms
```

所需源尺寸为 1920×1080。在 Windows DirectML、macOS Core ML 和 CPU 回退上运行
Fast、Balanced、High。签署公开发布之前，同一台机器上受支持的 GPU 必须快于 CPU。

### ZipDepth 开发基线 — 2026-08-16

模型：`zipdepth_base_npu_dynamic.onnx`
（`0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59`）。
每行使用三次预热。计时仅覆盖模型推理，不含 After Effects 检出、采样、上采样、
层级映射或输出写入。

Windows：AMD Ryzen 9 3950X、NVIDIA GeForce RTX 3080 Ti、Microsoft ONNX Runtime
DirectML 1.17.3。

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| DirectML | 1920×1080 Fast | 928×512 | 31 | 4.856 ms | 6.133 ms |
| DirectML | 1920×1080 Balanced | 1376×768 | 31 | 9.803 ms | 10.902 ms |
| DirectML | 1920×1080 High | 1920×1088 | 31 | 18.772 ms | 20.187 ms |
| DirectML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 66.774 ms | 68.058 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 232.177 ms | 248.822 ms |

macOS：MacBook Pro、Apple M1、16 GB、ONNX Runtime Core ML 1.17.3。

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML | 1920×1080 Fast | 928×512 | 31 | 57.106 ms | 64.411 ms |
| Core ML | 1920×1080 Balanced | 1376×768 | 31 | 123.735 ms | 136.330 ms |
| Core ML | 1920×1080 High | 1920×1088 | 31 | 246.623 ms | 478.115 ms |
| Core ML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2707.446 ms | 2772.560 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 397.556 ms | 441.676 ms |

### 可选 CUDA 构建

同一 Windows 主机、ONNX Runtime 1.27.0 `win-x64-gpu_cuda13`、CUDA 13.1 和
cuDNN 9.13.1。CUDA 保持 ONNX 图为动态；将其自由维度具体化后，ORT 1.27.0 CUDA
的重复执行不稳定。

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| CUDA | 1920×1080 Fast | 928×512 | 31 | 6.402 ms | 8.034 ms |
| CUDA | 1920×1080 Balanced | 1376×768 | 31 | 12.447 ms | 14.713 ms |
| CUDA | 1920×1080 High | 1920×1088 | 31 | 21.702 ms | 23.923 ms |
| CUDA | 3840×2160 Custom 2160 | 3840×2176 | 7 | 68.990 ms | 70.557 ms |

CUDA 一致性通过全部四个发布形状（最差 MAE 0.037%，最大 0.36%）。在此 GPU 上
每个测得的 1080p 预设中 DirectML 都更小更快，因此仍是默认零售路径。

### 可选 macOS 运行时变体

同一 Apple M1 主机、ONNX Runtime 1.22.0 universal2。`NeuralNetwork` 是默认
Core ML 格式；`MLProgram CPU+GPU` 是最接近的受支持 GPU 路径，并排除
Neural Engine。

| Provider / format | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Fast | 928×512 | 31 | 49.666 ms | 56.652 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Balanced | 1376×768 | 31 | 108.808 ms | 123.950 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 High | 1920×1088 | 31 | 230.971 ms | 253.245 ms |
| Core ML 1.22 / NeuralNetwork | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2992.628 ms | 3282.407 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Fast | 928×512 | 31 | 110.066 ms | 118.898 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Balanced | 1376×768 | 31 | 253.990 ms | 266.143 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 High | 1920×1088 | 31 | 511.669 ms | 577.960 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2270.519 ms | 2458.447 ms |

在此 M1 上 MLProgram 仅改善 UHD 导出情况，并保持为可选。它不是直接的 MPS
执行提供程序；使用 PyTorch MPS 需要单独的运行时和模型负载。

对 Core ML 和 DirectML，会在创建会话前用当前推理尺寸覆盖动态 ONNX 的自由
维度。这会产生正确的 Core ML 输出，并让每个提供程序编译具体图。推理尺寸
不变时会复用会话。CUDA 是上述例外。

当此模型高度达到 1024 像素时，ONNX Runtime 1.17.3 的 DirectML 图优化器会
产生错误输出。DepthGen 仅对这些 DirectML 形状禁用图优化；提供程序一致性随后
与 CPU 匹配，同时 FHD 和 UHD 性能仍适合对应质量模式。

提供程序一致性覆盖 928×512（Fast）、1376×768（Balanced）、1920×1088（High）
和 3840×2176（UHD Custom 上限）。Core ML 的归一化图容差为 MAE 2%、最大 11%。
Custom 2160 在测试的 M1 上是导出质量选项，不是交互预设。这是开发验证，不是
After Effects 主机性能证书；8/16/32-bpc、UI/Undo 和 MFR 主机验证仍属发布门槛。

---

## 한국어

공개 릴리스마다 검증된 모델로 `depthgen_benchmark`를 실행하십시오. 워밍업 후
CSV 헤더와 기계 가독 한 줄을 출력합니다. 릴리스 기록에는 31회 측정 샘플을
쓰고, 원본 CSV, 모델 SHA-256, OS 빌드, 프로세서, GPU와 드라이버, ONNX Runtime
버전, DepthGen 리비전을 보관합니다.

```text
provider,source_width,source_height,short_edge,inference_width,inference_height,warm_up_runs,measured_runs,p50_ms,p95_ms
```

필요한 소스 크기는 1920×1080입니다. Windows DirectML, macOS Core ML, CPU
대체에서 Fast, Balanced, High를 실행합니다. 공개 릴리스에 서명하기 전에 같은
머신에서 지원 GPU가 CPU보다 빨라야 합니다.

### ZipDepth 개발 기준선 — 2026-08-16

모델: `zipdepth_base_npu_dynamic.onnx`
(`0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59`).
각 행은 워밍업 3회입니다. 측정은 모델 추론만이며 After Effects 체크아웃,
샘플링, 업샘플링, 레벨 매핑, 출력 쓰기는 포함하지 않습니다.

Windows: AMD Ryzen 9 3950X, NVIDIA GeForce RTX 3080 Ti, Microsoft ONNX Runtime
DirectML 1.17.3.

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| DirectML | 1920×1080 Fast | 928×512 | 31 | 4.856 ms | 6.133 ms |
| DirectML | 1920×1080 Balanced | 1376×768 | 31 | 9.803 ms | 10.902 ms |
| DirectML | 1920×1080 High | 1920×1088 | 31 | 18.772 ms | 20.187 ms |
| DirectML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 66.774 ms | 68.058 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 232.177 ms | 248.822 ms |

macOS: MacBook Pro, Apple M1, 16 GB, ONNX Runtime Core ML 1.17.3.

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML | 1920×1080 Fast | 928×512 | 31 | 57.106 ms | 64.411 ms |
| Core ML | 1920×1080 Balanced | 1376×768 | 31 | 123.735 ms | 136.330 ms |
| Core ML | 1920×1080 High | 1920×1088 | 31 | 246.623 ms | 478.115 ms |
| Core ML | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2707.446 ms | 2772.560 ms |
| CPU fallback | 1920×1080 Balanced | 1376×768 | 15 | 397.556 ms | 441.676 ms |

### 선택적 CUDA 빌드

같은 Windows 호스트, ONNX Runtime 1.27.0 `win-x64-gpu_cuda13`, CUDA 13.1,
cuDNN 9.13.1. CUDA는 ONNX 그래프를 동적으로 유지합니다. 자유 차원을 구체화하면
ORT 1.27.0 CUDA의 반복 실행이 불안정했습니다.

| Provider | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| CUDA | 1920×1080 Fast | 928×512 | 31 | 6.402 ms | 8.034 ms |
| CUDA | 1920×1080 Balanced | 1376×768 | 31 | 12.447 ms | 14.713 ms |
| CUDA | 1920×1080 High | 1920×1088 | 31 | 21.702 ms | 23.923 ms |
| CUDA | 3840×2160 Custom 2160 | 3840×2176 | 7 | 68.990 ms | 70.557 ms |

CUDA 일치는 릴리스 네 형상 모두 통과했습니다(최악 MAE 0.037%, 최대 0.36%).
이 GPU에서는 측정한 모든 1080p 프리셋에서 DirectML이 더 작고 빠르므로 기본
소매 경로는 DirectML입니다.

### 선택적 macOS 런타임 변형

같은 Apple M1 호스트, ONNX Runtime 1.22.0 universal2. `NeuralNetwork`가 기본
Core ML 형식입니다. `MLProgram CPU+GPU`는 가장 가까운 지원 GPU 경로이며
Neural Engine을 제외합니다.

| Provider / format | Source / quality | Inference size | Samples | p50 | p95 |
| --- | --- | --- | ---: | ---: | ---: |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Fast | 928×512 | 31 | 49.666 ms | 56.652 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 Balanced | 1376×768 | 31 | 108.808 ms | 123.950 ms |
| Core ML 1.22 / NeuralNetwork | 1920×1080 High | 1920×1088 | 31 | 230.971 ms | 253.245 ms |
| Core ML 1.22 / NeuralNetwork | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2992.628 ms | 3282.407 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Fast | 928×512 | 31 | 110.066 ms | 118.898 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 Balanced | 1376×768 | 31 | 253.990 ms | 266.143 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 1920×1080 High | 1920×1088 | 31 | 511.669 ms | 577.960 ms |
| Core ML 1.22 / MLProgram CPU+GPU | 3840×2160 Custom 2160 | 3840×2176 | 7 | 2270.519 ms | 2458.447 ms |

이 M1에서 MLProgram은 UHD 내보내기 경우만 개선되며 옵트인으로 남습니다. 직접
MPS 실행 제공자가 아니며, PyTorch MPS를 쓰려면 별도의 런타임과 모델 페이로드가
필요합니다.

동적 ONNX 자유 차원은 Core ML과 DirectML에서 세션 생성 전에 현재 추론 크기로
덮어씁니다. 이렇게 하면 올바른 Core ML 출력이 나오고 각 제공자가 구체 그래프를
컴파일할 수 있습니다. 추론 크기가 같으면 세션을 재사용합니다. CUDA만 위 예외입니다.

이 모델 높이가 1024픽셀에 이르면 ONNX Runtime 1.17.3의 DirectML 그래프 최적화기가
잘못된 출력을 냅니다. DepthGen은 해당 DirectML 형상에만 그래프 최적화를 끕니다.
제공자 일치는 CPU와 맞고, FHD와 UHD 성능은 해당 품질 모드에 적합합니다.

제공자 일치는 928×512(Fast), 1376×768(Balanced), 1920×1088(High),
3840×2176(UHD Custom 상한)을 다룹니다. Core ML의 정규화 맵 허용 오차는 MAE 2%,
최대 11%입니다. Custom 2160은 시험한 M1에서 내보내기 품질 옵션이며 대화형 프리셋이
아닙니다. 이는 개발 검증이며 After Effects 호스트 성능 인증이 아닙니다.
8/16/32-bpc, UI/Undo, MFR 호스트 검증은 릴리스 게이트 작업입니다.
