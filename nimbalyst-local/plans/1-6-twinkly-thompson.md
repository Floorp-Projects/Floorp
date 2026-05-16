# インタラクティブチュートリアル実装計画

## Status: COMPLETED ✓

## Context

Steps 1-5 の実装完了後のレビューで、チュートリアルが「テキストのみ」であることが判明。計画書の「実際にジェスチャーを描かせて操作感を体験」などのインタラクティブ要素が未実装。設定ページ内で完結するインタラクティブデモを3つ作成する。

---

## Phase 1: TutorialStep 型拡張 + TutorialModal 改修

### TutorialStep に `content` フィールド追加

**修正ファイル:** `browser-features/pages-settings/src/components/common/tutorial-modal.tsx`

```typescript
export interface TutorialStep {
  titleKey: string;
  descriptionKey: string;
  image?: React.ReactNode;
  content?: React.ReactNode;  // ← NEW: インタラクティブコンテンツ領域
}
```

TutorialModal のレンダリング順序:
1. `step.image` があれば画像表示（既存）
2. `step.content` があればフル幅で表示（新規）
3. `t(step.titleKey)` と `t(step.descriptionKey)` は常時表示

`content` がある場合、モーダルを `max-w-lg` → `max-w-2xl` に広げてインタラクティブ領域に十分なスペースを確保。

---

## Phase 2: マウスジェスチャー体験キャンバス

**新規ファイル:** `browser-features/pages-settings/src/components/common/gesture-canvas.tsx`

### GestureCanvas コンポーネント

ユーザーがマウスドラッグで軌跡を描き、方向シーケンスを検出するインタラクティブキャンバス。

**実装方針:**
- `<canvas>` または SVG でマウス軌跡を描画
- `onPointerDown` → `onPointerMove` → `onPointerUp` でポイント記録
- 移動距離が閾値（sensitivity相当）を超えたら方向判定:
  - dx > threshold && dy < threshold/2 → `right`
  - dy > threshold && dx < threshold/2 → `down`
  - 斜め方向（dx, dy 両方閾値超え） → `upRight`/`upLeft`/`downRight`/`downLeft`
- `onGestureComplete(directions: GestureDirection[])` コールバック
- トレイル描画はSVG `<polyline>` で実装（canvasよりReactとの親和性高い）
- ユーザーの gesture 設定（trailColor, trailWidth）を読み込んで反映

**Props:**
```typescript
interface GestureCanvasProps {
  onGestureComplete: (directions: GestureDirection[]) => void;
  targetPattern?: GestureDirection[];  // 正解パターン（任意）
  trailColor?: string;
  trailWidth?: number;
  className?: string;
}
```

**ビジュアル:**
- 灰色背景のキャンバス領域（高さ200px程度）
- 中央に「ここにドラッグしてジェスチャーを描いてください」のプレースホルダー
- ドラッグ中は色付きの軌跡がリアルタイム表示
- 検出された方向はキャンバス下にバッジ表示（↑↓←→）
- `targetPattern` が指定されている場合、一致すれば ✓ 成功表示

### ジェスチャーチュートリアルステップ（5ステップ）

**修正ファイル:** `browser-features/pages-settings/src/app/gesture/page.tsx`

```
Step 1: (text) 「マウスジェスチャーとは」
  - 右クリックしながらドラッグして操作を実行する機能の説明
  - content: なし（テキストのみ）

Step 2: (interactive) 「基本的なジェスチャーを体験」
  - content: GestureCanvas with targetPattern=["down","up"]
  - ユーザーが ↓↑（タブを閉じる）を描く練習
  - 正解したら次へ進める

Step 3: (interactive) 「進む・戻るジェスチャー」
  - content: GestureCanvas with targetPattern=["left","right"]
  - ←→（戻る）を描く練習

Step 4: (interactive) 「フリージェスチャー」
  - content: GestureCanvas (targetPatternなし)
  - 自由に描いて検出結果を確認
  - 検出されたパターンに対応するアクション名を表示

Step 5: (text) 「カスタマイズ」
  - 設定ページでジェスチャーの追加・変更ができることを説明
  - content: なし
```

---

## Phase 3: Split View レイアウトサンドボックス

**新規ファイル:** `browser-features/pages-settings/src/components/common/splitview-sandbox.tsx`

### SplitViewSandbox コンポーネント

ユーザーがレイアウトを変更し、ペインを追加・削除できるインタラクティブプレビュー。

**実装方針:**
- 模擬ブラウザウィンドウ（外枠 + タブバー + ツールバー + コンテンツ領域）
- コンテンツ領域に選択中のレイアウトで分割表示
- 各ペインに模擬コンテンツ（色付きプレースホルダー + タイトル）
- 「レイアウト変更」ボタン群：7つのレイアウトから選択可能
- ペイン数の増減（+/- ボタン、maxPanes制限）
- LayoutSettings のSVGアイコンを再利用

**Props:**
```typescript
interface SplitViewSandboxProps {
  onLayoutChange?: (layout: SplitViewLayout) => void;
}
```

**ビジュアル:**
- 外枠: 角丸ボーダー付きボックス（width: 100%, aspect-ratio: 16/10）
- ヘッダー: 模擬タブバー（2-4個のタブ + +ボタン）
- コンテンツ: flexbox/grid でレイアウト分割
  - horizontal: flex-row
  - vertical: flex-col
  - grid-2x2: grid 2x2
  - 3pane系: grid with span
- 各ペイン: 背景色違いのカード + 模擬テキスト行
- 下部: レイアウト選択ボタン（アイコン + ラベル）

### Split View チュートリアルステップ（4ステップ）

