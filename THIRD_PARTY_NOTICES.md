# Third-party notices

DepthGen itself is MIT-licensed. The following components are distributed only
under their respective licences. The canonical licence texts in
`third_party/licenses/` accompany every release package. The package process
also copies the exact ONNX Runtime `ThirdPartyNotices.txt` supplied by the
pinned runtime SDK when that file is available.

## ZipDepth Base NPU — MIT

- Purpose: relative monocular depth inference. It is not metric depth.
- UI label: ZipDepth (speed).
- Repository and exporter source: <https://github.com/fabiotosi92/ZipDepth>.
- Fixed revision: `94da7527f7030a0e79d54f33b113bdce4065d735`.
- Upstream checkpoint: `checkpoints/zipdepth_base_npu.pth`.
- SHA-256 (checkpoint): `627c04fda584133ead4310074884a4a037061b4c01ba86e73e492ea30fab570d`.
- Model embedded in this release: `zipdepth_base_npu_dynamic.onnx`, an IR-v8 /
  opset-17 dynamic export produced by `tools/build_zipdepth_model.py`.
- SHA-256 (embedded ONNX payload): `0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59`.
- Copyright: Fabio Tosi, 2026.
- Licence text: `third_party/licenses/ZipDepth-MIT.txt`.

## Depth Anything V2 Small — Apache-2.0

- Purpose: relative monocular depth inference. It is not metric depth.
- UI label: Depth Anything V2 Small (quality).
- Upstream ONNX exporter: <https://github.com/fabio-sim/Depth-Anything-ONNX>
  at commit `40ed31643bea3f537201aeb7752d8a16b6d6d178`.
- Upstream FP32 export: `depth_anything_v2_vits_dynamic.onnx`
  (SHA-256 `46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f`).
- Model embedded in this release: `depth_anything_v2_vits_dml.onnx`, produced by
  `tools/build_accelerated_model.py`.
- SHA-256 (embedded ONNX payload): `237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf`.
- Licence text: `third_party/licenses/Apache-2.0.txt`.

## ONNX Runtime — MIT

- Purpose: native ONNX inference. The plug-in tries CUDA, then DirectML, then
  Core ML (NeuralNetwork, then MLProgram when the runtime supports it), then
  CPU, and uses the first provider that initialises and runs.
- Upstream: <https://github.com/microsoft/onnxruntime>.
- Version: pinned by the configuring release build; record that version in the
  release validation record.
- Licence text: `third_party/licenses/ONNX-Runtime-MIT.txt`.
- The upstream runtime's `ThirdPartyNotices.txt` is copied without alteration
  to the package root where supplied by the selected SDK.
