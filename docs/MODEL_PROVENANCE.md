# Model provenance and reproduction

DepthGen v1 uses only `Depth Anything V2 Small`, a 24.8M-parameter relative
monocular-depth model licensed under Apache-2.0. It produces relative depth:
larger raw values mean nearer apparent surfaces. It does not promise metric
distance.

## Shipped asset and fixed exporter

The release package uses the FP32 dynamic model asset
`depth_anything_v2_vits_dynamic.onnx`, published in the
`fabio-sim/Depth-Anything-ONNX` release `v2.0.0`. That release tag resolves to
the fixed Apache-2.0 exporter revision
`40ed31643bea3f537201aeb7752d8a16b6d6d178`. The model's input and output are
dynamic, with `H mod 14 = 0` and `W mod 14 = 0`.

The project's asset CMake target downloads it only on explicit request and
verifies this SHA-256:

```text
46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f
```

The model is never downloaded at effect runtime and is not stored in the Git
repository. `Resources/Models/model-manifest.json` repeats the asset URL,
exporter repository, fixed revision, model origin, and SHA-256 for release
auditability. Before creating a session, the native effect computes and checks
the model SHA-256. It then caches that successful check by model path, size,
and modification time, so normal per-frame rendering does not repeatedly hash
the 99 MB asset.

## Input contract

DepthGen reproduces the official Depth Anything V2 image path:

1. Convert source RGB from premultiplied to straight alpha; fully transparent
   pixels become black for inference.
2. Treat input as sRGB by default, or encode linear input with the sRGB OETF
   when selected.
3. Resize with cubic interpolation while preserving aspect ratio, making both
   dimensions multiples of 14. The selected quality controls the minimum short
   edge: 392, 518, 700, or an advanced custom value.
4. Normalise RGB by ImageNet means `(0.485, 0.456, 0.406)` and standard
   deviations `(0.229, 0.224, 0.225)` before NCHW inference.
5. Bilinearly upsample raw depth to the source frame with aligned corners,
   then map robust percentiles to black (far) and white (near).

The exporter itself is not linked or vendored into the plug-in. To regenerate
an updated artefact, explicitly choose a new Apache-2.0 exporter commit,
retain all notices, compare static and dynamic ONNX output with the official
PyTorch model, record a new SHA-256, and validate CPU, DirectML, and Core ML
providers before releasing it.
