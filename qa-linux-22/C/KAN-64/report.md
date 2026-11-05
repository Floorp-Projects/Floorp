# KAN-64 / Floorp #2568 — 実再現、拡張との連携条件

2026-11-05、担当C。対象ソースは最新main `984509d2053be7bae9027133bda7259cd706b208`。GitHub原報告・既存コメントとJira本文・コメントを照合した。Issue番号による既存PR検索は0件（不存在の証明ではない）。新規PR・製品コード変更なし。

## 結果

**現行公式Floorp 12.21.1 / Gecko 159.0.1 と、AMO署名済み Tree Style Tab 4.4.9 で症状を実再現した。** Workspace A内の2タブをネイティブグループ `KAN64-GROUP-A` にまとめ、Workspace Bへ切り替えると、Aのメンバータブは両方非表示になり、水平タブバーのグループも非表示になる。しかしTSTサイドバーには `KAN64-GROUP-A` のヘッダーだけが残る。`workspace-a.png` と `workspace-b.png` を比較できる。

実行はDebian GNU/Linux 13、Linux kernel 6.18.57、Xvfb/X11、専用DBus session、新規HOME・XDG・プロファイル。**原報告のFedora 44 / KDEではない。** 物理入力ではなく、Marionette経由で実際の製品のタブ・Workspace APIを操作した。画面は実製品のスクリーンショット。KDEや物理入力の成功とは扱わない。

## 観測と責任範囲

- Aでグループ内の2タブは `hidden=false`、グループのinline `display=""`。
- Bへ切り替えると2タブはともに `hidden=true`、グループのinline `display="none"`。グループ自身のDOM `hidden` はfalse。
- 同じ署名済みTSTを公式Firefox 159.0.1（BuildID `20261102144711`）へ導入し、選択中タブの外に置いたネイティブグループの2タブを `gBrowser.hideTab()` で非表示にした対照試験でも、TSTのヘッダーが残る。`firefox-after.png`。Firefox側では水平タブバーにもヘッダーが残る。FirefoxにはWorkspace機能がないため、これはWorkspace再現試験ではなく、同一のタブ非表示通知に対する拡張表示の対照試験。
- 実際に実行したFloorpの `browser/omni.ja` から取り出した `ext-tabGroups.js` のconvert関数は `collapsed,color,id,title,windowId` のみを返す。対応する `tabGroups.json` にgroup visibility/hidden契約はない。CSS `display:none` を拡張へ渡す標準group fieldはない。
- 署名済みTST 4.4.9の `sidebar/sidebar-items.js` はTAB_HIDDEN通知で個々のタブを非表示にする。一方 `TabGroup` とvirtual-scrollの実装には「全メンバーがhiddenならグループヘッダーを隠す」処理が見当たらず、画面観測と一致する。これでTST全コードの欠陥を網羅的に証明したわけではないが、Floorp固有の非表示通知欠落を支持しない。

したがって区分は **実再現 + TST/上流との連携依存**。Floorp単独の確定欠陥や修正済みとは扱わない。標準にないgroup hidden APIを追加したり、署名済み拡張を書き換えたりして合格を作らない。Floorpの最小修正を正当化する根拠がないため、重複・推測のDraft PRは作成しない。次はこの再現fixtureをTST側の「全メンバーhidden時のgroup header表示」仕様・テストと照合する。

## 保護・検証条件

公式リリースアーカイブとAMO配布XPIを使用。`Addons.install(..., temp=False)` で通常インストールした。コンテンツsandbox無効化環境変数は渡していない。`acceptInsecureCerts=false`。製品が要求する `MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1` はMarionetteによるchrome検査専用のopt-inで、拡張署名やWebセキュリティの無効化ではない。最終試験の `probe.json` は拡張の `signedState=2`、`isActive=true`、content `sandboxLevel=6` を記録。`xpinstall.signatures.required=false` はFloorp自身の出荷時既定値（対象SHAの `static/gecko/pref/override.ini:49`）であり、試験で変更していない。署名必須設定がtrueだったとは主張しない。署名済みXPIのAMOハッシュとの一致とAddonManagerの署名状態を別途確認した。

保存環境の `/workspace/.floorp-cloud/env.sh` には `MOZ_DISABLE_CONTENT_SANDBOX=1` が含まれていたためsourceせず、必要なライブラリパスだけを指定した。初期の制限付きコマンドでネットワークsocketやuser namespaceが拒否されたが、許可された実行経路で再測定してnamespaceとDBusは利用できた。これを製品バグやLinux非対応とは判定していない。

## 再実行・証拠

専用領域から `python3 /workspace/qa-linux-22/C/KAN-64/probe.py`。同じ実行権限・ローカルsocketを許可する環境で実行する。依存は隣の `python/`（marionette_driver）、署名済み `treestyletab.xpi`、共有の公式 `/workspace/qa-linux-22/C/products/floorp/floorp`、Xvfbとユーザー空間sysroot。共有日常プロファイルを使用しない。対照は `firefox-control.py`。製品・XPIの取得元は `release.json` と `tst-addon.json`、ハッシュは `sha256.txt`。

- `probe.json`, `probe.log`, `browser.log`, `environment.json`: 実観測・設定・環境。
- `workspace-a.png`, `workspace-b.png`, `workspace-b-final.png`: 切替前後と繰返し確認。
- `firefox-control.json`, `firefox-control.log`, `firefox-before.png`, `firefox-after.png`: Firefox対照。
- `packaged-ext-tabGroups.js`, `packaged-tabGroups.json`: 実行製品のAPI契約。
- `tst-native-tab-groups.js`, `sidebar_sidebar-items.js`, `common_TreeItem.js`, `common_tabs-store.js`, `common_tabs-update.js`: 署名済みXPIからの調査用抜粋。
- `source-0.json`, `source-1.json`, `source-2.json`: 原issue、既存PR検索、最新Jiraの取得記録。

Fedora 44/KDEそのもの、報告当時のFloorp 12.16.2・当時のTST版、物理入力は未試験。現行版の症状はDebianでも再現済みなので、Linux環境がない・単なる未再現とはしない。マージ・リリース・Done化は行っていない。
