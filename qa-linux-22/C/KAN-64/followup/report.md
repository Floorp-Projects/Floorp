# KAN-64 follow-up: TST公式契約と既存上流課題

2026-11-05。前回の実製品試験を再実行せず、公式ソースと既存上流記録を読み取りだけで照合した。外部投稿、Jira更新、製品変更、branch変更は行っていない。

## 結論

前回の「現行Floorpで実再現、Firefoxの同一TSTでも全メンバー非表示後にヘッダーが残る」は維持する。責任範囲を **TSTのnative-group表示方針とFloorp Workspaceの連携上の不足** まで絞れた。ただし **上流maintainerが認めたバグ・完全一致の既存issue・上流修正済み** とする根拠は得られていない。Floorpのタブ非表示通知欠落やFedora/KDE固有障害を支持する証拠でもない。

TSTの公式設定は「hidden tabを通常は描画しない」。これを「全メンバーhiddenならnative group headerも描画しない」という公式仕様へ拡張して断定できない。native groupはタブとは別のオブジェクトとして扱われ、ヘッダーの可視性規約は確認した公式資料に明記されていない。したがって、症状は確定しているが、現時点の区分は **外部連携上の仕様不足／TST側の可視性処理候補、上流合意待ち** が適切。

## 実行した署名済み版と公式ソースの対応

TST tag `4.4.9` はannotated tag `bb0582d503c9b77a1a2d6b9b84636ba1ff7689e6` を介し、commit [`123e698f8c96822453f4fef57e12dd5852e46fbe`](https://github.com/piroor/treestyletab/tree/123e698f8c96822453f4fef57e12dd5852e46fbe) を指す。以下3ファイルは、この固定コミットの公式ソースと前回実行したAMO署名済みXPI内部のファイルが **byte単位で一致** した。ハッシュ・照合結果・行番号付き引用を `official-source-evidence.json` に保存。

- [`common/common.js:237`](https://github.com/piroor/treestyletab/blob/123e698f8c96822453f4fef57e12dd5852e46fbe/webextensions/common/common.js#L237): `renderHiddenTabs: false` が既定。
- [`common/tabs-store.js:534`](https://github.com/piroor/treestyletab/blob/123e698f8c96822453f4fef57e12dd5852e46fbe/webextensions/common/tabs-store.js#L534): `updateVirtualScrollRenderabilityIndexForTab` は `tab.hidden && !configs.renderHiddenTabs` 等の条件で個々のタブを描画候補から除外する。
- [`sidebar/scroll.js:262`](https://github.com/piroor/treestyletab/blob/123e698f8c96822453f4fef57e12dd5852e46fbe/webextensions/sidebar/scroll.js#L262): `getRenderableTreeItems` は、上のフィルターを通過したタブ一覧に加え、`TabsStore.windows.get(windowId).tabGroups.values()` の **全グループ** をmixed listへ加える。グループ内メンバーが全てhiddenかを判定する分岐がなく、最後は `return group;`。この経路が観測された「メンバーは消え、ヘッダーだけ残る」を直接説明する。

読み取り時の公式既定branchは `trunk`、HEAD [`ad17fdfed02b357ffb73ea4dcf775c34d7a03365`](https://github.com/piroor/treestyletab/tree/ad17fdfed02b357ffb73ea4dcf775c34d7a03365)。同関数にも全グループを無条件に加える経路が残る。trunk全体は署名済み4.4.9と同一ではなく、trunkを実行・試験したとは主張しない。この静的照合だけで「上流未修正が実証された」とも言わない。

## 公式の既存課題・maintainer説明

| 参照 | 読み取れた契約・説明 | KAN-64との関係 |
| --- | --- | --- |
| [TST #3495](https://github.com/piroor/treestyletab/issues/3495)（closed/fixed） | maintainerは「TST 4 does not generate DOM nodes for hidden tabs」、後に `renderHiddenTabs` を追加しtrueならhidden tabsも表示すると説明。[該当comment](https://github.com/piroor/treestyletab/issues/3495#issuecomment-2005704670) はcommit `7c000b0` を引用。 | hidden **tabs** の既定動作を裏付ける。native group headersの全メンバーhidden時の処理は扱っておらず、同一バグではない。 |
| [TST #3617](https://github.com/piroor/treestyletab/issues/3617)（closed/wontfix/out of scope） | Zen Workspace利用者のshow-hidden-tabsトグル要望。maintainerはトグル追加を断った一方、「I'll try to solve compatibility issues with other addons instead of introducing something workaround like showing hidden tabs with options」と再現手順を求めた。[comment](https://github.com/piroor/treestyletab/issues/3617#issuecomment-2332237692) | Workspaceとhidden tabsの関連はあるが、取り残された**タブ**とUIトグルの相談。KAN-64のnative group headerそのものへの拒否・仕様確定ではない。 |
| [TST #3743](https://github.com/piroor/treestyletab/issues/3743)（closed/stale/help wanted） | maintainerは、Firefox APIからtabsとtab groupsは別リストで来て、TSTが`groupId`に基づくmixed display listを作ること、native groupsはtabsと別でfocusを持たないこと、現行native group対応が開発負担を抑える方針であることを説明。[comment](https://github.com/piroor/treestyletab/issues/3743#issuecomment-2953999572) | group headerを単なるhidden tabと同一視できない理由を裏付ける。要望自体はgroupをtab tree配下へ組み込むもので、KAN-64のduplicateではない。 |
| [TST #3877](https://github.com/piroor/treestyletab/issues/3877)（open/partially fixed/Firefox-issue） | 「Save and close group」はWebExtensions APIに手段がないため意図的に省略、とmaintainerが説明。[comment](https://github.com/piroor/treestyletab/issues/3877#issuecomment-3991677361) | native groupのclose/saveとhide/collapseは別機能。KAN-64を既存のsave/close制約に統合したり、このissueで修正済みと判断したりしない。 |

公式repo内を `"hidden" "group" in:title`、`"tab group" "hidden"`、`"renderHiddenTabs"`、`"native tab groups"` 等で検索した。取得した範囲で全メンバーhidden時のnative group header残留に完全一致する課題を特定できなかった。検索結果上限があるため、完全な不存在の証明ではない。検索語と候補一覧は `search-summary.json`、本文と関連maintainerコメントは個人メール等を含めず `related-upstream.json` に保存。

## 次の判断を具体化

上流へ将来照会する際は、Floorp固有Workspaceの説明に加え、既存の公式Firefox対照手順で「別タブを選択 → native groupの全メンバーをhide → group headerだけ残る」を提示できる。期待する変更は「`renderHiddenTabs=false` かつ全メンバーが`hidden=true`ならheaderも非描画」と明示し、maintainerに仕様合意を求める。ただし単なるcollapsed groupはhiddenとは違い、ヘッダーを残す必要がある。`renderHiddenTabs=true`、一部メンバー可視、最後の可視メンバーのhide/show、Workspace復帰時の復元も区別する。

これは今後の外部照会用の判断材料であり、投稿はしていない。上流合意なしの推測Floorp API拡張・署名済みTSTの改変・Draft PRは作成しない。
