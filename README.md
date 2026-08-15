# DepthGen for After Effects

[日本語](README_ja.md) · [简体中文](README_zh_CN.md) · [한국어](README_ko_KR.md)

DepthGen is an MIT-licensed Adobe After Effects effect which converts its
source layer into an AI-estimated **relative** depth map. It uses the
Apache-2.0 Depth Anything V2 Small model through ONNX Runtime. White means
nearer apparent depth by default; black means farther apparent depth.

It is intended for compositing, depth mattes, and depth-aware effects. A
single image cannot establish real-world distance, so DepthGen does not claim
metric depth or temporal tracking.

## Features

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

## Installation

Extract a verified release so that `DepthGen.aex` (Windows) or
`DepthGen.plugin` (macOS) remains beside its `Resources/Models` directory and
the bundled ONNX Runtime libraries. Copy the complete directory into the
After Effects plug-ins folder, then restart After Effects. Do not move only the
plug-in binary: the model is an intentional external resource.

## Controls

| Control | Meaning |
| --- | --- |
| Quality | Fast 392px, Balanced 518px (default), High 700px, or Custom. The value is the inference short edge. |
| Far Clip / Near Clip | Robust percentiles mapped to black / white. Defaults are 2% and 98%. |
| Contrast | Post-normalisation contrast; `1.0` is neutral. |
| Invert Depth | Reverses the white-near convention. |
| Show Advanced Controls | Reveals custom size, input transfer, alpha-aware levels, alpha threshold, and output-alpha mode. |

## Build, model, and licences

See [docs/BUILD.md](docs/BUILD.md), [docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md),
and [docs/BENCHMARKS.md](docs/BENCHMARKS.md).
The source repository deliberately excludes model weights and runtime binaries.
`THIRD_PARTY_NOTICES.md` identifies every redistributed component and its
licence. Only Depth Anything V2 Small is supported; non-commercial model
variants are intentionally excluded.

## Limitations

DepthGen independently estimates each frame. This is correct for AE’s
arbitrary-order renderer but does not eliminate temporal flicker in difficult
shots. Use keyframed levels or a downstream temporal workflow where stability
is more important than deterministic independent frames.
