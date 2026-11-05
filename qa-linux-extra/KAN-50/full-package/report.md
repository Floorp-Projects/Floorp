# KAN-50 Draft3042 完全パッケージ検証

## 対象とCI

対象はFloorp `9d042bc17be83c15d2ff3a25eb712f04c4d3eb45`。標準CI [42084373774](https://github.com/Floorp-Projects/Floorp/actions/runs/42084373774) はattempt1でsuccess。追加AppWindow browser-colocated試験を含む282モジュール失敗0、downloaded Firefox8files/16tasks成功を生成ログartifact13441309797の実ZIPから照合。artifact ZIP SHA256 `ac8c1d3de34882f340df6f38de7660c70b05442ff31990bf2e6af62d72c1f1a2`。全PRチェックはsuccessまたは意図したskipで、CI修正・rerun不要。小さな原観測は `ci/artifact-verification.json` / `ci/checks.json`。

## 完全製品の来歴

既存same-head package.yml runが0であることとリモートheadを直前確認し、[42086912660](https://github.com/Floorp-Projects/Floorp/actions/runs/42086912660) を1回だけworkflow_dispatch。既存package-only `stable-v2-validation` / Linux-x64 / beta=false / skip_signing=true / signed_validation=false。workflowのPublish dist/MARはActions artifact保存であり、本番release/publish workflowは呼ばない。

Runtimeは真正 `8965ffc346d426ccdf919bd18585e4d4a0c21654`、daily run41664957250。公式manifest artifact13274392012実ZIP SHA256 `2b6a1b20190dc7016957ca25f72f8e83f267de51458b515153fd209bb90d9138` を照合し、manifest中Linux/x86_64 target13272530069 / SHA256 `ed5a4f97ee7bf42c817f3146d5a843e095710dcc3be25fae0af0bf47274b739a` / expected Runtime BuildID20261103153232を使用。Release tarball等の別scopeのdigestで代用していない。`provenance/summary.json`はdispatch前の読み取り調査スナップショットで、そのdispatched=falseは当時の状態。実dispatchは `dispatch-result.json`。

この経路は真正prebuilt native Runtimeを利用してFloorpフロントエンドを構築・注入し、source common patchを適用し、extracted JS patch側でも同じ修正の既適用を逆適用チェックで確認して完全installerを生成する。Gecko nativeの再コンパイルを行ったとは主張しない。今回の修正はJavaScriptのみでnative再ビルド不要。前回の診断用1-resource copyとは別の検証。

## 試験方法

`verify-package.py` でActions ZIP全体digestと唯一のtar.xzを検証して新領域へ安全に展開。実 `omni.ja` の `moz-src/toolkit/components/pictureinpicture/PictureInPicture.sys.mjs` と、locked Runtime sourceへsource patchを適用した期待バイト列を照合。source/extractedの両patchは実出荷memberの独立fixtureへreverse --checkし、製品自体へは変更しない。全製品file/symlink inventoryを試験前後で比較。

`run-regression.sh` は `product-regression.py` を4つのfresh profile/private DBus/Xvfb/Openboxセッションで実行する。通常先/パネル先のopen順それぞれに、通常先/パネル先のclose順を組み合わせる。localhostの同一60秒VP8/WebVTT実動画、native video context-menu PiP、XTestによる実close controlを使用。read-only frame-scriptで観測しPiP handlerのmockやJS resource差替えを行わない。実EWMH/WM稼働とwindowState2をassertし、共有count2→1→0、残PiPの活動・caption進行、最後のcount/attribute解除、同一PID、sandbox6前後維持を測る。字幕の進行は修正前でも確認済みで、字幕停止を原欠陥とは主張しない。

環境は `environment.json` の実Debian13.6/Cloud kernel6.18.57/X11/software GPU。原Ubuntu/native GPU/Waylandや物理入力を資格化するものではない。実YouTube CONNECT403は既知で再試験・迂回しない。profile/cookie/資格情報/一時署名URL/巨大binary/動画本体はexportしない。元Linux22/C-five matrixは不変、追加課題として扱う。

## 結果

**全4経路、合計52/52チェック成功。** Workflowはattempt1/success、CIの失敗修正・rerun・重複dispatchは0。完全installer artifact **13444572566** (`noraneko-linux-x86_64-installer-unsigned`、108192761 bytes) のZIP SHA256は `1a0b538f14730130a63ac283e94465ca302781525ac44d2c6b91479418271aa0`。内側 `floorp-linux-x86_64.tar.xz` は108174124 bytes、SHA256 `584e4afd9865df3a5867b145ed75b7c3051a0a26d173da240056a4b355bfcad9`。digestのscopeを区別。

source patch適用をbuild log行977、extracted guardの既適用確認を行4297、最終installer確認を行6061で取得。実出荷PiPモジュール SHA256 `6d44e3319e1dbc555ffecb85157c1703eff470f08a021f1443cfbd4244b69fae` はsource期待値と完全一致、両patchのreverse --check成功。`build-evidence.json` と `package-verification.json` に生観測。

| 開く順 | 先に閉じた側 | チェック | 実最小化中の動画frames | PID |
|---|---|---|---|---|
| normal-first | normal | 13/13 | 182→231 | 61718維持 |
| normal-first | panel | 13/13 | 278→326 | 62883維持 |
| panel-first | normal | 13/13 | 279→327 | 61722維持 |
| panel-first | panel | 13/13 | 182→230 | 62882維持 |

全経路でrootMap2→1→未登録、最後のplayer/window count0、normal/panelのnative pictureinpicture属性なし、minimized windowState2、root active/forced=true、source visible、実clone=true/paused=falseとframes増加、WebVTT text変化。残PiPが通常/パネルの両方を確認。forceAppWindowActive/PiP moduleの例外0。同じnative process PIDを前後保持しsandbox6維持。今回の結果にnative browser-test JSを追加注入していない。小さなresult.jsonにPID/BC/count/frames/captionsを保存し、`verify-results.py`は配布物inventoryのbinary hashと全rawのsource/binaryを突き合わせ再検算。独立read-onlyレビューで指摘されたbinary結合と動画frames条件を初回実行前に強化済み。

展開直後と全4試験後の**全46 file/symlink entriesが完全一致**。inventory SHA256は両方 `3a201b3f8b18ab2cb23502b5f10d3417f8b61e79a7470c740037c00c1b77380e`。製品resourceの差替え・mock・保護無効化なし。`product-immutability.json`、`product-files-before.json` / `product-files-after.json` で検算可能。ネイティブfloorp/libxulは公式12.21.1とも同一hash。

外側application.ini/platform.iniのpackage timestamp BuildIDは**20261105184721**、実プロセスServices.appinfoのversionは159.0.1/Runtime BuildID **20261103153232**。外側INIには当該application.ini未使用の注記があり、両値を取り違えない。Floorp表示バージョン12.21.1@159.0.1、修正ソース9d042bc。真正Runtimeの実行は後者とnative hashesで照合。

これはローカル合成サイトの完全製品資格であり、原YouTube/Ubuntu/native GPU/Waylandや他OSの合格ではない。既知YouTube403再試験0。無関係なRemoteSettingsネットワーク/embedded PanelUI console出力は製品全体のconsole-cleanとは主張しない。終了後のchannel-error出力はharnessによる意図したprocess停止に続くもので、試験中クラッシュとは扱わない。字幕停止は今回も再現していない。旧missing-gBrowserは既存2699、現行activity/unload欠陥はDraft3042として分離。

## 再検算と再試験

allowlist exportから `python verify-results.py <export-root>` で4つのraw JSONとinventory/immutabilityを再検算できる。元installerをGitHub artifact13444572566から取得すれば、`verify-package.py --zip <artifact.zip> --zip-sha256 sha256:1a0b538f14730130a63ac283e94465ca302781525ac44d2c6b91479418271aa0 --out <new-dir> --expected-module expected-module.sys.mjs --patch-repo <checkout-at-9d042bc>` で元ZIP/実member/両patchを検証できる。

`run-regression.sh <new-dir>/extracted/floorp/floorp <fresh-output>` は依存path/portsを環境に合わせて調整する。QA_MARIONETTE_PYTHON/QA_VIDEO_FILEでMarionette venv/同一動画を指定。生成fixtureは `ffmpeg -f lavfi -i testsrc2=size=480x270:rate=24 -t 60 -c:v libvpx -b:v 350k -an <video.webm>` 相当、実バイト列hashは各rawに記録。今回用いた動画はtimestamp/frame番号を焼き込んだ同一ローカルfixtureであり、別生成バイト列との同一hashを主張しない。Xvfb/Openbox/xdotoolとライブラリ/テーマdata dirsはenvironment/runnerで明記。
