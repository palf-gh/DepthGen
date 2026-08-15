# Third-party notices

DepthGen itself is MIT-licensed. The following components are distributed only
under their respective licences. The canonical licence texts in
`third_party/licenses/` accompany every release package. The package process
also copies the exact ONNX Runtime `ThirdPartyNotices.txt` supplied by the
pinned runtime SDK when that file is available.

## Depth Anything V2 Small — Apache-2.0

- Purpose: relative monocular depth inference. It is not metric depth.
- Upstream model implementation and Small checkpoint: <https://github.com/DepthAnything/Depth-Anything-V2>.
- Model used by this release: `depth_anything_v2_vits_dynamic.onnx`, a FP32
  dynamic-shape conversion of the Small model.
- SHA-256: `46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f`.
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
