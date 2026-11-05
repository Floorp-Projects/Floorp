# KAN-87 / GitHub #2691

分類: **限定条件で未再現・既存修正と整合**

XvfbとKWin nested Waylandでlight/dark、normal/Zenを比較。Zenのnavigator-toolboxはalpha1の背景（light rgb235,235,237）で、カラフルなページ上でも背景が透けない画面を確認。右側panel railは同時表示。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 visual`

既存PR: https://github.com/Floorp-Projects/Floorp/pull/2767

残条件:
- CachyOS/Niri/Hyprland/MangoWM
- 右panel本体の同時reveal/画像theme

証拠:
- `../visual-29285.json`
- `../visual-29289.json`
- `../KAN-87/zen-0.png`
- `../KAN-87/zen-1.png`

状態・Sprint所属・Doneは変更しない。
