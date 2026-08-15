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
- Render code must be deterministic for arbitrary frame order and Multi-Frame
  Rendering. Do not add temporal caches, hidden frame checkout, or network
  access at plug-in runtime.

## Model and licence policy

- Only `Depth Anything V2 Small` may ship in v1. Its model licence is
  Apache-2.0. Do not substitute Base, Large, Giant, metric, or Video Depth
  Anything weights: their licensing or execution model is outside this
  project’s v1 contract.
- The top-level project licence is MIT. Preserve every third-party notice,
  version, download URL, and SHA-256 in `THIRD_PARTY_NOTICES.md` and
  `docs/MODEL_PROVENANCE.md`.
- Never commit model weights, ONNX Runtime binaries, downloaded archives, or
  generated release packages. The explicit CMake asset target verifies hashes.

## AE parameter UI

Advanced controls live in a collapsed topic group (`Advanced` / `詳細設定` /
`高级` / `고급`). Do not show or hide them with a checkbox or AEGP `HIDDEN`.
`DEPTHGEN_SHOW_ADVANCED` is retained as a permanently invisible leftover ID
so existing projects stay compatible; never reuse or renumber it.

Checkout, UI, and AEGP stream indices come from `ParamIndexFromID(id)` via
`kDepthGenParamOrder[]`. Do not treat persisted IDs as `params[]` indices.

`Quality` pixel labels are full-resolution short-edge sizes. SmartFX must
checkout the downsampled full frame (`DepthGenRenderWidth` /
`DepthGenRenderHeight`); `PF_InData::width` / `height` remain full-resolution.
Scale the labelled short edge with `ScaleShortEdgeToRender` before
`ComputeInferenceSize`. Disable Custom Short Edge unless Quality is Custom.

## Validation and records

- Run CTest image/normalisation tests and configure/build the applicable host
  target before hand-off.
- Validate missing/corrupt model errors, CPU fallback, alpha behaviour,
  8/16/32-bpc output, provider parity, and arbitrary render-order stability.
- Append a concise record to `../.agents/production-record.md` for every
  agent-authored DepthGen session.
