現行Linux実UI検証完了（2026-11-06 JST）。分類: 背景paint余白あり、現行Linuxで新しい意図しないFloorp欠陥は特定せず。原Windows10/Floorp12.17.0条件は未検証で、未再現/既修正/Doneを全体へ断定しない。

main29a3eb23145da90706d8a94441373278521cc737、公式Floorp12.21.1/Gecko159.0.1/build20261103153232とFirefox159.0.1をDebian13.6/private Xvfb101+Openbox/X11で実行。可視UIラベルでProton/Lepton/Fluerialを選び、checked/保存値/実描画/高さ・iconsを確認。39配置snapshotと5 OS合成pointer control、別CSSOM cascade検証を記録。物理入力ではない。メニュー/タイトルバー構成fixtureはテーマUI選択と区別。

全Floorpdesignの外tab hitbox上端0px、背景paint上端4px、Firefoxは0px/6px。menubar可視はrow全体+25px。最初のminimal sessionはOS titlebar19/20pxだったが、実WMに対応するXDG_CURRENT_DESKTOP=openboxをprocess限定で与えるとnative GTK CSD/customtitlebar有効・frame extents0。最大化両EWMHatomとGecko状態を照合し、真のscreenY0 OSclickで別QA tabをFloorp/Firefoxとも選択、center対照も成功。Windows CSS media強制なし・保護検証無効化なし。

Fluerialbase margin-block0と実測4pxの違いは、後続FLUERIAL_TAB_CORNER_CSSの強いID selectorが4px fallback!importantを指定するため。sourcecommentにtop gap復元が明記され、実機CSSOMでloaded sheet/selector.matches/priority/computed4pxを確認。任意8px drag-space prefはdefault未設定で別経路。現在mainのfull製品rebuild（無関係2934 sidebar修正のみ、KAN95fix候補ではない）も1条件同じ4px・topedge成功。

既存merged2854は別#2822 titlebar表示修正で2670修正の根拠にしない。推測CSS変更/新PRなし。残: 原Windows10・native titlebar/窓設定・12.17→current比較・物理入力。needs-repro維持、具体条件needs-windowsを付記。status/parentは変更なし。

取得可能な実測JSON/39paint screenshots/5control/cascade/source hashes/再現script: https://github.com/Floorp-Projects/Floorp/tree/2c0aeaa8/qa-linux-additional/browser-ui/KAN-95 。追加表のみ、元Linux22の分母維持。
