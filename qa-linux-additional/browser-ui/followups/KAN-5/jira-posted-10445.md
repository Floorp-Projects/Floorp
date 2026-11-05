KAN5実OS existing-window drop追補（2026-11-06 JST）。最新Jira10283まで・原GH2710/comment0・canonical merged2763の作者head b98749d110d5dd4e8cd87120c284394a047a0799と既存reviewを照合。原Fedora44/Flatpak/12.17.1@154.0.2は別条件として維持。

Debian13/privateXvfb123+Openbox、完全未改変Linux package source d6bd2fc457774f55ab1f36a2b01c941f908f1ab0（KAN97無関係修正のみ、stack候補ではない）。設定UIでstack有効化→正常quit/restart、実Ctrl多選択→native menu→group名入力/Enter確定で3proxyを作成。group/adopt/detach API、人工DragEvent/DataTransferなし。2実windowのEWMH/XIDを記録。

30055でOS XTestの通常tab positive controlと個別stack proxyを、既に存在するwindowへ実drag/drop。両方trusted source dragstart/target drop、exact1 tab移動、2window維持、1秒/3秒後も全URI/title multiset不変、QA文書loaded/titleあり・全tab nonpending。root独立Counter再検算も成功。通常はsource/target5/1→4/2、proxyは4/2→3/3、対象URIがtargetへ1件のみ移動、他member/URL/title保持。新window/遅延重複detachなし。probe/browser終了0、56payload entries全不変。

治具履歴も保存: 狭いwindowでのmultiselection、popupの座標選択、Escapeによる新group取消、Linuxの実Alt9をCtrl9としたキー誤認を段階的に切分け。先行4attemptはdrop前停止であり、製品FAIL/未再現件数へ混ぜない。実正規keybindingを読み取り最終1回で2drop成功。既存blank NewTabのtitle空をQA文書と区別するvalidator補正も明記し、全multiset/件数/trusted/nonpending条件は維持。

限定条件で未再現・merged2763と整合、重複PR/推測修正なし。直接existing-window UI routeは充足。原Fedora desktop・原Flatpak・物理dragは未達で該当ラベル保持、環境ラベル削除0。元Linux22/担当10件分母維持。status/parent/Sprint/Done/merge/release変更なし。

証拠: https://github.com/Floorp-Projects/Floorp/tree/7208c78814f2acd8e8029b90f855d1bfeffdd51f/qa-linux-additional/browser-ui/followups/KAN-5

