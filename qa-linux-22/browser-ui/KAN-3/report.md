# KAN-3 / GitHub #2567

分類: **実再現・Floorp固有CSS欠陥・最小修正の製品回帰試験成功**

Source: `984509d2053be7bae9027133bda7259cd706b208`。公式Floorp12.21.1 / Gecko159.0.1 / build20261103153232、独立profile。配布binaryとsource SHAのbyte単位同一性は未証明。

## 実施試験

Debian13.6 x86_64、Xvfb :92、隔離DBus、X11、software WebRender。製品保護を無効化せず起動。入力はMarionette chrome DOM/APIによる合成操作。KDE/原Fedora/物理ドラッグ試験ではない。

2組のnative splitを作成、一組を`replaceGroupWithWindow`で別windowへ移動し`adoptTabGroup`で戻す。Floorpでは移動先の第2titleが空になり、復帰後もページ状態を失う。公式Firefox159.0.1対照は全titleを保持。

## 原因と修正

Floorp split-view CSSが非current paneへ`display:none !important`を設定する。別splitへ選択が移ると、移動対象のbrowser frameがGeckoのbrowser swap前に破棄される経路がある。該当CSSを`position:absolute !important; visibility:hidden !important`へ変更してframeを維持し、非current paneをlayoutから除く。

複数のJS override/SessionStore/splitter診断では改善せず、`data-floorp-split` CSS抑止と該当selector単独overrideで改善。診断binaryの改変なし。

## 回帰試験

sourceにbrowser colocated testを追加。2組split、別window移動、元window帰還を行い、両documentのtitleとURLを照合。source CSSをimportし元/移動先のstyleに適用するため、公式製品上でも修正差分を独立評価できる。

- 原CSS: `regression-29316.json` **期待どおり失敗**。移動先第2titleが空。
- 修正CSS: `regression-29315.json` **成功**。両方向のtitle/URL保持。
- Deno typecheck、lint（新規2test files）、git diff --check成功。
- 完全なFloorpビルド/全browser suiteは未実施。実製品の既存runtime上でsource CSS差分と回帰testを実行した結果。

実行（esbuildをsourceのnode_modulesで解決できない場合、`ESBUILD_MODULE=/absolute/path/to/esbuild`を指定。Marionette Python packageと公式製品をQA rootのpython/productsへ配置。QA rootから。製品は保護を有効にしたまま実行可能なnamespace権限が必要）:

```sh
FLOORP_SOURCE=/path/to/fix-checkout node KAN-3/build-test.cjs KAN-3/regression-new.js
FLOORP_SOURCE=/path/to/fix-checkout QA_CSS_OLD=1 node KAN-3/build-test.cjs KAN-3/regression-old.js
PYTHONPATH=python python3 run-kan3-regression.py floorp 29315 regression
QA_REGRESSION_JS=regression-old.js PYTHONPATH=python python3 run-kan3-regression.py floorp 29316 regression
```

既存PR: 直接対応PR未確認（不存在証明ではない）。新Draft PR #3033を公開済み。修正commit `1a75d15d2925c9d5bf58964505937d07a35df54c`。

残条件: 原Fedora44、物理ドラッグ/メニュー経路、正式ビルドと広範囲suite、native Wayland。これらを本試験成功で満たしたと扱わない。

証拠: `../split-29285.json`（Floorp原再現）、`../split-29286.json`（Firefox対照）、`../split-29310.json`（selector override成功）、`../regression-29315.json`、`../regression-29316.json`、`regression-new.log`、`regression-old.log`、`typecheck.log`、`before.png`、`returned.png`。

状態・Sprint所属・Doneは変更しない。

## Fedora44 container追加試験

実Fedora44 Container Imageのuserlandで依存を隔離導入し、host Xvfb/X11に接続した公式Floorp12.21.1を一般ユーザーで実行。共有host kernelと仮想画面を使用し、元Fedora desktop/Flatpak/物理入力とは異なる。securityを無効化するenv/flagは使用していない。初回XPCOM起動はlibX11-xcb不足でexit255、その依存を補って製品起動と試験に成功。

同じSplit View往復でtitle空化を再現（split-29412.json）。

## 追加layout・pointer回帰（Draft3033）

追加commit `e7dda31f`。split↔split→通常tab→split復帰を、移動前と元window帰還後に確認。

- browser regression `regression-29426.json`: 成功。active pane幅/高さ、非重なり、computed visibility、中心elementFromPoint、hidden paneのhit除外、selectedTab維持をassert。
- 実測 `layout-29424.json`: 8状態/14回のW3C pointer clickが全成功。各clickごとのcontent title増分を確認。合成入力で物理操作ではない。
- tabpanels幅1238px。split各pane約610–612px、通常tab1238px。split中の非current paneはabsolute/hiddenでcenter hitなし。通常tab時の非current paneはnative deck隠蔽のためcomputed visibilityはvisibleだがhitなし。CSSのhidden条件とnative deck条件を区別。
- 新testのtypecheck/lint/diff-check成功。追加headの標準CI結果は統括側で別確認。

実測再実行:

```sh
QA_SOURCE_CSS=/path/to/fix-checkout/browser-features/chrome/common/split-view/styles/split-view.css PYTHONPATH=python python3 run-kan3-layout.py floorp 29424 layout
```

追加証拠: `../layout-29424.json`, `../regression-29426.json`, `layout-probe.py`, `../run-kan3-layout.py`, `layout-w3c-final.log`, `layout-regression-final.log`, `typecheck-layout.log`, `lint-layout.log`。

標準CI: 初期head1a75d15dは282モジュール/0失敗+Firefox16tasks、artifact digest/内容確認済み。追加head e7dda31fのCIはsnapshot時queued（ローカル拡張回帰と14clickは成功）。

Jira最終comment ID: `10294`。
