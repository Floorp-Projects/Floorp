#!/bin/bash
# Floorp エンハンスドエフェクト テスト（ローカルHTMLページ使用）

set -e

BASE_URL="http://127.0.0.1:58261"
TEST_PAGE="file://$(pwd)/tools/os-test/test-page.html"

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

# フォーム入力テスト（自動的に紫色のエフェクト + 3秒インターバル）
echo -e "${BLUE}📋 Step 2: Fill Form with Enhanced Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/fillForm" \
  -H "Content-Type: application/json" \
  -d '{
    "formData": {
      "#name": "山田太郎",
      "#email": "yamada@floorp.app",
      "#message": "Floorp のエンハンスドエフェクトは素晴らしいです！"
    }
  }' | jq .
echo -e "${PURPLE}✓ フォーム入力完了（紫色のエフェクト + 各フィールドの進捗表示 + 3秒表示）${NC}"
echo -e "${YELLOW}👀 ブラウザを確認：右上に情報パネルと進捗、各フィールドに紫色のエフェクトが表示されます${NC}"
echo ""

# 送信ボタンをクリック（自動的にオレンジ色のエフェクト + 3秒インターバル）
echo -e "${BLUE}📋 Step 3: Click Submit Button with Enhanced Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#submitBtn"
  }' | jq .
echo -e "${ORANGE}✓ 送信ボタンをクリック（オレンジ色のエフェクト + 情報パネル + 3秒表示）${NC}"
echo -e "${YELLOW}👀 送信ボタンにオレンジ色のハイライトが表示されました${NC}"
echo ""

# リセットボタンをクリック（自動的に3秒インターバル）
echo -e "${BLUE}📋 Step 4: Click Reset Button${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#resetBtn"
  }' | jq .
echo -e "${GREEN}✓ リセットボタンをクリック（オレンジ色のエフェクト + 3秒表示）${NC}"
echo ""

# フォームを再入力してSubmit（赤色のエフェクト + 自動的に3秒インターバル）
echo -e "${BLUE}📋 Step 5: Fill and Submit Form${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/fillForm" \
  -H "Content-Type: application/json" \
  -d '{
    "formData": {
      "#name": "佐藤花子",
      "#email": "sato@floorp.app",
      "#message": "テスト送信"
    }
  }' | jq .

curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/submit" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "#testForm"
  }' | jq .
echo -e "${RED}✓ フォーム送信（赤色のエフェクト + 情報パネル + 3秒表示）${NC}"
echo -e "${YELLOW}👀 フォーム全体に赤色のハイライトが表示されました${NC}"
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
echo -e "  ${GREEN}✓${NC} 右上の操作情報パネル（アクション、要素情報、進捗表示）"
echo -e "  ${GREEN}✓${NC} アクション別の色分け（自動適用）:"
echo -e "      ${PURPLE}■${NC} Fill/Input = 紫色"
echo -e "      ${ORANGE}■${NC} Click = オレンジ色"
echo -e "      ${RED}■${NC} Submit = 赤色"
echo -e "  ${GREEN}✓${NC} 各操作での詳細な情報表示（要素名、進捗など）"
echo -e "  ${GREEN}✓${NC} 既存APIの自動エフェクト化（新規エンドポイント不要）"
echo ""

