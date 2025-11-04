#!/bin/bash
# Floorp エンハンスドエフェクト テスト（ローカルHTMLページ使用）

set -e

BASE_URL="http://127.0.0.1:58261"
TEST_PAGE="file://$(pwd)/test-page.html"

echo "=========================================="
echo "🎨 Floorp Enhanced Effects デモ"
echo "=========================================="
echo ""
echo "📄 Test Page: ${TEST_PAGE}"
echo ""

# 色コード
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
ORANGE='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# タブインスタンスを作成
echo -e "${BLUE}📋 Step 1: Create Tab Instance with Test Page${NC}"
RESPONSE=$(curl -s -X POST "${BASE_URL}/tabs/instances" \
  -H "Content-Type: application/json" \
  -d "{\"url\": \"${TEST_PAGE}\", \"inBackground\": false}")
echo "$RESPONSE" | jq .
INSTANCE_ID=$(echo "$RESPONSE" | jq -r '.instanceId')
echo -e "${GREEN}✓ Instance ID: ${INSTANCE_ID}${NC}"
echo ""
sleep 2

# 複数要素のハイライト（緑色 - Read/Inspect）
echo -e "${BLUE}📋 Step 2: Highlight Multiple Boxes (Green - Inspect)${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/highlight" \
  -H "Content-Type: application/json" \
  -d '{
    "selectors": ["#box1", "#box2", "#box3"],
    "action": "Inspect",
    "elementInfo": "3つのボックスを検査しています",
    "duration": 3000
  }' | jq .
echo -e "${GREEN}✓ 3つのボックスがハイライトされました（緑色）${NC}"
echo -e "${YELLOW}👀 ブラウザを確認：右上に情報パネルが表示され、3つのボックスが緑色でハイライトされています${NC}"
sleep 4
echo ""

# フォームフィールドのハイライト（紫色 - Fill）
echo -e "${BLUE}📋 Step 3: Highlight Form Fields (Purple - Fill)${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/highlight" \
  -H "Content-Type: application/json" \
  -d '{
    "selectors": ["#name", "#email", "#message"],
    "action": "Fill",
    "elementInfo": "フォームフィールドに入力を準備しています",
    "duration": 2500
  }' | jq .
echo -e "${PURPLE}✓ フォームフィールドがハイライトされました（紫色）${NC}"
sleep 3
echo ""

# 名前フィールドに入力（紫色 - Input）
echo -e "${BLUE}📋 Step 4: Input Name Field (Purple - Input)${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/input" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#name",
    "value": "山田太郎"
  }' | jq .
echo -e "${PURPLE}✓ 名前フィールドに入力（紫色のエフェクト）${NC}"
sleep 2
echo ""

# メールフィールドに入力
echo -e "${BLUE}📋 Step 5: Input Email Field (Purple - Input)${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/input" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#email",
    "value": "yamada@floorp.app"
  }' | jq .
echo -e "${PURPLE}✓ メールフィールドに入力${NC}"
sleep 2
echo ""

# メッセージフィールドに入力
echo -e "${BLUE}📋 Step 6: Input Message Field (Purple - Input)${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/input" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#message",
    "value": "Floorp のエンハンスドエフェクトは素晴らしいです！"
  }' | jq .
echo -e "${PURPLE}✓ メッセージフィールドに入力${NC}"
sleep 2
echo ""

# ボタンをハイライト（オレンジ色 - Click）
echo -e "${BLUE}📋 Step 7: Highlight Submit Button (Orange)${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/highlight" \
  -H "Content-Type: application/json" \
  -d '{
    "selectors": ["#submitBtn"],
    "action": "Click",
    "elementInfo": "送信ボタンをクリックします",
    "duration": 2000
  }' | jq .
echo -e "${ORANGE}✓ 送信ボタンがハイライトされました（オレンジ色）${NC}"
sleep 2
echo ""

# 送信ボタンをクリック
echo -e "${BLUE}📋 Step 8: Click Submit Button (Orange - Click)${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#submitBtn"
  }' | jq .
echo -e "${ORANGE}✓ 送信ボタンをクリック（オレンジ色のエフェクト）${NC}"
sleep 2
echo ""

# リセットボタンをハイライトしてクリック
echo -e "${BLUE}📋 Step 9: Highlight and Click Reset Button${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/highlight" \
  -H "Content-Type: application/json" \
  -d '{
    "selectors": ["#resetBtn"],
    "action": "Click",
    "elementInfo": "リセットボタンをクリックしてフォームをクリアします",
    "duration": 2000
  }' | jq .
sleep 2

curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#resetBtn"
  }' | jq .
echo -e "${GREEN}✓ リセットボタンをクリック${NC}"
sleep 2
echo ""

# すべてのエフェクトをクリア
echo -e "${BLUE}📋 Step 10: Clear All Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/clearEffects" \
  -H "Content-Type: application/json" \
  -d '{}' | jq .
echo -e "${GREEN}✓ すべてのエフェクトがクリアされました${NC}"
sleep 1
echo ""

# クリーンアップ
echo -e "${BLUE}🧹 Cleanup: Destroying instance${NC}"
curl -s -X DELETE "${BASE_URL}/tabs/instances/${INSTANCE_ID}" | jq .
echo ""

echo "=========================================="
echo -e "${GREEN}✅ デモ完了！${NC}"
echo "=========================================="
echo ""
echo "📊 確認できた機能:"
echo -e "  ${GREEN}✓${NC} 右上の操作情報パネル（アクション、要素情報、セレクタ、要素数）"
echo -e "  ${GREEN}✓${NC} 複数要素の同時ハイライト"
echo -e "  ${GREEN}✓${NC} アクション別の色分け:"
echo -e "      ${GREEN}■${NC} Inspect/Read = 緑色"
echo -e "      ${PURPLE}■${NC} Input/Fill/Write = 紫色"
echo -e "      ${ORANGE}■${NC} Click = オレンジ色"
echo -e "  ${GREEN}✓${NC} 各操作での詳細な情報表示"
echo -e "  ${GREEN}✓${NC} エフェクトのクリア"
echo ""

