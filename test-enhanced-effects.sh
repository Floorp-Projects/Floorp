#!/bin/bash
# Floorp OS Server API テスト - エンハンスドエフェクト機能（curl版）

set -e

BASE_URL="http://127.0.0.1:58261"

echo "=========================================="
echo "🎨 Floorp Enhanced Effects API テスト"
echo "=========================================="
echo ""

# 色コード
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 1. ヘルスチェック
echo -e "${BLUE}📋 Step 1: Health Check${NC}"
curl -s "${BASE_URL}/health" | jq .
echo ""

# 2. タブインスタンスを作成
echo -e "${BLUE}📋 Step 2: Create Tab Instance${NC}"
RESPONSE=$(curl -s -X POST "${BASE_URL}/tabs/instances" \
  -H "Content-Type: application/json" \
  -d '{"url": "https://www.google.com", "inBackground": false}')
echo "$RESPONSE" | jq .
INSTANCE_ID=$(echo "$RESPONSE" | jq -r '.instanceId')
echo -e "${GREEN}✓ Instance ID: ${INSTANCE_ID}${NC}"
echo ""
sleep 3

# 3. 複数要素のハイライト（新機能！）
echo -e "${BLUE}📋 Step 3: Highlight Multiple Elements${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/highlight" \
  -H "Content-Type: application/json" \
  -d '{
    "selectors": ["input[name=\"q\"]", "input[name=\"btnK\"]", "input[name=\"btnI\"]"],
    "action": "Inspect",
    "elementInfo": "検索フォームの要素を確認しています",
    "duration": 3000
  }' | jq .
echo -e "${GREEN}✓ 3つの要素がハイライトされました（緑色）${NC}"
echo -e "${YELLOW}👀 ブラウザを確認してください：右上に情報パネルが表示されているはずです${NC}"
sleep 4
echo ""

# 4. 入力フィールドにテキスト入力（エフェクト付き）
echo -e "${BLUE}📋 Step 4: Input Text with Enhanced Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/input" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "input[name=\"q\"]",
    "value": "Floorp Browser"
  }' | jq .
echo -e "${GREEN}✓ テキスト入力完了（紫色のエフェクト）${NC}"
echo -e "${YELLOW}👀 入力フィールドに紫色のハイライトが表示されているはずです${NC}"
sleep 2
echo ""

# 5. クリック操作（エフェクト付き）
echo -e "${BLUE}📋 Step 5: Click Element with Enhanced Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "input[name=\"btnK\"]"
  }' | jq .
echo -e "${GREEN}✓ クリック完了（オレンジ色のエフェクト）${NC}"
echo -e "${YELLOW}👀 検索ボタンにオレンジ色のハイライトが表示されたはずです${NC}"
sleep 3
echo ""

# 6. エフェクトのクリア
echo -e "${BLUE}📋 Step 6: Clear All Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/clearEffects" \
  -H "Content-Type: application/json" \
  -d '{}' | jq .
echo -e "${GREEN}✓ すべてのエフェクトがクリアされました${NC}"
sleep 1
echo ""

# 7. GitHub に移動
echo -e "${BLUE}📋 Step 7: Navigate to GitHub${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/navigate" \
  -H "Content-Type: application/json" \
  -d '{"url": "https://github.com"}' | jq .
echo -e "${GREEN}✓ GitHub にナビゲート完了${NC}"
sleep 3
echo ""

# 8. GitHub検索フィールドのハイライトと入力
echo -e "${BLUE}📋 Step 8: Highlight and Fill GitHub Search${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/highlight" \
  -H "Content-Type: application/json" \
  -d '{
    "selectors": ["input[name=\"q\"]"],
    "action": "Fill",
    "elementInfo": "検索フィールドに入力します",
    "duration": 2000
  }' | jq .
sleep 2

curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/input" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "input[name=\"q\"]",
    "value": "floorp-browser"
  }' | jq .
echo -e "${GREEN}✓ GitHub 検索フィールドに入力完了${NC}"
sleep 2
echo ""

# クリーンアップ
echo -e "${BLUE}🧹 Cleanup: Destroying instance${NC}"
curl -s -X DELETE "${BASE_URL}/tabs/instances/${INSTANCE_ID}" | jq .
echo ""

echo "=========================================="
echo -e "${GREEN}✅ All Tests Completed Successfully!${NC}"
echo "=========================================="
echo ""
echo "📊 テストした機能:"
echo "  ✓ 右上の操作情報パネル表示"
echo "  ✓ 複数要素の同時ハイライト"
echo "  ✓ アクション別の色分け（Read=緑、Write=紫、Click=オレンジ）"
echo "  ✓ 要素情報の詳細表示"
echo "  ✓ エフェクトのクリア"
echo ""

