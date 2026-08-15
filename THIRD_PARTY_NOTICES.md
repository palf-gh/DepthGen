# Third-party notices

DepthGen itself is MIT-licensed. The following components are distributed only
under their respective licences. The canonical licence texts in
`third_party/licenses/` accompany every release package. The package process
also copies the exact ONNX Runtime `ThirdPartyNotices.txt` supplied by the
pinned runtime SDK when that file is available.

## Depth Anything V2 Small — Apache-2.0

- Purpose: relative monocular depth inference. It is not metric depth.
- Upstream model implementation and Small checkpoint: <https://github.com/DepthAnything/Depth-Anything-V2>.
- Model used by this release: `depth_anything_v2_vits_dml.onnx`, a mechanically
  repackaged FP32 conversion of the Small model produced by
  `tools/build_accelerated_model.py` from the hash-verified upstream export
  (constant-scale bilinear upsamples; linear positional-embedding resampling,
  so every Resize runs on the GPU under DirectML). Weights are unchanged.
- SHA-256 (shipped repackage): `237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf`.
- SHA-256 (upstream export input): `46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f`.
- Licence text: `third_party/licenses/Apache-2.0.txt`.
- Attribution: Depth Anything V2, Lihe Yang, Bingyi Kang, Zilong Huang, Zhen
  Zhao, Xiaogang Xu, Jiashi Feng, and Hengshuang Zhao (2024).

## Depth Anything ONNX exporter — Apache-2.0

- Purpose: reproducible ONNX export provenance; it is not linked into the
  plug-in binary.
- Repository: <https://github.com/fabio-sim/Depth-Anything-ONNX>.
- Fixed revision: `40ed31643bea3f537201aeb7752d8a16b6d6d178` (`v2.0.0`).
- Release asset: <https://github.com/fabio-sim/Depth-Anything-ONNX/releases/download/v2.0.0/depth_anything_v2_vits_dynamic.onnx>.
- Copyright: Fabio Milentiansen Sim, 2024.
- Licence text: `third_party/licenses/Apache-2.0.txt`.

## ONNX Runtime — MIT

- Purpose: native ONNX inference and the optional DirectML/Core ML execution
  provider.
- Upstream: <https://github.com/microsoft/onnxruntime>.
- Version: pinned by the configuring release build; record that version in the
  release validation record.
- Licence text: `third_party/licenses/ONNX-Runtime-MIT.txt`.
- The upstream runtime's `ThirdPartyNotices.txt` is copied without alteration
  to the package root where supplied by the selected SDK.
