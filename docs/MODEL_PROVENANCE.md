# Model provenance and reproduction

DepthGen v1 uses only `Depth Anything V2 Small`, a 24.8M-parameter relative
monocular-depth model licensed under Apache-2.0. It produces relative depth:
larger raw values mean nearer apparent surfaces. It does not promise metric
distance.

## Shipped asset: accelerated repackage

The release package ships `depth_anything_v2_vits_dml.onnx`, produced from the
upstream FP32 dynamic export by `tools/build_accelerated_model.py`. The script
keeps every weight unchanged and rewrites only interpolation wiring so that all
`Resize` nodes execute on the GPU under DirectML (DirectML rejects cubic
`Resize` and dynamically computed `sizes`):

1. The refinenet 2x upsamples and the head 1.75x upsample take constant
   `scales` (`[1,1,2,2]` / `[1,1,1.75,1.75]`) instead of `sizes` computed by
   `Shape`/`Gather` chains. Output sizes and the `align_corners` coordinate
   mapping are identical; measured against upstream on CPU the raw depth
   differs by at most 1e-6.
2. The learned 37x37 positional-embedding table is resampled with linear
   instead of cubic interpolation. Upstream keeps this node in cubic mode,
   which DirectML cannot execute; profiling showed it consumed about 274 ms of
   CPU time per 924x518 frame while the entire GPU graph needed about 92 ms.
   The effect on normalised depth is small (MAE about 0.3%, p99 about 1.4%,
   exactly zero at the model's native 518x518 patch grid).

Measured on the pinned DirectML runtime (Ryzen 9 3950X, RTX 3080 Ti,
924x518 Balanced): 320 ms with the upstream graph, 53 ms with the repackage.

Shipped repackage SHA-256 (also pinned in
`Source/DepthGen_ModelIntegrity.h` and checked before every session):

```text
237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf
```

## Upstream asset and fixed exporter

The conversion input is the FP32 dynamic model
`depth_anything_v2_vits_dynamic.onnx`, published in the
`fabio-sim/Depth-Anything-ONNX` release `v2.0.0`. That release tag resolves to
the fixed Apache-2.0 exporter revision
`40ed31643bea3f537201aeb7752d8a16b6d6d178`. The model's input and output are
dynamic, with `H mod 14 = 0` and `W mod 14 = 0`.

The `depthgen_fetch_model` target downloads it only on explicit request into
`build/depthgen_upstream/` and verifies this SHA-256:

```text
46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f
```

`depthgen_build_model` then regenerates the shipped repackage and fails unless
the output matches the pinned repackage hash, so the artefact is reproducible
from upstream plus the committed script. Model files are never downloaded at
effect runtime and are not stored in the Git repository.
`Resources/Models/model-manifest.json` repeats the URLs, revisions, and hashes
for release auditability. The native effect caches its successful SHA-256 check
by model path, size, and modification time, so per-frame rendering does not
repeatedly hash the asset.

## Input contract

DepthGen reproduces the official Depth Anything V2 image path:

1. Reduce the premultiplied source straight into the inference-size NCHW
   tensor with alpha-weighted bilinear sampling; fully transparent pixels
   become black for inference.
2. Treat input as sRGB by default, or encode linear input with the sRGB OETF
   when selected.
3. The selected quality controls the full-resolution short edge (392, 518,
   700, or an advanced custom value); the size is scaled into the current
   preview resolution and both dimensions are multiples of 14.
4. Normalise RGB by ImageNet means `(0.485, 0.456, 0.406)` and standard
   deviations `(0.229, 0.224, 0.225)` before NCHW inference.
5. Bilinearly upsample raw depth to the source frame with aligned corners,
   then map robust percentiles to black (far) and white (near).

The exporter itself is not linked or vendored into the plug-in. To regenerate
an updated artefact, explicitly choose a new Apache-2.0 exporter commit,
retain all notices, rerun `tools/build_accelerated_model.py --self-test`,
compare output with the official PyTorch model, record the new SHA-256 values,
and validate CPU, DirectML, and Core ML providers before releasing it.
