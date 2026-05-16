# Floorp UX改善計画 — 機能説明・イントロダクション強化

## Context

Floorpブラウザは強力な独自機能（Panel Sidebar、Workspaces、Split View、マウスジェスチャー、PWAなど）を持つが、ユーザーへの機能紹介・説明が不十分。初回オンボーディングは充実しているものの、初回以降の継続的なユーザー教育や、その場で助けられる仕組みが不足している。特にSplit Viewは完全に未紹介。

---

## Step 1: Split View の設定ページ・ウェルカム追加

**最も明白な欠落を最初に修正する。**

### 1-1. Split View 設定ページを新規作成

**新規ファイル:** `browser-features/pages-settings/src/app/splitview/page.tsx`

- 既存パターンに従う（sidebar/page.tsx をテンプレート）
- 内容:
  - ページ説明：「複数のタブを同時に画面分割で表示」
  - レイアウト選択（水平/垂直/グリッド）の設定
  - ペインサイズの調整（Seekbarコンポーネント使用）
  - 使い方への外部リンク
- `dataManager.ts` を作成し、`floorp.splitView.config`, `floorp.splitView.paneSizes` をRPCで操作

### 1-2. 設定ページナビゲーションに追加

**修正ファイル:** `browser-features/pages-settings/src/components/app-sidebar.tsx`
- サイドバーにSplit View項目を追加

### 1-3. ウェルカムページの機能紹介にSplit Viewを追加

**修正ファイル:** `browser-features/pages-welcome/src/app/features/page.tsx`
- Split View用の説明カード追加（画像・説明・箇条書き）

**新規ファイル:** `browser-features/pages-welcome/src/app/features/assets/splitview.svg`

### 1-4. 翻訳追加

**修正ファイル:** 以下の `en-US.json` および `ja-JP.json`
- `browser-features/pages-settings/src/lib/i18n/locales/en-US.json`
- `browser-features/pages-settings/src/lib/i18n/locales/ja-JP.json`
- `browser-features/pages-welcome/src/lib/i18n/locales/en-US.json`
- `browser-features/pages-welcome/src/lib/i18n/locales/ja-JP.json`

**翻訳キー構造:**
```json
"splitView": {
  "title": "Split View",
  "description": "View multiple tabs simultaneously in a split layout",
  "features": ["View 2-4 tabs at once", "Multiple layouts: horizontal, vertical, grid", "Customizable pane sizes"],
  "layoutSettings": "Layout Settings",
  "layoutDescription": "Choose default arrangement for split view panes",
  "sizeSettings": "Pane Sizes",
  "sizeDescription": "Adjust the relative size of each pane"
}
```

---

## Step 2: 機能発見（Feature Discovery）メカニズム

**オンボーディング後もユーザーが新機能や未知の機能に気づける仕組み。**

### 2-1. Tip of the Day システムの実装

**新規ファイル群:** `browser-features/chrome/common/feature-tips/`

- `tip-manager.ts` — tipの表示管理（一度見たtipは二度表示しない、`floorp.featureTips.seen` preferenceで管理）
- `tip-registry.ts` — tipデータの定義
- `components/tip-banner.tsx` — 表示用バナーコンポーネント

tipデータ例:
```typescript
{ id: "split-view", title: "Split Viewでマルチタスク",
  description: "タブを右クリック→「Split Viewで開く」で...", feature: "splitView" }
```

### 2-2. 新規タブページにTip表示エリアを追加

**修正ファイル:** `browser-features/pages-newtab/` の該当コンポーネント

- ページ下部に「ヒント」セクションを追加
- 表示/非表示設定あり（ユーザーが閉じられる）
- クリックで該当機能の設定ページへ遷移（`about:hub` 該当セクション）

### 2-3. 設定ページサイドバーに「新機能」バッジ

**修正ファイル:** `browser-features/pages-settings/src/components/app-sidebar.tsx`

- 新規機能や更新された機能に「New」バッジを表示
- `floorp.featureTips.seen` preference で管理

### 2-4. 翻訳追加

各tipテキストを `en-US.json`, `ja-JP.json` に追加。

---

## Step 3: インタラクティブチュートリアル

**HighlightManager（既存）を活用し、ガイド付きチュートリアルを実現。**

### 3-1. チュートリアルエンジンの作成

**新規ファイル群:** `browser-features/chrome/common/tutorial/`

- `tutorial-manager.ts` — チュートリアルのステップ管理（開始・進行・完了・中断）
- `tutorial-overlay.ts` — Solid.jsオーバーレイ
  - ハイライト対象要素の周囲を暗くする（HighlightManagerパターン）
  - ステップ番号・説明テキスト・次へ/前へボタン
  - ポップオーバー型の説明パネル

