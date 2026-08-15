# After Effects 用 DepthGen

[English](README.md) · [简体中文](README_zh_CN.md) · [한국어](README_ko_KR.md)

DepthGen は、ソースレイヤーから **相対深度** マップを生成する MIT ライセンスの
After Effects エフェクトです。Apache-2.0 の Depth Anything V2 Small と ONNX
Runtime を使用します。初期設定では白が近景、黒が遠景です。

単一画像だけから実距離を保証することはできないため、用途はコンポジット、深度
マット、深度対応エフェクトです。メートル単位の距離や時間追跡は提供しません。

## 主な機能

- 任意順・Multi-Frame Rendering 対応の決定論的なフレーム単位 SmartFX レンダー
- 8/16/32-bpc 出力とソース alpha の保持
- Fast 392px、Balanced 518px（既定）、High 700px、Custom の推論品質
- 遠景／近景 percentile、コントラスト、反転、sRGB／リニア入力、alpha-aware levels
- 対応ランタイムでは Windows DirectML、macOS Core ML を優先し、失敗時は CPU へフォールバック
- 英語、日本語、簡体字中国語、韓国語の UI

## インストール

配布物はバイナリだけでなく `Resources/Models` と ONNX Runtime ライブラリを含めた
ディレクトリ全体で配置してください。Windows は `DepthGen.aex`、macOS は
`DepthGen.plugin` をそのリソースとともに AE の Plug-ins フォルダへコピーし、AE を
再起動します。

## 操作

`Quality` は推論短辺を選びます。`Far Clip` と `Near Clip` は黒／白へ割り当てる
robust percentile（既定 2%／98%）です。`Contrast=1.0` は中立、`Invert Depth` は
白近景の向きを反転します。`Show Advanced Controls` でカスタム解像度、入力 transfer、
alpha の扱いを表示します。

ビルド、モデルの検証、第三者ライセンスは [docs/BUILD.md](docs/BUILD.md)、
[docs/MODEL_PROVENANCE.md](docs/MODEL_PROVENANCE.md)、
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。
