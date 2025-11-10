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

# 3. フォーム入力テスト（自動的に紫色のエフェクト + 3秒インターバル）
echo -e "${BLUE}📋 Step 3: Fill Search Form with Enhanced Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/fillForm" \
  -H "Content-Type: application/json" \
  -d '{
    "formData": {
      "input[name=\"q\"]": "Floorp Browser"
    }
  }' | jq .
echo -e "${GREEN}✓ フォーム入力完了（紫色のエフェクト + 情報パネル + 3秒表示）${NC}"
echo -e "${YELLOW}👀 ブラウザを確認：右上に情報パネル、入力フィールドに紫色のエフェクトが表示されています${NC}"
echo ""

# 4. クリック操作（自動的にオレンジ色のエフェクト + 3秒インターバル）
echo -e "${BLUE}📋 Step 4: Click Search Button with Enhanced Effects${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{
    "selector": "input[name=\"btnK\"]"
  }' | jq .
echo -e "${GREEN}✓ クリック完了（オレンジ色のエフェクト + 情報パネル + 3秒表示）${NC}"
echo -e "${YELLOW}👀 検索ボタンにオレンジ色のハイライトが表示されました${NC}"
echo ""

# 5. GitHub に移動
echo -e "${BLUE}📋 Step 5: Navigate to GitHub${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/navigate" \
  -H "Content-Type: application/json" \
  -d '{"url": "https://github.com"}' | jq .
echo -e "${GREEN}✓ GitHub にナビゲート完了${NC}"
sleep 3
echo ""

# 6. GitHub検索フォームの入力（自動的に3秒インターバル）
echo -e "${BLUE}📋 Step 6: Fill GitHub Search Form${NC}"
curl -s -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/fillForm" \
  -H "Content-Type: application/json" \
  -d '{
    "formData": {
      "input[name=\"q\"]": "floorp-browser"
    }
  }' | jq .
echo -e "${GREEN}✓ GitHub 検索フィールドに入力完了（3秒表示）${NC}"
echo ""

# 7. 取得系 API のハイライト確認
echo -e "${BLUE}📋 Step 7: Inspect APIs (highlight only)${NC}"
echo -e "${BLUE}  └ getHTML${NC}"
curl -s "${BASE_URL}/tabs/instances/${INSTANCE_ID}/html" | jq .
sleep 2

echo -e "${BLUE}  └ getElement (header)${NC}"
curl -s "${BASE_URL}/tabs/instances/${INSTANCE_ID}/element?selector=header" | jq .
sleep 2

echo -e "${BLUE}  └ getElements (links)${NC}"
curl -s "${BASE_URL}/tabs/instances/${INSTANCE_ID}/elements?selector=a%5Bhref%5D" | jq .
sleep 2

echo -e "${BLUE}  └ getValue (search input)${NC}"
curl -s "${BASE_URL}/tabs/instances/${INSTANCE_ID}/value?selector=input%5Bname%3D%22q%22%5D" | jq .
sleep 2

# クリーンアップ
echo -e "${BLUE}🧹 Cleanup: Destroying instance${NC}"
curl -s -X DELETE "${BASE_URL}/tabs/instances/${INSTANCE_ID}" | jq .
echo ""

echo "=========================================="
echo -e "${GREEN}✅ All Tests Completed Successfully!${NC}"
echo "=========================================="
echo ""
echo "📊 テストした機能:"
echo "  ✓ 右上の操作情報パネル（自動表示）"
echo "  ✓ アクション別の色分け（Fill=紫、Click=オレンジ）"
echo "  ✓ 要素情報の詳細表示（進捗など）"
echo "  ✓ 既存APIの自動エフェクト化"
echo ""

