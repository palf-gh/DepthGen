# DepthGen Agent Guide

## Scope

`DepthGen` is a self-contained, open-source Adobe After Effects effect that
generates a relative depth map from its source layer. It may reference only the
Adobe After Effects SDK Examples tree and the third-party assets documented in
`THIRD_PARTY_NOTICES.md`. Never introduce a dependency on `Palf_Plugins`.

## Branches

- `main` is release-ready history.
- `develop` is the permanent integration branch.
- Cut topic branches from `develop`; do not rewrite history, force-push,
  squash, rebase, or commit directly to `main` without explicit approval.

## Compatibility contract

- Display name is `DepthGen`; Match Name is `PALF DepthGen`.
- Debug builds use a separate identity so they can load beside Release:
  display name `DepthGen debug`, Match Name `PALF DepthGen debug`, and
  filename `DepthGen_debug.aex` / `DepthGen_debug.plugin`. Do not change
  the Release identity.
- `DepthGenParamID` values, PiPL identity, package layout, model manifest
  schema, and resource filenames are persisted/distribution interfaces. Append
  parameter IDs only; never renumber or reuse one.
- Support Windows x64 and macOS universal (Intel plus Apple Silicon), AE 2022+
  and macOS 11+.
- Render code must be deterministic for a given cached previous frame, arbitrary
  frame order, and Multi-Frame Rendering. Do not add hidden layer checkout or
  network access at plug-in runtime. Temporal Stabilisation may keep a
  composition-time keyed range cache in sequence data (never persisted);
  a cache miss must leave the current frame unadjusted.

## Model and licence policy

- v1 ships two embedded models, selected by the `Model` popup:
  - MIT `ZipDepth Base NPU` (speed) at commit
    `94da7527f7030a0e79d54f33b113bdce4065d735`;
  - Apache-2.0 Depth Anything V2 Small (quality), the DirectML-ready repackage
    of the fabio-sim ONNX export at commit
    `40ed31643bea3f537201aeb7752d8a16b6d6d178`.
- Do not substitute another ZipDepth or Depth Anything size, checkpoint, or
  upsampler without revisiting performance, licensing, hashes, and provider
  parity.
- ZipDepth ONNX is an IR-v8 / opset-17 export from `tools/build_zipdepth_model.py`.
  Depth Anything V2 Small ONNX is produced by `tools/build_accelerated_model.py`.
  Regenerate via `depthgen_build_model`; never edit the graphs by hand.
- The top-level project licence is MIT. Preserve every third-party notice,
  version, download URL, and SHA-256 in `THIRD_PARTY_NOTICES.md` and
  `docs/MODEL_PROVENANCE.md`.
- Never commit model weights, ONNX Runtime binaries, CUDA/cuDNN binaries,
  downloaded archives, or `dist/` plug-in outputs. CMake still writes host
  plug-ins to `dist/Win/<Config>/` and `dist/Mac/<Config>/` locally. Embedded
  models make Release binaries exceed GitHub's 100 MB blob limit, so ship them
  via GitHub Releases, not Git. CMake embeds both verified ONNX payloads and the
  Windows `onnxruntime.dll`, which is delay-loaded from LocalAppData after first
  extract. Do not leave a sidecar DLL beside `DepthGen.aex`. macOS still embeds
  the runtime inside `DepthGen.plugin/Contents/Frameworks`. CUDA provider DLLs,
  when compiled in, remain extra files and are not part of the single-`.aex`
  layout.
- CUDA, DirectML, and Core ML are compiled in whenever the configured ORT SDK
  exports them. At runtime the plug-in tries CUDA, then DirectML, then Core ML
  NeuralNetwork, then Core ML MLProgram (when the header provides
  `COREML_FLAG_CREATE_MLPROGRAM`), then CPU. CUDA keeps dynamic free dimensions
  because ORT 1.27.0 is unstable after concretising them. Keep at most one
  CUDA/DirectML/Core ML session at a time; switching Model rebuilds it.

## AE parameter UI

Advanced controls live in a collapsed topic group (`Advanced` / `詳細設定` /
`高级` / `고급`). Do not show or hide them with a checkbox or AEGP `HIDDEN`.
`DEPTHGEN_SHOW_ADVANCED` is retained as a permanently invisible leftover ID
so existing projects stay compatible; never reuse or renumber it.
`DEPTHGEN_MODEL` is an appended ID; keep `USE_VALUE_FOR_OLD_PROJECTS` so older
projects default to ZipDepth.

`DEPTHGEN_TEMPORAL_STABILITY` is an appended 0–100 slider (default 0, with
`USE_VALUE_FOR_OLD_PROJECTS`). It temporally smooths Far/Near mapping endpoints
and piecewise-matches the unit-depth histogram (including the median) to the
previous frame. It must not blend pixels in space. Scene cuts skip the mix and
store the current range.

Checkout, UI, and AEGP stream indices come from `ParamIndexFromID(id)` via
`kDepthGenParamOrder[]`. Do not treat persisted IDs as `params[]` indices.

`Quality` pixel labels depend on the selected model. ZipDepth uses Fast 512,
Balanced 768, and High 1080. Depth Anything V2 Small uses Fast 384, Balanced
518, and High 736. Custom remains 256–2160 for both and is not quantised.
SmartFX must checkout the downsampled full frame (`DepthGenRenderWidth` /
`DepthGenRenderHeight`); `PF_InData::width` / `height` remain full-resolution.
Scale the labelled short edge with `ScaleShortEdgeToRender` before
`ComputeInferenceSize`, passing ZipDepth patch 32 or Depth Anything patch 14.
ZipDepth takes `[0,1]` NCHW RGB; Depth Anything V2 Small takes ImageNet-
normalised planar RGB. Disable Custom Short Edge unless Quality is Custom.

## Validation and records

- Run CTest image-pipeline tests and configure/build the applicable host
  target before hand-off.
- Validate missing/corrupt model errors, CPU fallback, alpha behaviour,
  8/16/32-bpc output, provider parity, and arbitrary render-order stability.
- Append a concise record to `../.agents/production-record.md` for every
  agent-authored DepthGen session.
