# Building and packaging DepthGen

DepthGen must be placed inside an Adobe After Effects SDK `Examples` directory.
The source tree is small by design: model and inference-runtime binaries are
obtained explicitly, never committed, and never fetched by the effect.

## Development build

Windows CPU build:

```powershell
cmake -S . -B build/Win -G 'Visual Studio 17 2022' -A x64 `
  -DDEPTHGEN_ORT_ROOT=C:/path/to/onnxruntime-win-x64-1.20.1
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
inference, DepthGen deterministically retries on CPU.

## Benchmark record

When `DEPTHGEN_TEST_MODEL_PATH` is set, CMake also builds
`depthgen_benchmark`. It measures post-warm-up model inference on a
synthetic, ImageNet-normalised image that represents the inference size for a
1080p source. Its stdout is a two-row CSV record suitable for a release
validation log.

```powershell
$env:DEPTHGEN_MODEL_PATH = 'C:\verified\depth_anything_v2_vits_dml.onnx'
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider accelerated --runs 31
.\build\Win\Release\depthgen_benchmark.exe --quality balanced --provider cpu --runs 31
```

Run the command for `fast`, `balanced`, and `high` on Windows DirectML, macOS
Core ML, and the CPU fallback. Preserve each CSV record with the machine,
operating system, driver/runtime version, model SHA-256, and release revision.
The public release gate requires a supported GPU to be faster than the CPU on
the same machine; the benchmark intentionally reports the actual provider so
a fallback cannot be mistaken for accelerated execution.

`ctest` covers image preprocessing and mapping, the verified-model smoke
path, accelerated-versus-CPU parity, forced accelerator-initialisation and
execution CPU fallback, and missing/mismatched-model failures. The fallback
injection exists only in the test executable; production builds contain no
test override.

## Model asset

The effect looks for `Resources/Models/depth_anything_v2_vits_dml.onnx` beside
the Windows `.aex`, or inside the macOS bundle. The shipped file is a
DirectML-ready repackage of the upstream export. Fetch the upstream model and
build the repackage once (the `.onnx` files are gitignored), then rebuild so
POST_BUILD copies the model next to the plug-in:

```powershell
cmake --build build/Win --target depthgen_build_model
cmake --build build/Win --config Release --target DepthGen
```

`depthgen_fetch_model` downloads only the declared upstream Dynamic Small
export into `build/depthgen_upstream/` and validates its SHA-256;
`depthgen_build_model` runs `tools/build_accelerated_model.py` (requires
Python 3 with the `onnx` package) and fails unless the produced file matches
the pinned repackage SHA-256. Do not copy an unverified file into
`Resources/Models`; retain `model-manifest.json` and
`THIRD_PARTY_NOTICES.md` beside every binary distribution.

## Release gate

Before a public ZIP, validate the exact runtime/model pair on CPU, DirectML,
and Core ML; compare normalised maps within a documented tolerance; test
8/16/32-bpc alpha handling and arbitrary MFR frame order; and record warm-up
p50/p95 figures for 1080p at Fast, Balanced, and High quality. Sign the final
macOS bundle only after all contents, runtime frameworks, model, PiPL, and
notices are present, then run `codesign --verify --deep --strict`.
