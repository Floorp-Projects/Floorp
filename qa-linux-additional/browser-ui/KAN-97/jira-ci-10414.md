CI追補: Draft3047 / source d6bd2fc457774f55ab1f36a2b01c941f908f1ab0 の通常CI42096334025が全job成功。smoke、Linux x64/ARM・Windows・macOS native資材検証、Linux browser統合が成功しました。

artifact13447489059/test-logs-ubuntu-24.04 ZIPのSHA256 f42b0a5134fb0545ab498ce9b22e12a57980335e3f758c0ae2f9845752d1bef1がGitHub digestと一致。scrollTarget.test.ts実行pass（7ms）、全282 test modules成功/0fail、Firefox8files/16tasks成功をartifact実内容で確認。7policy caseはsourceで無条件登録・awaitされ、harnessが集約失敗をthrowする。artifactはmodule単位のpass記録であり個別case行を作っていません。再CIなし。

CI実行: https://github.com/Floorp-Projects/Floorp/actions/runs/42096334025 。digest付きsummary/検証抜粋は専用QA branchのKAN-97/ci-3047/に保存。詳細の実OS before/after・候補範囲・原条件は前コメント10412のとおりです。原Windows11/HuggingFace原因一致・物理入力・新修正の完全production packageは未検証を維持。候補はcompiled-module診断copyとして明記。全工程でmerge/release/Doneなし、元22の分母維持。
