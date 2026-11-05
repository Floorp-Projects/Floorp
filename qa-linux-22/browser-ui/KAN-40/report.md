# KAN-40 / GitHub #2704

分類: **限定条件で未再現・既存修正と整合**

3枚stackにW3C pointer中ボタンdown/up。非選択memberのみ閉じ選択保持、続いて選択memberを閉じ残り1枚へ選択移動。画面とDOMを保存。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 middle`

既存PR: https://github.com/Floorp-Projects/Floorp/pull/2763

残条件:
- Fedora44/Flatpak/物理中クリック
- 通常タブ・stack全体中クリックの追加対照

証拠:
- `../middle-29284.json`
- `../KAN-40/before.png`
- `../KAN-40/after.png`

状態・Sprint所属・Doneは変更しない。
