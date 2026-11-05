# KAN-39 / GitHub #2736

分類: **正規化実測・原症状の解消は未確定**

30タブstackで合成pixel120→120px、line1→84.87px、page1→1181px。OS XTest1ノッチはdeltaMode1/deltaY±6、stackは約509.2px移動。通常tabbarでは同入力が届いたが位置不変で、公平な速度比較は成立していない。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 wheel`

既存PR: https://github.com/Floorp-Projects/Floorp/pull/2763

残条件:
- Fedora44/物理wheel・DPI/加速
- 通常tabbarとの有効な同入力距離比較

証拠:
- `../wheel-29284.json`
- `../wheel-os-29296.json`
- `../KAN-39/harness-at-end.json`
- `../KAN-39/stack.png`

状態・Sprint所属・Doneは変更しない。
