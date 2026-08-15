# After Effects 的 DepthGen

[English](README.md) · [日本語](README_ja.md) · [한국어](README_ko_KR.md)

DepthGen 是一个 MIT 许可的 After Effects 效果，可从源图层生成 **相对深度** 图。
它使用 Apache-2.0 的 Depth Anything V2 Small 和 ONNX Runtime。默认情况下白色表示
较近、黑色表示较远。

它适用于合成、深度遮罩和深度感知效果，并不保证真实世界的米制距离或时间跟踪。

## 功能

- 与任意帧顺序和 Multi-Frame Rendering 兼容的确定性逐帧 SmartFX 渲染
- 8/16/32-bpc 输出并保留源 Alpha
- Fast 392px、Balanced 518px（默认）、High 700px 和自定义推理尺寸
- 远近百分位映射、对比度、深度反转、sRGB/线性输入和 Alpha-aware levels
- 已编译支持时优先 Windows DirectML 或 macOS Core ML，失败时回退 CPU
- 英语、日语、简体中文和韩语控件

## 安装与使用

请完整复制发行目录，包括 `Resources/Models` 和 ONNX Runtime 库，而不是只复制
`DepthGen.aex` 或 `DepthGen.plugin`。`Far Clip` 与 `Near Clip` 默认以 2%/98% 映射到
黑/白；`Contrast=1.0` 为中性；`Invert Depth` 反转默认白近黑远的方向。

构建、模型验证与第三方许可请参阅 [docs/BUILD.md](docs/BUILD.md)、
[docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md) 和
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
