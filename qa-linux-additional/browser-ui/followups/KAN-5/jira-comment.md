## KAN-5：既存窓への個別stack proxyの実OSドラッグ追加追試

**分類：限定Linux条件で未再現・既存PR2763と整合。** 30055で通常タブ対照と個別2段目proxyの既存窓dropが2/2成功。既存main＋無関係KAN97修正の完全製品packageを使用し、KAN5修正候補とはしていない。GreD実読一致、描画/コード差替なし。

Debian13.6/X11/Xvfb123/Openboxの実normal2窓。Hubの可視labelでstackを有効化→正常quit0/restart、3tabを実Ctrl多選択、native NewGroup menuの実キー選択、実name textboxへQA KAN5入力/Enterでgroup1/proxy3を作成。group/adopt/detach API・作成DragEvent/手動DataTransferなし。

OS XTest synthetic入力で通常tabはsource/target5/1→4/2、proxy Bは4/2→3/3。両方trusted source dragstart/target drop、expectedURL一回のみ移動、全URI/title multisetがbefore/1s/3sで不変、全nonpending、窓数2を保持。元の既知about:blankの空titleだけを正しく許容し、QA文書のtitleは全て保持。sourceにはA/Cの2member stack、targetにはBの実paintを画面確認。

setup/final browserとも正常exit0、final probe0。全製品56entryのtype/mode/contenthashが前後不変、root独立照合も成功。終了ownprofileのみ/proc参照なしを確認して削除、display123も終了。先行30051〜30054はtab overflow/menu座標/作成editor Escape取消/Linuxで無効なCtrl9という治具停止として区別し、製品失敗/OSdrop結果には数えない。最終は製品内定義に一致するAlt9で対照tabを実UI選択。

Fedora44原desktop/Flatpak/物理dragは未達のまま。OS XTestをphysical入力と呼ばない。既存2763正本、新defect/newPRなし、原envラベル/status/Doneを変更しない。既存KAN5一件の追加証拠であり分母は増やさない。

証拠 followups/KAN-5/report.md、matrix.json、summary.json、drop-30055.json、normal/final-quit JSON、payload manifests/comparison、root-independent-checks.json、cleanup-receipt.json、実screen/reproduction scripts。