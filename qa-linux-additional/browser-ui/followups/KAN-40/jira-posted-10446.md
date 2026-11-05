KAN40追加対照（2026-11-06 JST）。原GH2704、Jira10287まで、merged2763本文/既存comment照合。原文はFedora44/12.17.1@154.0.2でpackage種別は未記載。別課題のFlatpak条件を転用しない。原文どおり通常tabは1件・whole-stackは全member閉鎖が期待値。

Debian13/privateXvfb124+Openbox、完全Linux package source d6bd2fc457774f55ab1f36a2b01c941f908f1ab0（KAN97のみ無関係差、middleclick候補ではない）で実OS XTest button2を測定。通常tabは7→6で対象1件のみ、whole-stack chipは12→9でそのgroup3件のみ閉鎖、元stackと他全tab保持。trusted mousedown/up/click、実pointer位置/label hit、前後paintを保存。既存member selected/nonselected試験は繰返さず。UIからstack有効化・正常quit/restart。native tab/group作成は明示gBrowser API fixtureで、close handler/inputは未改変。

通常tabのnative id空欄、復元profileの同URL重複を集計で検出。rawは保持し、通常tabは固有URL、chipはURL+実native-group multisetで独立再集計しexact1/exact3/他tab保持を確認。初期誤集計を成功根拠にしない。誤設定URLとxdotool同位置--syncの治具停止も履歴保持。最後はchipだけ限定継続、全製品通常終了0・全payload不変。自担当終了profileのみ/proc非参照確認で整理。

限定未再現・既存2763と整合、新規欠陥/重複PRなし。原Fedora desktop/元package・物理middleclickは未達。環境ラベル除去0、status/parent/Sprint/Done変更なし、merge/releaseなし。元10/全Linux22分母維持。

証拠: https://github.com/Floorp-Projects/Floorp/tree/7208c78814f2acd8e8029b90f855d1bfeffdd51f/qa-linux-additional/browser-ui/followups/KAN-40

