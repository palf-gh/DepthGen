#!/usr/bin/env python3
"""Build DepthGen's portable ZipDepth ONNX model.

The input is the MIT-licensed ZipDepth NPU checkpoint from the pinned
upstream commit. The exporter fuses inference-time branches and replaces three
shape-specialised forwards with equivalent dynamic-shape operations so one
IR-v8 / opset-17 graph supports every aspect ratio and quality size.

The NPU checkpoint is intentionally used because its unfold-free convex
upsampler maps cleanly to DirectML and Core ML.
"""

import argparse
import hashlib
import sys
import types
from pathlib import Path

import torch
import torch.nn.functional as F


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
UPSTREAM_COMMIT = "94da7527f7030a0e79d54f33b113bdce4065d735"
CHECKPOINT_SHA256 = "627c04fda584133ead4310074884a4a037061b4c01ba86e73e492ea30fab570d"
EXPECTED_OUTPUT_SHA256 = "0741a0d574609da33c5081b1054a2dd1e8845ecdbed9a5f69c48807c22400d59"
DEFAULT_SOURCE_ROOT = REPOSITORY_ROOT / "build" / "depthgen_upstream" / "ZipDepth"
DEFAULT_CHECKPOINT = DEFAULT_SOURCE_ROOT / "checkpoints" / "zipdepth_base_npu.pth"
DEFAULT_OUTPUT = REPOSITORY_ROOT / "Resources" / "Models" / "zipdepth_base_npu_dynamic.onnx"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_model(source_root: Path, checkpoint_path: Path):
    sys.path.insert(0, str(source_root))
    from zipdepth.model.architecture import create_model
    from zipdepth.utils.model_utils import fuse_remaining_conv_bn, strip_state_dict_prefixes

    model = create_model(variant="base", global_mode="balanced", upsample_unfold=False)
    checkpoint = torch.load(checkpoint_path, map_location="cpu", weights_only=True)
    state = checkpoint.get("model_state_dict", checkpoint)
    missing, unexpected = model.load_state_dict(strip_state_dict_prefixes(state), strict=False)
    if missing or unexpected:
        raise RuntimeError(
            f"pinned ZipDepth checkpoint does not match the model: "
            f"missing={missing}, unexpected={unexpected}"
        )
    model = model.cpu().eval()
    model.fuse_for_inference()
    fuse_remaining_conv_bn(model)
    return model


def enable_dynamic_export(model) -> None:
    # The upstream static exporter replaces these blocks with fixed H/W
    # pooling kernels. Equivalent reductions keep H/W dynamic and avoid the
    # unsupported adaptive-pooling path in ONNX.
    for module in model.modules():
        if type(module).__name__ == "GlobalContextBlock":
            def global_context_forward(self_module, tensor):
                context = tensor.mean(dim=(2, 3), keepdim=True)
                return tensor + self_module.transform(context)

            module.forward = types.MethodType(global_context_forward, module)

        elif type(module).__name__ == "StripPoolingAttention":
            def strip_pooling_forward(self_module, tensor):
                gate = self_module.gate_conv(
                    tensor.mean(dim=3, keepdim=True) +
                    tensor.mean(dim=2, keepdim=True)
                )
                return tensor * gate

            module.forward = types.MethodType(strip_pooling_forward, module)

    cross_scale = model.encoder.cross_scale

    def cross_scale_forward(self_module, high, low):
        low_up = F.interpolate(
            self_module.low_to_high(low), scale_factor=2.0, mode="nearest"
        )
        high_down = F.avg_pool2d(self_module.high_to_low(high), 2, 2)
        return high + low_up * 0.3, low + high_down * 0.3

    cross_scale.forward = types.MethodType(cross_scale_forward, cross_scale)


def export_model(model, output_path: Path) -> None:
    enable_dynamic_export(model)
    dummy = torch.randn(1, 3, 384, 384)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with torch.no_grad():
        torch.onnx.export(
            model,
            dummy,
            output_path,
            input_names=["image"],
            output_names=["depth"],
            dynamic_axes={
                "image": {2: "height", 3: "width"},
                "depth": {2: "height", 3: "width"},
            },
            opset_version=17,
            do_constant_folding=True,
            dynamo=False,
        )


def self_test(model, output_path: Path) -> None:
    import numpy as np
    import onnxruntime as ort

    generator = torch.Generator().manual_seed(42)
    tensor = torch.rand((1, 3, 384, 672), generator=generator)
    with torch.no_grad():
        expected = model(tensor).cpu().numpy()
    session = ort.InferenceSession(str(output_path), providers=["CPUExecutionProvider"])
    actual = session.run(None, {"image": tensor.numpy()})[0]
    difference = np.abs(expected - actual)
    maximum = float(difference.max())
    mean = float(difference.mean())
    print(f"self-test vs fused PyTorch: MAE={mean:.8f} max={maximum:.8f}")
    if mean > 1.0e-5 or maximum > 1.0e-3:
        raise RuntimeError("exported ZipDepth model exceeds the parity tolerance")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=DEFAULT_SOURCE_ROOT)
    parser.add_argument("--checkpoint", type=Path, default=DEFAULT_CHECKPOINT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument(
        "--expect-output-sha256",
        default=EXPECTED_OUTPUT_SHA256,
        help="fail unless the written model matches this digest",
    )
    args = parser.parse_args()

    if not (args.source_root / "zipdepth" / "model" / "architecture.py").is_file():
        print(f"error: pinned ZipDepth source not found: {args.source_root}", file=sys.stderr)
        return 1
    if not args.checkpoint.is_file():
        print(f"error: ZipDepth NPU checkpoint not found: {args.checkpoint}", file=sys.stderr)
        return 1
    checkpoint_digest = sha256_file(args.checkpoint)
    if checkpoint_digest != CHECKPOINT_SHA256:
        print(f"error: checkpoint SHA-256 mismatch: {checkpoint_digest}", file=sys.stderr)
        return 1

    model = load_model(args.source_root, args.checkpoint)
    export_model(model, args.output)
    output_digest = sha256_file(args.output)
    print(f"wrote {args.output}")
    print(f"output sha256: {output_digest}")
    if args.expect_output_sha256 and output_digest != args.expect_output_sha256:
        print(
            "error: output hash does not match the pinned digest; "
            "use the documented exporter environment",
            file=sys.stderr,
        )
        return 1
    if args.self_test:
        self_test(model, args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
