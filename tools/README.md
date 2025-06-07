# Noraneko Build Tools

この `tools/` ディレクトリには、Noranekoプロジェクトのビルドと開発に関するすべてのツールが含まれています。

## 📜 履歴

> **移行について**: このディレクトリは以前の `scripts/` ディレクトリから移行されました。プロジェクトの成長に伴い、より良い組織化と拡張性のために `scripts/` から `tools/` へとリネームされ、構造が改善されました。すべての機能は保持され、新しいモジュラーアーキテクチャで強化されています。

## 📁 ディレクトリ構造

```
tools/
├── build/                      # ビルドシステム
│   ├── phases/                 # ビルドフェーズ
│   │   ├── pre-build.ts        # Mozilla ビルド前の処理
│   │   └── post-build.ts       # Mozilla ビルド後の処理
│   ├── tasks/                  # 個別のビルドタスク
│   │   ├── inject/             # コード注入タスク
│   │   ├── git-patches/        # Git パッチ適用
│   │   └── update/             # アップデート関連
│   ├── index.ts                # メインビルドオーケストレーター
│   ├── defines.ts              # パスと定数の定義
│   ├── logger.ts               # ロギングユーティリティ
│   └── utils.ts                # 共通ユーティリティ
│
└── dev/                        # 開発ツール
    ├── launchDev/              # 開発環境起動
    ├── prepareDev/             # 開発環境準備
    ├── cssUpdate/              # CSS更新ツール
    └── index.ts                # 開発ツールエントリーポイント
```

## 🚀 使用方法

### ビルドシステム

```bash
# 開発ビルド
deno run -A build.ts --dev

# プロダクションビルド
deno run -A build.ts --production

# Mozilla ビルドをスキップして開発ビルド
deno run -A build.ts --dev-skip-mozbuild
```

### 開発ツール

```bash
# 開発サーバー開始
deno run -A tools/dev/index.ts start

# 開発環境クリーンアップ
deno run -A tools/dev/index.ts clean

# 開発環境リセット
deno run -A tools/dev/index.ts reset
```

## 🔧 主な改善点

### 1. **明確な関心の分離**
- `build/` : ビルド関連の全ての機能
- `dev/` : 開発関連のツールとユーティリティ

### 2. **モジュラー設計**
- フェーズベースのビルドシステム
- 再利用可能なタスク
- 独立したユーティリティ

### 3. **保守性の向上**
- 明確なインポートパス
- 一貫したエラーハンドリング
- 包括的なロギング

### 4. **開発者体験の向上**
- 直感的なCLIインターフェース
- ヘルプメッセージ
- エラーメッセージの改善

## 🔄 移行ガイド

### 旧 `scripts/` からの変更点

| 旧パス | 新パス |
|--------|--------|
| `scripts/build.ts` | `tools/build/build.ts` |
| `scripts/defines.ts` | `tools/build/defines.ts` |
| `scripts/inject/` | `tools/build/tasks/inject/` |
| `scripts/launchDev/` | `tools/dev/launchDev/` |
| `scripts/prepareDev/` | `tools/dev/prepareDev/` |

### インポートパスの更新

```typescript
// 旧
import { log } from "./scripts/logger.ts";

// 新
import { log } from "./tools/build/logger.ts";
```

## 📝 今後の改善予定

1. **TypeScript設定の統一**
2. **テストの追加**
3. **CI/CD統合の改善**
4. **ドキュメントの拡充**
5. **パフォーマンス最適化**
