# Benchmark protocol and development baseline

Run `depthgen_benchmark` with the verified model for every public release. It
prints a CSV header and a single machine-readable row after a warm-up phase.
Use 31 measured samples for the release record and retain the raw CSV, the
model SHA-256, operating-system build, processor, GPU and driver, ONNX Runtime
version, and DepthGen revision.

```text
provider,source_width,source_height,short_edge,inference_width,inference_height,warm_up_runs,measured_runs,p50_ms,p95_ms
```

The required source dimensions are 1920×1080. Run Fast, Balanced, and High on
Windows DirectML, macOS Core ML, and CPU fallback. A supported GPU must be
faster than CPU on the same machine before a public release can be signed.

## Windows development baseline — 2026-08-16 (accelerated repackage)

Same machine and pinned runtime as the 2026-08-15 baseline, but with the
DirectML-ready repackage (`237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf`)
and the streamlined host pipeline. 15 measured samples per row.

| Provider | Quality | Inference size | p50 | p95 |
| --- | --- | --- | ---: | ---: |
| DirectML | Fast | 700×392 | 32.3 ms | 34.8 ms |
| DirectML | Balanced | 924×518 | 53.4 ms | 56.1 ms |
| DirectML | High | 1246×700 | 140.2 ms | 145.0 ms |
| CPU fallback | Balanced | 924×518 | 1061.8 ms | 1084.7 ms |

Every Resize now executes on the GPU; the upstream graph kept one cubic Resize
on the CPU that alone cost about 274 ms per Balanced frame. The host pre/post
pipeline (`depthgen_pipeline_bench`, 1080p 8-bpc, best of 5) totals about
42 ms: alpha read 3.6 ms, fused tensor sampling 12.4 ms, depth upsample
7.2 ms, level mapping 14.3 ms, output write 4.9 ms.

## Windows development baseline — 2026-08-15

This is a development validation, not a macOS sign-off or an After Effects
host-performance certificate. It used the pinned dynamic Small model
(`46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f`),
Microsoft ONNX Runtime DirectML 1.17.3, AMD Ryzen 9 3950X, and NVIDIA GeForce
RTX 3080 Ti. DirectML had 3 warm-up and 31 measured samples. The CPU fallback
used the same DirectML-enabled runtime but forced the CPU provider; its
Balanced/High sample counts are shorter because this controlled Windows agent
session limits each command to 30 seconds. Repeat those rows at 31 samples in
the release environment.

| Provider | Quality | Inference size | Warm-up / samples | p50 | p95 |
| --- | --- | --- | --- | ---: | ---: |
| DirectML | Fast | 700×392 | 3 / 31 | 206.038 ms | 229.571 ms |
| DirectML | Balanced | 924×518 | 3 / 31 | 342.558 ms | 369.535 ms |
| DirectML | High | 1246×700 | 3 / 31 | 520.736 ms | 567.667 ms |
| CPU fallback | Fast | 700×392 | 3 / 31 | 613.333 ms | 668.517 ms |
| CPU fallback | Balanced | 924×518 | 1 / 15 | 1357.316 ms | 1480.702 ms |
| CPU fallback | High | 1246×700 | 1 / 6 | 3878.785 ms | 3942.943 ms |

The tested DirectML provider was faster than CPU for all three quality levels.
Core ML, a 31-sample CPU record at every quality, 8/16/32-bpc host rendering,
and After Effects UI/Undo/MFR validation remain mandatory release-gate work on
their respective hosts.
