追加実製品QA（2026-11-06 JST）: main29a3eb23145da90706d8a94441373278521cc737を独立worktreeで照合。公式Floorp12.21.1/Gecko159.0.1、Debian13.6/private Xvfb109+DBus、OS XTest合成pointer/keyboard（物理入力ではない）。原報告Windows10 22H2/12.20.1とは環境差あり。

現行版のhover-only/サイドバーfocusedの両Ctrl+Tabで、選択タブは変わり、sidebar :hover=true/isOpen=true/hidden=falseのままopacity0へ落ちることを実画面とtrusted event traceで再現（29922）。open PR2934 head b7866aaa4905be4d86fb3dff1237d6a70c483f10のblur→deactivateだけを診断製品copyに適用すると両ケースでopacity1維持（29923）。pointer離脱は両版とも閉じる。GreD/chrome資材root/hashを検証。full公式候補buildではなくcompiled listener差替fixture。sandbox等を無効化していない。signatureRequired=falseは製品既定/userSet=false。

既存2934が正本のため重複PRなし・既存head変更なし。小さなbrowser回帰patchを保存: final29935旧版FAIL/29936候補PASS（blur保持/deactivate閉じ/rehover/leave）。これはwindow.dispatchEvent合成DOM試験で、上記実pointer/paintとは別。通常CI組込み未確認。

残条件: 原Windows、物理入力、通常Sidebery row選択、full候補build/標準CI、qualified desktop deactivation。signed Sidebery5.6.1は通常install成功(signedState2)したがtoolbarで通常選択に入れず、明示showInitially panel選択fixtureの画面でもtab行が空白で実選択未達。元の症状と混同しない。Xvfb単体の別window focus移動はdeactivate未発生なので未合格。追加WM対照は別証拠に追記予定。

証拠・再現script・patch: https://github.com/Floorp-Projects/Floorp/tree/32b1b13b/qa-linux-additional/browser-ui/KAN-30 （Linux22と別追加表）。分類: Linux現行で再現、既存PR計算部の改善を確認。未マージ・Doneにしない。