**修正ファイル:** `browser-features/pages-settings/src/app/splitview/page.tsx`

```
Step 1: (text) 「Split Viewとは」
  - 複数タブを同時に画面分割表示する機能の説明
  - content: なし

Step 2: (interactive) 「レイアウトを選択」
  - content: SplitViewSandbox
  - ユーザーが7つのレイアウトから選んで違いを体験
  - ペイン数も変更可能

Step 3: (text) 「使い方」
  - タブを右クリック→「Split Viewで開く」の手順説明
  - content: なし

Step 4: (interactive) 「ペインサイズの調整」
  - content: SplitViewSandbox（リサイズ可能な仕切り線付き）
  - ドラッグで仕切り線を動かして比率を変更
```

---

## Phase 4: ワークスペース ガイド付きウォークスルー

**新規ファイル:** `browser-features/pages-settings/src/components/common/workspace-guide.tsx`

### WorkspaceGuide コンポーネント

ワークスペースはブラウザクローム側の操作のため、設定ページ内ではシミュレーション不可。代わりに**視覚的な図解アニメーション**で操作手順を説明。

**実装方針:**
- 模擬ブラウザウィンドウ（タブバー + ツールバー + サイドバー）
- CSS アニメーションで操作手順を自動再生:
  1. タブバーにワークスペースアイコンが表示される（ハイライト点滅）
  2. クリックするとワークスペース一覧が展開
  3. ワークスペースを選択するとタブが切り替わる
- アニメーションはループ再生、ユーザーがクリックで手動ステップ送りも可能

**Props:**
```typescript
interface WorkspaceGuideProps {
  step?: number;  // 0-2のアニメーションステップ
}
```

### ワークスペースチュートリアルステップ（3ステップ）

**修正ファイル:** `browser-features/pages-settings/src/app/workspaces/page.tsx`

```
Step 1: (text+animation) 「ワークスペースとは」
  - タブをグループ化して切り替える機能の説明
  - content: WorkspaceGuide (自動アニメーション)

Step 2: (text) 「基本的な使い方」
  - ワークスペース切り替え、タブの移動、アイコン割り当て
  - content: WorkspaceGuide (step=1, クリックでステップ送り)

Step 3: (text) 「アーカイブと管理」
  - 使わないワークスペースのアーカイブ、初期化
  - content: なし
```

---

## 翻訳キー追加

### pages-settings en-US.json / ja-JP.json に追加

```json
"tutorial": {
  "stepOf": "Step {{current}} of {{total}}",
  "previous": "Previous",
  "next": "Next",
  "finish": "Finish",
  "learnHowToUse": "Learn How to Use",
  "gestureCanvas": {
    "placeholder": "Click and drag here to draw a gesture",
    "detected": "Detected: {{pattern}}",
    "matched": "Matched: {{action}}",
    "success": "Correct! Well done!",
    "tryAgain": "Try Again"
  },
  "splitviewSandbox": {
    "changeLayout": "Change Layout",
    "addPane": "Add Pane",
    "removePane": "Remove Pane",
    "pane": "Pane {{n}}"
  }
}
```

ジェスチャー各ステップの titleKey/descriptionKey は既存キーを再定義:
```json
"mouseGesture.tutorial.step1.title": "What are Mouse Gestures?",
"mouseGesture.tutorial.step1.description": "Hold right-click and drag...",
"mouseGesture.tutorial.step2.title": "Try a Basic Gesture",
"mouseGesture.tutorial.step2.description": "Draw ↓↑ to close a tab...",
...（以下同様）
```

---

## ファイル一覧

| 操作 | ファイルパス |
|------|------------|
| **修正** | `pages-settings/src/components/common/tutorial-modal.tsx` — TutorialStep.content 追加 + max-width 可変 |
| **新規** | `pages-settings/src/components/common/gesture-canvas.tsx` — ジェスチャー描画キャンバス |
| **新規** | `pages-settings/src/components/common/splitview-sandbox.tsx` — Split View レイアウトサンドボックス |
| **新規** | `pages-settings/src/components/common/workspace-guide.tsx` — ワークスペース図解アニメーション |
| **修正** | `pages-settings/src/app/gesture/page.tsx` — チュートリアルステップを5ステップに拡張 |
| **修正** | `pages-settings/src/app/splitview/page.tsx` — チュートリアルステップを4ステップに拡張 |
| **修正** | `pages-settings/src/app/workspaces/page.tsx` — チュートリアルステップを3ステップに拡張 |
| **修正** | `pages-settings/src/lib/i18n/locales/en-US.json` — 翻訳追加 |
| **修正** | `pages-settings/src/lib/i18n/locales/ja-JP.json` — 翻訳追加 |

## 再利用する既存コード

| コンポーネント | パス | 用途 |
|---------------|------|------|
| `patternToString` | `gesture/dataManager.ts` | 方向→矢印文字変換 |
| `GestureDirection` 型 | `types/pref.ts` | 方向の型定義 |
| `SplitViewLayout` 型 | `types/pref.ts` | レイアウトの型定義 |
| LayoutSettings のSVG | `splitview/components/LayoutSettings.tsx` | レイアウトプレビューSVG |

## 検証方法

1. `feles-build dev` で起動
2. `about:hub#/features/gesture` → 「使い方を学ぶ」→ 5ステップ中 Step 2-4 でキャンバスに描画できる
3. `about:hub#/features/splitview` → 「使い方を学ぶ」→ Step 2 でレイアウト変更・ペイン追加削除ができる
4. `about:hub#/features/workspaces` → 「使い方を学ぶ」→ Step 1 でアニメーションが再生される

---

# === 以下: 旧計画（実装済み） ===

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
