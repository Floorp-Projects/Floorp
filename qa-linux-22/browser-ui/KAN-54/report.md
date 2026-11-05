# KAN-54 / GitHub #3020

分類: **表示重複を実観測・Sprint対象外**

KWin nested Wayland/Lepton、標準menu表示、horizontal/multirow×inTitlebar0/1を比較。multirow+inTitlebar1でmenuとtabbarに各1つのclose群を画面確認。horizontalはmenu側のみ。実画面にmin/maxは出ないnested条件。仕様違反とは未判定。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 controls`

既存PR: 直接対応PR未確認（不存在証明ではない）

残条件:
- Manjaro/完全なPlasma session
- minimize/maximize含む3ボタン/仕様確認

証拠:
- `../controls-29289.json`
- `../KAN-54/controls-multirow-1.png`
- `../KAN-54/controls-horizontal-1.png`

状態・Sprint所属・Doneは変更しない。
