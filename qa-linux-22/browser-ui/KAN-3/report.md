# KAN-3 / GitHub #2567

分類: **実再現・Floorp固有CSS経路**

2組のnative splitを作成し、一組をreplaceGroupWithWindow→adoptTabGroup。移動先の最後と復帰後2枚のtitleが空。Firefox159.0.1は同手順で全titleを保持。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 split`

既存PR: 直接対応PR未確認（不存在証明ではない）

残条件:
- Fedora44/物理ドラッグ/メニュー操作
- CSS修正の回帰試験と正式ビルド

証拠:
- `../split-29285.json`
- `../split-29286.json`
- `../KAN-3/before.png`
- `../KAN-3/returned.png`

状態・Sprint所属・Doneは変更しない。
