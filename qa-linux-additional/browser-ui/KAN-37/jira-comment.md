追加実製品QA（2026-11-06 JST）: main29a3eb23145da90706d8a94441373278521cc737、公式Floorp12.21.1/159.0.1とFirefox159.0.1対照、Debian13.6/private Xvfb99/101、合成W3C/XTest。GH2754のLinux/Windows12.18.0とGH2997/既存Jira10084を照合。関連3003は別NewTab再設計で修正確定ではない。

Floorp StartをProton/Fluerial/Leptonの実UI選択で比較。initial/reload/native cache clear、Leptonは実process restartも実行。計10条件・70画像decodeチェック成功。remote2、製品bundled4、local4色favicon1の各画像を確認し、10枚の実paintで4色pixel一致。localはnative pinnedLinks APIによる永続data fixtureでUI作成/HTTP取得ではない。DOM src/CSS差替なし。初期remote失敗はCloudブラウザーのproxy未設定で、隔離profileの通常proxyと配布CA正規trust後に成功。TLS/sandbox検証を無効化していない。

原報告画像に近いclassic NewTabは、通常UI disable Floorp Start+再起動/通常Ctrl+Tでもroot全体が空。Firefox対照も同じで、タイル画像のみ空白の元症状とは異なるnative初期化不足。原条件未再現成立とはせず、部分試験とする。Windows/原Linux desktop/元classic profile/物理入力は未実施。Lepton変更で原issue解決と断定しない。新規欠陥を推測してPR化しない。

分類: 部分検証。Floorp Startの画像取得・描画・cache・restartは試験範囲で正常、元classic条件は未検証。証拠・screenshots・script: https://github.com/Floorp-Projects/Floorp/tree/32b1b13b/qa-linux-additional/browser-ui/KAN-37 （Linux22と別）。status/parent/Doneは変更しない。
