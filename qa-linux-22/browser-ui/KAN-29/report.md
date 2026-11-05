# KAN-29 / GitHub #2876

分類: **限定条件で未再現**

KWin nested Wayland、LTR、縦タブhoverとpanel rail左右4組合せを測定。展開後の5標本中、遷移後は幅253.33pxと端位置が安定。panel本体は閉じた条件。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 layout`

既存PR: https://github.com/Floorp-Projects/Floorp/pull/2877

残条件:
- CachyOS/Flatpak
- panel本体open・反復hover・ピン留め差
- PR2877適用時のRTL/boxless比較

証拠:
- `../layout-29289.json`
- `../KAN-29/layout-False-True.png`

状態・Sprint所属・Doneは変更しない。
