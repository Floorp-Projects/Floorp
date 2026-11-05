# KAN-93 / GitHub #2712

分類: **限定条件で未再現**

KWin nested WaylandのFloorpとFirefox159.0.1でlight/darkのchrome色・opacity・画面を比較。lightは文字rgb47,47,52/opacity1、nav白背景。薄い文字・iconの原症状は見られない。Firefoxはstock theme対照でありFirefox154+Leptonではない。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 visual`

既存PR: 直接対応PR未確認（不存在証明ではない）

残条件:
- KDE Neon/実theme・accent・font条件
- 元版12.17.1/Firefox154+Lepton

証拠:
- `../visual-29289.json`
- `../visual-29295.json`
- `../KAN-93/chrome-0.png`
- `../KAN-93/Firefox-chrome-0.png`

状態・Sprint所属・Doneは変更しない。
