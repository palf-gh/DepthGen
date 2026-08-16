# Building and packaging DepthGen

[English](#english) · [日本語](#日本語) · [中文](#中文) · [한국어](#한국어)

---

## English

DepthGen must be placed inside an Adobe After Effects SDK `Examples` directory.
The source tree is small by design: model and inference-runtime binaries are
obtained explicitly, never committed, and never fetched by the effect.

### Development build

Windows CPU build:

```powershell
cmake -S . -B build/Win -G 'Visual Studio 17 2022' -A x64 `
  -DDEPTHGEN_ORT_ROOT=C:/path/to/onnxruntime-directml-1.17.3
cmake --build build/Win --config Release
ctest --test-dir build/Win -C Release --output-on-failure
```

Debug builds write `DepthGen_debug.aex` / `DepthGen_debug.plugin` with display
name `DepthGen debug` and Match Name `PALF DepthGen debug`, so they can load
beside the Release plug-in. Release output names stay `DepthGen.aex` /
`DepthGen.plugin`.

macOS:

```sh
cmake -S . -B build/Mac -DDEPTHGEN_ORT_ROOT=/path/to/onnxruntime-coreml
cmake --build build/Mac --config Release
ctest --test-dir build/Mac --output-on-failure
```

The normal Microsoft Windows package enables CPU inference. For DirectML, use
the Microsoft `Microsoft.ML.OnnxRuntime.DirectML` 1.17.3 native package as
`DEPTHGEN_ORT_ROOT` (its root contains `build/native/include` and
`runtimes/win-x64/native`), or an equivalently pinned custom DML build. A
macOS distribution must use an ONNX Runtime build compiled with `--use_coreml`.
DepthGen detects neither provider by filename alone; the compiled API is the
authority. If the selected provider cannot initialise, or fails during
inference, DepthGen deterministically retries the next provider and then CPU.

### Acceleration providers

When the configured `DEPTHGEN_ORT_ROOT` exports CUDA, DirectML, or Core ML, those
providers are compiled into the normal plug-in. At runtime DepthGen tries CUDA,
then DirectML, then Core ML NeuralNetwork, then Core ML MLProgram (if the ORT
header provides `COREML_FLAG_CREATE_MLPROGRAM`), then CPU. A missing GPU, missing
provider DLL, or session/inference failure falls through to the next candidate.

CUDA requires an ORT GPU package that contains `onnxruntime_providers_cuda.dll`
(validated: ONNX Runtime 1.27.0 `win-x64-gpu_cuda13`, CUDA 13.x, cuDNN 9.x).
cuDNN is not bundled; put its `bin` directory on `PATH`. CUDA keeps the graph
dynamic because ORT 1.27.0 can crash after free dimensions are concretised.

Core ML MLProgram needs an ORT build that exposes `COREML_FLAG_CREATE_MLPROGRAM`
(validated: ORT 1.22.0 universal2). NeuralNetwork is tried first because it was
faster at 1080p on the validation M1; MLProgram is used when NeuralNetwork cannot
create a session. `-DDEPTHGEN_COREML_CPU_AND_GPU=ON` excludes the Neural Engine.

The release build signs nested ORT dylibs before signing the bundle.

### Benchmark record

When `DEPTHGEN_TEST_MODEL_PATH` is set, CMake also builds
`depthgen_benchmark`. It measures post-warm-up model inference on a
synthetic `[0,1]` RGB image that represents the inference size for a 1080p
source. Its stdout is a CSV record suitable for a release
validation log.

```powershell
$env:DEPTHGEN_MODEL_PATH = 'C:\verified\zipdepth_base_npu_dynamic.onnx'
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider accelerated --runs 31
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider cpu --runs 31
```

Run the command for `fast`, `balanced`, and `high` on Windows DirectML, macOS
Core ML, and the CPU fallback. Preserve each CSV record with the machine,
operating system, driver/runtime version, model SHA-256, and release revision.
The public release gate requires a supported GPU to be faster than the CPU on
the same machine; the benchmark intentionally reports the actual provider so
a fallback cannot be mistaken for accelerated execution.

`ctest` covers image preprocessing and mapping, external and executable-
embedded verified-model smoke paths, accelerated-versus-CPU parity, forced
accelerator-initialisation and execution CPU fallback, and
missing/mismatched-model failures. The fallback injection exists only in the
test executable; production builds contain no test override.

### Model asset

The build embeds the SHA-256-verified
`zipdepth_base_npu_dynamic.onnx` and `depth_anything_v2_vits_dml.onnx` as Windows
`RCDATA` resources 256/257 or read-only macOS Mach-O sections `__zipdepth` and
`__dav2`. ONNX Runtime creates sessions directly from those bytes; release layouts
contain no ONNX sidecar. Fetch and export both models once (the files are
gitignored), then rebuild to relink them:

```powershell
cmake --build build/Win --target depthgen_build_model
cmake --build build/Win --config Release --target DepthGen
```

`depthgen_fetch_model` checks out ZipDepth commit
`94da7527f7030a0e79d54f33b113bdce4065d735` under
`build/depthgen_upstream/ZipDepth` and validates the NPU checkpoint SHA-256.
`depthgen_build_model` runs `tools/build_zipdepth_model.py` and fails unless
the produced IR-v8 / opset-17 model matches the pinned SHA-256. The documented
exporter environment is Python 3.12.0, PyTorch 2.13.0+cpu, and ONNX 1.22.0;
`--self-test` also requires NumPy 1.26.4 and ONNX Runtime 1.17.3. A previously
exported ONNX with the expected SHA-256 is reused without requiring PyTorch on
the build host. Do not place an unverified file in `Resources/Models`; retain
`model-manifest.json` and
`THIRD_PARTY_NOTICES.md` beside every binary distribution.

### Release gate

Before a public ZIP, validate the exact runtime/model pair on CPU, DirectML,
Core ML, and CUDA if a CUDA runtime package is distributed; compare normalised
maps within a documented tolerance; test 8/16/32-bpc alpha handling and
arbitrary MFR frame order; and record warm-up p50/p95 figures for 1080p at
Fast, Balanced, and High quality. Sign the final
macOS bundle only after the embedded model, runtime frameworks, PiPL, and
notices are present, then run `codesign --verify --deep --strict`.

---

## 日本語

DepthGen は Adobe After Effects SDK の `Examples` ディレクトリ内に置く必要が
あります。ソースツリーは意図的に小さく、モデルと推論ランタイムのバイナリは
明示的に取得し、コミットせず、エフェクト自身が取得することもありません。

### 開発ビルド

Windows CPU ビルド:

```powershell
cmake -S . -B build/Win -G 'Visual Studio 17 2022' -A x64 `
  -DDEPTHGEN_ORT_ROOT=C:/path/to/onnxruntime-directml-1.17.3
cmake --build build/Win --config Release
ctest --test-dir build/Win -C Release --output-on-failure
```

Debug ビルドは表示名 `DepthGen debug`、Match Name `PALF DepthGen debug` の
`DepthGen_debug.aex` / `DepthGen_debug.plugin` を出力するので、Release
プラグインと並べて読み込めます。Release の出力名は `DepthGen.aex` /
`DepthGen.plugin` のままです。

macOS:

```sh
cmake -S . -B build/Mac -DDEPTHGEN_ORT_ROOT=/path/to/onnxruntime-coreml
cmake --build build/Mac --config Release
ctest --test-dir build/Mac --output-on-failure
```

通常の Microsoft Windows パッケージは CPU 推論を有効にします。DirectML には
Microsoft `Microsoft.ML.OnnxRuntime.DirectML` 1.17.3 の native パッケージを
`DEPTHGEN_ORT_ROOT` に指定します（ルートに `build/native/include` と
`runtimes/win-x64/native` があります）。同等にピンした独自 DML ビルドでも
構いません。macOS 配布物は `--use_coreml` 付きでビルドした ONNX Runtime が
必要です。DepthGen はファイル名だけではプロバイダーを判定せず、コンパイル
された API が正です。選択したプロバイダーの初期化や推論が失敗した場合は、
次のプロバイダー、最後に CPU へ決定論的に再試行します。

### 加速プロバイダー

`DEPTHGEN_ORT_ROOT` が CUDA、DirectML、Core ML を公開していれば、通常のプラグインに
それらをコンパイルします。実行時は CUDA、DirectML、Core ML NeuralNetwork、
Core ML MLProgram（ヘッダーが `COREML_FLAG_CREATE_MLPROGRAM` を持つ場合）、CPU
の順です。GPU や DLL が無い場合、またはセッション／推論が失敗した場合は次へ進みます。

CUDA には `onnxruntime_providers_cuda.dll` を含む GPU ORT パッケージが必要です。
cuDNN は同梱しないので `bin` を `PATH` に入れてください。CUDA ではグラフを動的のままにします。
MLProgram は NeuralNetwork が作れないときのフォールバックです。Release は bundle 署名の前に
入れ子の ORT dylib に署名します。

### ベンチマーク記録

`DEPTHGEN_TEST_MODEL_PATH` を設定すると、CMake は `depthgen_benchmark` も
ビルドします。1080p ソースの推論サイズを表す合成 `[0,1]` RGB 画像に対し、
ウォームアップ後のモデル推論を計測します。標準出力はリリース検証ログ向けの
CSV 記録です。

```powershell
$env:DEPTHGEN_MODEL_PATH = 'C:\verified\zipdepth_base_npu_dynamic.onnx'
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider accelerated --runs 31
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider cpu --runs 31
```

`fast`、`balanced`、`high` を Windows DirectML、macOS Core ML、CPU
フォールバックで実行してください。各 CSV 記録をマシン、OS、ドライバー／
ランタイム版、モデル SHA-256、リリースリビジョンと一緒に保管します。公開
リリースゲートでは、同一マシンで対応 GPU が CPU より速いことが必要です。
ベンチマークは実際に使ったプロバイダーを報告するので、フォールバックを
加速実行と誤認できません。

`ctest` は画像の前処理とマッピング、外部および実行ファイル埋め込みの検証
済みモデル煙テスト、加速対 CPU の一致、強制的な加速初期化／実行の CPU
フォールバック、欠落／不一致モデルの失敗をカバーします。フォールバック注入は
テスト実行ファイルにのみ存在し、製品ビルドにテスト上書きはありません。

### モデル資産

ビルドは SHA-256 検証済みの
`Resources/Models/zipdepth_base_npu_dynamic.onnx` を Windows `RCDATA`
リソース、または読み取り専用の macOS Mach-O セクションへ埋め込みます。
ONNX Runtime はそのバイトから直接セッションを作成し、リリース配置に ONNX
サイドカーはありません。ピンした ZipDepth のソース／チェックポイントを取得し、
動的 ONNX を一度エクスポートしてから（モデルファイルは gitignore）、再ビルド
して再リンクします:

```powershell
cmake --build build/Win --target depthgen_build_model
cmake --build build/Win --config Release --target DepthGen
```

`depthgen_fetch_model` は ZipDepth コミット
`94da7527f7030a0e79d54f33b113bdce4065d735` を
`build/depthgen_upstream/ZipDepth` にチェックアウトし、NPU チェックポイントの
SHA-256 を検証します。`depthgen_build_model` は
`tools/build_zipdepth_model.py` を実行し、生成した IR-v8 / opset-17 モデルが
ピンした SHA-256 と一致しない限り失敗します。文書化したエクスポーター環境は
Python 3.12.0、PyTorch 2.13.0+cpu、ONNX 1.22.0 です。`--self-test` には
さらに NumPy 1.26.4 と ONNX Runtime 1.17.3 が必要です。期待する SHA-256 の
既存 ONNX は、ビルドホストに PyTorch がなくても再利用されます。未検証の
ファイルを `Resources/Models` に置かないでください。すべてのバイナリ配布物の
横に `model-manifest.json` と `THIRD_PARTY_NOTICES.md` を残します。

### リリースゲート

公開 ZIP の前に、CPU、DirectML、Core ML、および CUDA ランタイムパッケージを
配布する場合は CUDA で、正確なランタイム／モデル対を検証してください。正規化
マップを文書化した許容差内で比較し、8/16/32-bpc のアルファ処理と任意の MFR
フレーム順を試し、1080p の Fast / Balanced / High 品質でウォームアップ後の
p50/p95 を記録します。埋め込みモデル、ランタイム framework、PiPL、通知が
揃ってから最終 macOS bundle に署名し、`codesign --verify --deep --strict` を
実行します。

---

## 中文

DepthGen 必须放在 Adobe After Effects SDK 的 `Examples` 目录中。源码树刻意保持
精简：模型和推理运行时二进制文件需显式获取，从不提交，效果本身也不会下载。

### 开发构建

Windows CPU 构建：

```powershell
cmake -S . -B build/Win -G 'Visual Studio 17 2022' -A x64 `
  -DDEPTHGEN_ORT_ROOT=C:/path/to/onnxruntime-directml-1.17.3
cmake --build build/Win --config Release
ctest --test-dir build/Win -C Release --output-on-failure
```

Debug 构建会写出显示名为 `DepthGen debug`、Match Name 为 `PALF DepthGen debug`
的 `DepthGen_debug.aex` / `DepthGen_debug.plugin`，因此可与 Release 插件并存。
Release 输出名仍为 `DepthGen.aex` / `DepthGen.plugin`。

macOS：

```sh
cmake -S . -B build/Mac -DDEPTHGEN_ORT_ROOT=/path/to/onnxruntime-coreml
cmake --build build/Mac --config Release
ctest --test-dir build/Mac --output-on-failure
```

常规 Microsoft Windows 包启用 CPU 推理。若使用 DirectML，请将 Microsoft
`Microsoft.ML.OnnxRuntime.DirectML` 1.17.3 native 包作为 `DEPTHGEN_ORT_ROOT`
（根目录含 `build/native/include` 和 `runtimes/win-x64/native`），或使用同等
固定版本的自定义 DML 构建。macOS 发行版必须使用以 `--use_coreml` 编译的
ONNX Runtime。DepthGen 不会仅凭文件名检测提供程序；以编译后的 API 为准。
若所选提供程序无法初始化或在推理中失败，DepthGen 会按确定顺序重试下一个
提供程序，最后回退到 CPU。

### 加速提供程序

当配置的 `DEPTHGEN_ORT_ROOT` 导出 CUDA、DirectML 或 Core ML 时，普通插件会编译
这些提供程序。运行时依次尝试 CUDA、DirectML、Core ML NeuralNetwork、Core ML
MLProgram（若头文件提供 `COREML_FLAG_CREATE_MLPROGRAM`），然后 CPU。缺少 GPU/DLL
或会话/推理失败时进入下一候选。

CUDA 需要含 `onnxruntime_providers_cuda.dll` 的 GPU ORT 包；cuDNN 不随附。
MLProgram 在 NeuralNetwork 无法建会话时作为回退。Release 会在签署 bundle 前签署
嵌套 ORT dylib。

### 基准记录

设置 `DEPTHGEN_TEST_MODEL_PATH` 时，CMake 还会构建 `depthgen_benchmark`。
它在表示 1080p 源推理尺寸的合成 `[0,1]` RGB 图像上测量预热后的模型推理。
标准输出是适合发布验证日志的 CSV 记录。

```powershell
$env:DEPTHGEN_MODEL_PATH = 'C:\verified\zipdepth_base_npu_dynamic.onnx'
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider accelerated --runs 31
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider cpu --runs 31
```

请在 Windows DirectML、macOS Core ML 和 CPU 回退上对 `fast`、`balanced`、
`high` 运行该命令。将每条 CSV 与机器、操作系统、驱动/运行时版本、模型
SHA-256 和发行修订一并保存。公开发布门槛要求同一台机器上受支持的 GPU
快于 CPU；基准会报告实际使用的提供程序，以免将回退误认为加速执行。

`ctest` 覆盖图像预处理与映射、外部及可执行文件内嵌的已验证模型冒烟路径、
加速与 CPU 一致性、强制加速初始化/执行的 CPU 回退，以及缺失/不匹配模型
失败。回退注入仅存在于测试可执行文件；产品构建不含测试覆盖。

### 模型资源

构建会将通过 SHA-256 验证的
`Resources/Models/zipdepth_base_npu_dynamic.onnx` 嵌入 Windows `RCDATA`
资源或只读 macOS Mach-O 段。ONNX Runtime 直接从这些字节创建会话；发行布局
不含 ONNX sidecar。获取固定的 ZipDepth 源码/检查点并导出一次动态 ONNX
（模型文件被 gitignore），然后重新构建以重新链接：

```powershell
cmake --build build/Win --target depthgen_build_model
cmake --build build/Win --config Release --target DepthGen
```

`depthgen_fetch_model` 会将 ZipDepth 提交
`94da7527f7030a0e79d54f33b113bdce4065d735` 检出到
`build/depthgen_upstream/ZipDepth`，并验证 NPU 检查点 SHA-256。
`depthgen_build_model` 运行 `tools/build_zipdepth_model.py`，除非生成的
IR-v8 / opset-17 模型匹配固定 SHA-256，否则失败。文档中的导出环境为
Python 3.12.0、PyTorch 2.13.0+cpu 和 ONNX 1.22.0；`--self-test` 还需要
NumPy 1.26.4 和 ONNX Runtime 1.17.3。具有预期 SHA-256 的既有 ONNX 可在
构建主机没有 PyTorch 时复用。不要将未验证文件放入 `Resources/Models`；
在每个二进制发行旁保留 `model-manifest.json` 和 `THIRD_PARTY_NOTICES.md`。

### 发布门槛

公开 ZIP 之前，请在 CPU、DirectML、Core ML 上验证精确的运行时/模型对；若
分发 CUDA 运行时包，也要验证 CUDA。在文档容差内比较归一化图；测试
8/16/32-bpc Alpha 处理与任意 MFR 帧顺序；并记录 1080p Fast、Balanced、
High 质量下预热后的 p50/p95。仅在嵌入模型、运行时 framework、PiPL 和声明
齐全后再签署最终 macOS bundle，然后运行 `codesign --verify --deep --strict`。

---

## 한국어

DepthGen은 Adobe After Effects SDK의 `Examples` 디렉터리 안에 두어야 합니다.
소스 트리는 의도적으로 작습니다. 모델과 추론 런타임 바이너리는 명시적으로
가져오며, 커밋하지 않고, 효과가 직접 내려받지도 않습니다.

### 개발 빌드

Windows CPU 빌드:

```powershell
cmake -S . -B build/Win -G 'Visual Studio 17 2022' -A x64 `
  -DDEPTHGEN_ORT_ROOT=C:/path/to/onnxruntime-directml-1.17.3
cmake --build build/Win --config Release
ctest --test-dir build/Win -C Release --output-on-failure
```

Debug 빌드는 표시 이름 `DepthGen debug`, Match Name `PALF DepthGen debug`인
`DepthGen_debug.aex` / `DepthGen_debug.plugin`을 쓰므로 Release 플러그인과
함께 로드할 수 있습니다. Release 출력 이름은 `DepthGen.aex` /
`DepthGen.plugin`입니다.

macOS:

```sh
cmake -S . -B build/Mac -DDEPTHGEN_ORT_ROOT=/path/to/onnxruntime-coreml
cmake --build build/Mac --config Release
ctest --test-dir build/Mac --output-on-failure
```

일반 Microsoft Windows 패키지는 CPU 추론을 켭니다. DirectML에는 Microsoft
`Microsoft.ML.OnnxRuntime.DirectML` 1.17.3 native 패키지를
`DEPTHGEN_ORT_ROOT`로 사용합니다(루트에 `build/native/include`와
`runtimes/win-x64/native`가 있습니다). 동일하게 고정한 사용자 DML 빌드도
가능합니다. macOS 배포물은 `--use_coreml`로 컴파일한 ONNX Runtime이
필요합니다. DepthGen은 파일 이름만으로 제공자를 감지하지 않으며, 컴파일된
API가 기준입니다. 선택한 제공자가 초기화에 실패하거나 추론 중 실패하면
다음 제공자, 그다음 CPU를 결정적으로 재시도합니다.

### 가속 제공자

구성된 `DEPTHGEN_ORT_ROOT`가 CUDA, DirectML, Core ML을 내보내면 일반 플러그인에
컴파일합니다. 런타임은 CUDA, DirectML, Core ML NeuralNetwork, Core ML MLProgram
(헤더가 `COREML_FLAG_CREATE_MLPROGRAM`을 제공할 때), CPU 순입니다. GPU/DLL이
없거나 세션/추론이 실패하면 다음 후보로 넘어갑니다.

CUDA는 `onnxruntime_providers_cuda.dll`이 있는 GPU ORT 패키지가 필요합니다.
MLProgram은 NeuralNetwork가 세션을 만들지 못할 때의 대체입니다. Release는 bundle
서명 전에 중첩 ORT dylib에 서명합니다.

### 벤치마크 기록

`DEPTHGEN_TEST_MODEL_PATH`를 설정하면 CMake가 `depthgen_benchmark`도
빌드합니다. 1080p 소스의 추론 크기를 나타내는 합성 `[0,1]` RGB 이미지에서
워밍업 후 모델 추론을 측정합니다. 표준 출력은 릴리스 검증 로그용 CSV입니다.

```powershell
$env:DEPTHGEN_MODEL_PATH = 'C:\verified\zipdepth_base_npu_dynamic.onnx'
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider accelerated --runs 31
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider cpu --runs 31
```

Windows DirectML, macOS Core ML, CPU 대체에서 `fast`, `balanced`, `high`에
대해 명령을 실행하십시오. 각 CSV를 머신, OS, 드라이버/런타임 버전, 모델
SHA-256, 릴리스 리비전과 함께 보관합니다. 공개 릴리스 게이트는 같은 머신에서
지원 GPU가 CPU보다 빠를 것을 요구합니다. 벤치마크는 실제 제공자를 보고하므로
대체를 가속 실행으로 오인할 수 없습니다.

`ctest`는 이미지 전처리와 매핑, 외부 및 실행 파일 내장 검증 모델 스모크,
가속 대 CPU 일치, 강제 가속기 초기화/실행 CPU 대체, 누락/불일치 모델 실패를
다룹니다. 대체 주입은 테스트 실행 파일에만 있으며 제품 빌드에는 테스트
재정의가 없습니다.

### 모델 자산

빌드는 SHA-256으로 검증된
`Resources/Models/zipdepth_base_npu_dynamic.onnx`를 Windows `RCDATA` 리소스
또는 읽기 전용 macOS Mach-O 섹션에 내장합니다. ONNX Runtime은 그 바이트에서
바로 세션을 만들고, 릴리스 배치에는 ONNX sidecar가 없습니다. 고정한 ZipDepth
소스/체크포인트를 가져와 동적 ONNX를 한 번 내보낸 뒤(모델 파일은 gitignore),
다시 빌드해 다시 링크합니다:

```powershell
cmake --build build/Win --target depthgen_build_model
cmake --build build/Win --config Release --target DepthGen
```

`depthgen_fetch_model`은 ZipDepth 커밋
`94da7527f7030a0e79d54f33b113bdce4065d735`를
`build/depthgen_upstream/ZipDepth`에 체크아웃하고 NPU 체크포인트 SHA-256을
검증합니다. `depthgen_build_model`은 `tools/build_zipdepth_model.py`를
실행하며, 생성된 IR-v8 / opset-17 모델이 고정 SHA-256과 일치하지 않으면
실패합니다. 문서화된 내보내기 환경은 Python 3.12.0, PyTorch 2.13.0+cpu,
ONNX 1.22.0입니다. `--self-test`에는 NumPy 1.26.4와 ONNX Runtime 1.17.3도
필요합니다. 기대한 SHA-256의 기존 ONNX는 빌드 호스트에 PyTorch가 없어도
재사용됩니다. 검증되지 않은 파일을 `Resources/Models`에 두지 마십시오.
모든 바이너리 배포 옆에 `model-manifest.json`과 `THIRD_PARTY_NOTICES.md`를
유지합니다.

### 릴리스 게이트

공개 ZIP 전에 CPU, DirectML, Core ML, 그리고 CUDA 런타임 패키지를 배포하는
경우 CUDA에서 정확한 런타임/모델 쌍을 검증하십시오. 문서화된 허용 오차
안에서 정규화 맵을 비교하고, 8/16/32-bpc 알파 처리와 임의 MFR 프레임 순서를
시험하며, 1080p Fast/Balanced/High 품질의 워밍업 후 p50/p95를 기록합니다.
내장 모델, 런타임 framework, PiPL, 고지가 갖춰진 뒤에만 최종 macOS bundle에
서명하고 `codesign --verify --deep --strict`를 실행합니다.
