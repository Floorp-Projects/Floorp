実製品の切り分けと固有欠陥の候補提出（2026-11-06 JST）。原2692はWindows11/Floorp12.17.0@154.0・HuggingFace model-card動画。現在のHTTP fixtureと同じ原因とは未証明。原Windows症状を再現/解消したとは扱わない。Jira10093、GH原文/comment0、関連2244、既存PR2678/2467/2447等を照合し重複回避。

現main29a3eb23145da90706d8a94441373278521cc737の公式12.21.1/Gecko159.0.1と、既存3031+3034 full strict候補をDebian13.6/private Xvfb117+Openbox/正常native focusで実行。設定UIを実W3Cで有効化し、OS XTestのup/downで正常page/overflowとsame/cross-origin iframeを比較。認識ラベルは正しく出る。両製品ともroot+focusedoverflow4/4gesture成功、iframe4/4はframe1800pxのまま親rootだけ0/7083へ動く。方向認識・activationとは別の送信先欠陥。Firefox CtrlHome/Endも同じ8条件すべて正しい端点へ動き、4製品全32keyboard対照成功。既存2678はactor opt-in履歴だが、今回root成功の原因帰属は旧版対照なしでは確定しない。

Draft3047 https://github.com/Floorp-Projects/Floorp/pull/3047 、source d6bd2fc457774f55ab1f36a2b01c941f908f1ab0、3files113追加/1削除。フォーカス中BCのtopが選択tabと一致する時だけそのframeへ送り、他tab/window/null時は選択tabへfallback。actor whitelist・距離・方向・保護は変更しない。

候補はcompiled gestures module1entryだけを変えた明示診断copy（完全source-built packageではない）。ownGreD/module URI/hashを実測。gesture8/8+keyboard8/8成功、非対象root/overflow/frame位置不変、全24gestureのmenu0、各Floorp8inputs=8dispatch。7browser policy case成功。old-route adapterのframe/nested2assertFAILは副証拠で、literal old module試験ではなく、実baselineOS失敗を主証拠とする。全56primary観測監査済み。ログ名衝突の回収再走はsource変更なしで候補だけ実施しprimary件数へ加算せず。

署名falseは公式製品既定/userSet=false、sandbox6。無効化してPASSを作っていない。共有製品hash不変、全own process終了・自担当終了profileのみ整理。CI42096334025はsmoke/ARM/Windows/mac native検証成功、Linux combined browser進行中。再実行なし。

残: 原Windows11/元web page・profile/focus構造、物理入力、完全production package。元22分母は維持し追加別表。取得可能なreport/matrix/repro/位置JSON/paint/dispatch抜粋/hash/source.patch: https://github.com/Floorp-Projects/Floorp/tree/3d313f3b/qa-linux-additional/browser-ui/KAN-97 。status/parent/Done/merge/release変更なし。