### 3-2. チュートリアルコンテンツ定義

優先度高のチュートリアル：
1. **マウスジェスチャー体験** — 実際にジェスチャーを描かせて操作感を体験
2. **ワークスペース基本操作** — ワークスペース作成→タブ移動→切替
3. **Split View基本操作** — タブの右クリック→Split View→レイアウト変更

各チュートリアルは設定ページの「使い方を学ぶ」ボタンから起動。

### 3-3. 設定ページにチュートリアル起動ボタンを追加

**修正ファイル:** 各設定ページ（gesture, workspaces, splitview）
- ページ上部に「使い方を学ぶ」ボタンを追加
- クリックでブラウザ上のチュートリアルオーバーレイを起動

---

## Step 4: 高度な機能説明・アニメーション追加

### 4-1. 設定ページに「機能ガイド」セクション追加

各設定ページの上部に、その機能の概要を説明するガイドセクションを追加：

- **Rest Mode** — キーボードショートカット設定ページ内に説明追加
- **コンテナ統合** — Panel Sidebar設定内に説明追加
- **Lepton テーマオプション** — Design設定内に各テーマの違いを視覚的に表示

### 4-2. CSSアニメーションによる機能デモ

Split Viewの分割アニメーション、ジェスチャーの軌跡アニメーションなどをCSS/SVGアニメーションで実装し、ウェルカムページ・設定ページに埋め込み。

**修正ファイル:** `browser-features/pages-welcome/src/app/features/page.tsx`
- 動的機能（ジェスチャー、ワークスペース切替、Split View分割）はCSSアニメーションで簡易デモ表示

---

## Step 5: 設定ページのコンテキストヘルプ強化

**既存のInfoTipコンポーネントを活用し、説明不足の設定項目にヘルプテキストを追加。**

### 5-1. HelpSection コンポーネントの作成

**新規ファイル:** `browser-features/pages-settings/src/components/common/help-section.tsx`

InfoTip（1行ツールチップ）では不十分な場合に使う、展開可能なヘルプセクション：
```tsx
<HelpSection summary={t("feature.quickHelp")}>
  <p>{t("feature.detailedHelp")}</p>
</HelpSection>
```

### 5-2. 各設定ページにヘルプセクションを追加

- **`app/gesture/page.tsx`** — デフォルトジェスチャー一覧
- **`app/workspaces/page.tsx`** — アイコン割り当て、アーカイブ機能
- **`app/design/page.tsx`** — Lepton/Proton/Photonテーマの違い
- **`app/sidebar/page.tsx`** — Firefoxサイドバーとの違い、コンテナ機能

---

## 実装順序と優先度

| 順序 | Step | 内容 | 規模 |
|------|------|------|------|
| 1 | Step 1 | Split View設定ページ・ウェルカム追加 | 中 |
| 2 | Step 2 | 機能発見メカニズム | 大 |
| 3 | Step 3 | インタラクティブチュートリアル | 大 |
| 4 | Step 4 | 高度な機能説明・アニメーション | 中 |
| 5 | Step 5 | 設定ページのコンテキストヘルプ強化 | 中 |

---

## 再利用する既存コンポーネント・パターン

| コンポーネント | パス | 用途 |
|---------------|------|------|
| `InfoTip` | `pages-settings/src/components/common/infotip.tsx` | ツールチップヘルプ |
| `Card/CardHeader/CardDescription` | `pages-settings/src/components/common/card.tsx` | セクション枠 |
| `Seekbar` | `pages-settings/src/components/common/seekbar.tsx` | 数値設定スライダー |
| `ExternalLink` パターン | 各設定ページ | 外部ドキュメントリンク |
| `HighlightManager` | `modules/actors/webscraper/HighlightManager.ts` | チュートリアルのハイライト |
| `ModalParent` | `chrome/common/modal-parent/` | チュートリアルダイアログ |
| データマネージャパターン | `pages-settings/src/app/sidebar/dataManager.ts` | RPC設定読み書き |
| DaisyUI tooltip | `data-tip` 属性 | シンプルなツールチップ |

---

## 検証方法

1. **Step 1:** `about:hub` → Split View設定ページが表示される。ウェルカムページにSplit Viewが追加されている。
2. **Step 2:** 新規タブページにヒントが表示され、クリックで設定ページに遷移する。設定サイドバーに「New」バッジが表示される。
3. **Step 3:** 「使い方を学ぶ」ボタンからチュートリアルが起動し、ステップごとにハイライトが移動する。
4. **Step 4:** 各機能ガイドが正しい内容で表示される。アニメーションが滑らかに動作する。
5. **Step 5:** 各設定ページのInfoTip/HelpSectionがホバー・クリックで正しく表示される。

各Step完了時に `feles-build dev` で起動して手動確認。
