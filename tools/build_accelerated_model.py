#!/usr/bin/env python3
"""Build DepthGen's accelerated Depth Anything V2 Small model.

Starts from the hash-verified upstream FP32 dynamic ONNX export and applies
two mechanical graph rewrites so every Resize executes on the GPU under
DirectML (they are also faster on CPU):

1. The refinenet 2x and head 1.75x bilinear upsamples take constant `scales`
   instead of dynamically computed `sizes`. Output sizes and the
   align_corners coordinate mapping are unchanged; the result is bit-level
   equivalent (observed max difference 9.5e-07 on raw depth).
2. The learned 37x37 positional-embedding table is resampled with linear
   instead of cubic interpolation. DirectML executes no cubic Resize, and
   this single node otherwise costs more CPU time than the rest of the graph
   combined (about 274 ms vs about 92 ms GPU time per 924x518 frame).
   Measured effect on normalised depth: MAE about 0.3%, p99 about 1.4%.

Usage:
    python tools/build_accelerated_model.py [--input UPSTREAM.onnx]
        [--output Resources/Models/depth_anything_v2_vits_dml.onnx]
        [--self-test]

`--self-test` requires onnxruntime and compares both models on synthetic
input with the CPU provider.
"""

import argparse
import hashlib
import sys
from pathlib import Path

import onnx

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_INPUT = (
    REPOSITORY_ROOT / "build" / "depthgen_upstream" / "depth_anything_v2_vits_dynamic.onnx"
)
DEFAULT_OUTPUT = REPOSITORY_ROOT / "Resources" / "Models" / "depth_anything_v2_vits_dml.onnx"

# Pinned provenance. Keep in sync with cmake/DepthGenAssets.cmake,
# Source/DepthGen_ModelIntegrity.h, Resources/Models/model-manifest.json,
# docs/MODEL_PROVENANCE.md, and THIRD_PARTY_NOTICES.md.
UPSTREAM_SHA256 = "46c4e8eeda3a27f34701831b6a2ec7753d7b38779b215acb5633424703deed8f"
EXPECTED_OUTPUT_SHA256 = "237cfaaf329bc97b9914c14e2d2497b1159cc05cca1b6d7a68aa42a262ea99bf"

CONSTANT_SCALES = {
    "/depth_head/refinenet1/Resize": [1.0, 1.0, 2.0, 2.0],
    "/depth_head/refinenet2/Resize": [1.0, 1.0, 2.0, 2.0],
    "/depth_head/refinenet3/Resize": [1.0, 1.0, 2.0, 2.0],
    "/depth_head/Resize": [1.0, 1.0, 1.75, 1.75],
}
POSITIONAL_RESIZE = "/Resize"  # cubic -> linear


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def convert(input_path: Path, output_path: Path) -> None:
    model = onnx.load(str(input_path))
    graph = model.graph

    resize_names = {node.name for node in graph.node if node.op_type == "Resize"}
    missing = (set(CONSTANT_SCALES) | {POSITIONAL_RESIZE}) - resize_names
    if missing:
        raise RuntimeError(f"upstream graph changed; missing Resize nodes: {sorted(missing)}")

    for node in graph.node:
        scales = CONSTANT_SCALES.get(node.name)
        if scales is not None:
            for attribute in node.attribute:
                if attribute.name == "mode" and attribute.s != b"linear":
                    raise RuntimeError(f"{node.name} is not a linear Resize")
                if attribute.name == "coordinate_transformation_mode" and attribute.s != b"align_corners":
                    raise RuntimeError(f"{node.name} is not align_corners")
            initializer = onnx.helper.make_tensor(
                name=f"depthgen_const_scales_{node.name.strip('/').replace('/', '_')}",
                data_type=onnx.TensorProto.FLOAT,
                dims=[4],
                vals=scales,
            )
            graph.initializer.append(initializer)
            while len(node.input) < 4:
                node.input.append("")
            node.input[1] = ""
            node.input[2] = initializer.name
            node.input[3] = ""
        elif node.name == POSITIONAL_RESIZE:
            for attribute in node.attribute:
                if attribute.name == "mode":
                    attribute.s = b"linear"

    onnx.checker.check_model(model)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    onnx.save(model, str(output_path))


def self_test(input_path: Path, output_path: Path) -> None:
    import numpy as np
    import onnxruntime as ort

    rng = np.random.default_rng(42)
    height, width = 518, 924
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    image = np.stack(
        [xx / width, yy / height, 0.5 + 0.5 * np.sin((xx / width + yy / height) * 6.2831853)],
        axis=0,
    )
    image += rng.normal(0.0, 0.02, image.shape).astype(np.float32)
    mean = np.array([0.485, 0.456, 0.406], dtype=np.float32)[:, None, None]
    dev = np.array([0.229, 0.224, 0.225], dtype=np.float32)[:, None, None]
    tensor = ((image - mean) / dev)[None].astype(np.float32)

    def run(path):
        session = ort.InferenceSession(str(path), providers=["CPUExecutionProvider"])
        return session.run(None, {"image": tensor})[0][0]

    def normalise(values):
        span = max(values.max() - values.min(), 1e-6)
        return (values - values.min()) / span

    difference = np.abs(normalise(run(input_path)) - normalise(run(output_path)))
    mae = float(difference.mean())
    maximum = float(difference.max())
    print(f"self-test vs upstream: MAE={mae:.6f} max={maximum:.6f}")
    if mae > 0.01 or maximum > 0.05:
        raise RuntimeError("converted model deviates more than the documented tolerance")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT,
                        help="verified upstream FP32 dynamic ONNX export")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT,
                        help="destination for the accelerated model")
    parser.add_argument("--self-test", action="store_true",
                        help="compare against the upstream model with onnxruntime CPU")
    parser.add_argument("--expect-output-sha256", default=EXPECTED_OUTPUT_SHA256,
                        help="fail unless the written file matches this digest")
    args = parser.parse_args()

    if not args.input.is_file():
        print(f"error: upstream model not found: {args.input}", file=sys.stderr)
        return 1
    input_digest = sha256_file(args.input)
    if input_digest != UPSTREAM_SHA256:
        print(f"error: upstream SHA-256 mismatch: {input_digest}", file=sys.stderr)
        return 1

    convert(args.input, args.output)
    output_digest = sha256_file(args.output)
    print(f"wrote {args.output}")
    print(f"output sha256: {output_digest}")
    if args.expect_output_sha256 and output_digest != args.expect_output_sha256:
        print("error: output hash does not match the pinned digest; "
              "update the pin deliberately if the toolchain changed", file=sys.stderr)
        return 1

    if args.self_test:
        self_test(args.input, args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
