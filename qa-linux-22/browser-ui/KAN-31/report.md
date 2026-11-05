# KAN-31 / GitHub #2546

分類: **限定条件で未再現**

新規profileで標準about:preferences初回proxy検索にW3Cキー入力。初回から接続設定カードを画面確認。正しい#connectionSecurity訪問後も結果保持。Firefox159.0.1も同様。初期治具の誤URL#connectionはsyncに戻ったため、その試行を最終比較から除外。

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。binaryとsource SHAのbyte単位同一性は未証明。

環境: Debian13.6 x86_64, Xvfb :92, isolated DBus; KWin6.3.6 nested Wayland for visual/layout/controls; software WebRender。W3C・構成イベント・OS XTestは全て合成入力で、物理入力ではない。KWin nestedは完全なPlasma/原ディストリではない。保護やTLS/署名検証は無効化していない。

実行: `PYTHONPATH=python python3 run.py floorp 29284 search`

既存PR: 直接対応PR未確認（不存在証明ではない）

残条件:
- Fedora44/元配布形式
- 起動直後のより早期入力/他言語

証拠:
- `../search-29291.json`
- `../search-29292.json`
- `../KAN-31/29291-first.png`
- `../KAN-31/29292-first.png`

状態・Sprint所属・Doneは変更しない。

Jira追記comment ID: `10285`。needs-linuxは実試験によりlinux-testedへ置換し、原ディストリ/物理入力の具体条件とenv-blockedは保持。

## Fedora44 container追加試験

実Fedora44 Container Imageのuserlandで依存を隔離導入し、host Xvfb/X11に接続した公式Floorp12.21.1を一般ユーザーで実行。共有host kernelと仮想画面を使用し、元Fedora desktop/Flatpak/物理入力とは異なる。securityを無効化するenv/flagは使用していない。初回XPCOM起動はlibX11-xcb不足でexit255、その依存を補って製品起動と試験に成功。

初回proxy検索とconnectionSecurity訪問後の検索で結果表示（search-29411.json）。

Jira最終comment ID: `10285`。
