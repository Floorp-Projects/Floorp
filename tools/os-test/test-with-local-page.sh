#!/bin/bash
# Floorp エンハンスドエフェクト テスト（ローカルHTMLページ使用）

set -euo pipefail

BASE_URL="${FLOORP_OS_BASE_URL:-http://127.0.0.1:58261}"
BASE_URL="${BASE_URL%/}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_PAGE="file://${SCRIPT_DIR}/test-page.html"

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

TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t floorp-os-test)"
INSTANCE_ID=""
FAIL_COUNT=0

assert_http_code() {
  local expected="$1"
  local actual="$2"
  local label="$3"
  if [[ "$actual" == "$expected" ]]; then
    echo -e "${GREEN}✓ ${label} HTTP ${actual}${NC}"
  else
    echo -e "${RED}✗ ${label} expected HTTP ${expected}, got ${actual}${NC}"
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
}

call_json() {
  local method="$1"
  local path="$2"
  local label="$3"
  local payload="${4:-}"
  local out_file
  out_file="$(mktemp "${TMP_DIR}/api-XXXXXX.json")"
  local status

  if [[ -n "${payload}" ]]; then
    status=$(curl -sS -o "${out_file}" -w "%{http_code}" -X "${method}" "${BASE_URL}${path}" \
      -H "Content-Type: application/json" \
      -d "${payload}")
  else
    status=$(curl -sS -o "${out_file}" -w "%{http_code}" -X "${method}" "${BASE_URL}${path}")
  fi

  assert_http_code 200 "${status}" "${label}"
  jq . "${out_file}"
}

cleanup() {
  if [[ -n "${INSTANCE_ID}" && "${INSTANCE_ID}" != "null" ]]; then
    curl -s -X DELETE "${BASE_URL}/tabs/instances/${INSTANCE_ID}" >/dev/null 2>&1 || true
  fi
  rm -rf "${TMP_DIR}" >/dev/null 2>&1 || true
}

trap cleanup EXIT

# タブインスタンスを作成
echo -e "${BLUE}📋 Step 1: Create Tab Instance with Test Page${NC}"
CREATE_BODY_FILE="${TMP_DIR}/create_instance.json"
CREATE_STATUS=$(curl -sS -o "${CREATE_BODY_FILE}" -w "%{http_code}" -X POST "${BASE_URL}/tabs/instances" \
  -H "Content-Type: application/json" \
  -d "{\"url\": \"${TEST_PAGE}\", \"inBackground\": false}")
assert_http_code 200 "${CREATE_STATUS}" "create tab instance"
jq . "${CREATE_BODY_FILE}"
INSTANCE_ID=$(jq -r '.instanceId // empty' "${CREATE_BODY_FILE}")
if [[ -z "${INSTANCE_ID}" ]]; then
  echo -e "${RED}✗ instanceId が取得できませんでした。終了します。${NC}"
  exit 1
fi
echo -e "${GREEN}✓ Instance ID: ${INSTANCE_ID}${NC}"
echo ""
sleep 2

# フォーム入力テスト（自動的に紫色のエフェクト + 3秒インターバル）
echo -e "${BLUE}📋 Step 2: Fill Form with Enhanced Effects${NC}"
call_json "POST" "/tabs/instances/${INSTANCE_ID}/fillForm" "fill form (step 2)" '{
    "formData": {
      "#name": "山田太郎",
      "#email": "yamada@floorp.app",
      "#message": "Floorp のエンハンスドエフェクトは素晴らしいです！"
    }
  }'
echo -e "${PURPLE}✓ フォーム入力完了（紫色のエフェクト + 各フィールドの進捗表示 + 3秒表示）${NC}"
echo -e "${YELLOW}👀 ブラウザを確認：右上に情報パネルと進捗、各フィールドに紫色のエフェクトが表示されます${NC}"
echo ""

# 送信ボタンをクリック（自動的にオレンジ色のエフェクト + 3秒インターバル）
echo -e "${BLUE}📋 Step 3: Click Submit Button with Enhanced Effects${NC}"
call_json "POST" "/tabs/instances/${INSTANCE_ID}/click" "click submit button (step 3)" '{
    "selector": "#submitBtn"
  }'
echo -e "${ORANGE}✓ 送信ボタンをクリック（オレンジ色のエフェクト + 情報パネル + 3秒表示）${NC}"
echo -e "${YELLOW}👀 送信ボタンにオレンジ色のハイライトが表示されました${NC}"
echo ""

# リセットボタンをクリック（自動的に3秒インターバル）
echo -e "${BLUE}📋 Step 4: Click Reset Button${NC}"
call_json "POST" "/tabs/instances/${INSTANCE_ID}/click" "click reset button (step 4)" '{
    "selector": "#resetBtn"
  }'
echo -e "${GREEN}✓ リセットボタンをクリック（オレンジ色のエフェクト + 3秒表示）${NC}"
echo ""

# フォームを再入力してSubmit（赤色のエフェクト + 自動的に3秒インターバル）
echo -e "${BLUE}📋 Step 5: Fill and Submit Form${NC}"
call_json "POST" "/tabs/instances/${INSTANCE_ID}/fillForm" "fill form (step 5)" '{
    "formData": {
      "#name": "佐藤花子",
      "#email": "sato@floorp.app",
      "#message": "テスト送信"
    }
  }'

call_json "POST" "/tabs/instances/${INSTANCE_ID}/submit" "submit form (step 5)" '{
    "selector": "#testForm"
  }'
echo -e "${RED}✓ フォーム送信（赤色のエフェクト + 情報パネル + 3秒表示）${NC}"
echo -e "${YELLOW}👀 フォーム全体に赤色のハイライトが表示されました${NC}"
echo ""

# 取得系 API で Inspect ハイライトを確認
echo -e "${BLUE}📋 Step 6: Inspect APIs (highlight only)${NC}"
echo -e "${BLUE}  └ getHTML${NC}"
call_json "GET" "/tabs/instances/${INSTANCE_ID}/html" "get html (step 6)"
sleep 2

echo -e "${BLUE}  └ getElement (#submitBtn)${NC}"
call_json "GET" "/tabs/instances/${INSTANCE_ID}/element?selector=%23submitBtn" "get element #submitBtn (step 6)"
sleep 2

echo -e "${BLUE}  └ getElements (form input)${NC}"
call_json "GET" "/tabs/instances/${INSTANCE_ID}/elements?selector=form%20input" "get elements form input (step 6)"
sleep 2

echo -e "${BLUE}  └ getValue (#name)${NC}"
call_json "GET" "/tabs/instances/${INSTANCE_ID}/value?selector=%23name" "get value #name (step 6)"
sleep 2
echo -e "${GREEN}✓ Inspect ハイライトの挙動を確認${NC}"
echo ""

# waitForElement 契約チェック
echo -e "${BLUE}📋 Step 7: Wait contract checks${NC}"
echo -e "${BLUE}  └ waitForElement (#title)${NC}"
call_json "POST" "/tabs/instances/${INSTANCE_ID}/waitForElement" "waitForElement #title (step 7)" '{"selector":"#title","timeout":2500}'

echo -e "${BLUE}  └ waitForElement (non-existent selector -> ok=false/found=false)${NC}"
call_json "POST" "/tabs/instances/${INSTANCE_ID}/waitForElement" "waitForElement non-existent selector (step 7)" '{"selector":"#does-not-exist","timeout":100}'
echo ""

# fingerprint/selector 負系チェック
echo -e "${BLUE}📋 Step 8: Negative validation checks${NC}"
echo -e "${BLUE}  └ click with invalid fingerprint (expect 400)${NC}"
NEG_INVALID_FP_JSON="${TMP_DIR}/click_invalid_fp.json"
NEG_INVALID_FP_STATUS=$(curl -sS -o "${NEG_INVALID_FP_JSON}" -w "%{http_code}" \
  -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{"fingerprint":"badformat"}'
)
echo "HTTP ${NEG_INVALID_FP_STATUS}"
assert_http_code 400 "${NEG_INVALID_FP_STATUS}" "click invalid fingerprint"
jq . "${NEG_INVALID_FP_JSON}"

echo -e "${BLUE}  └ click with missing selector/fingerprint (expect 400)${NC}"
NEG_MISSING_PAYLOAD_JSON="${TMP_DIR}/click_missing_payload.json"
NEG_MISSING_PAYLOAD_STATUS=$(curl -sS -o "${NEG_MISSING_PAYLOAD_JSON}" -w "%{http_code}" \
  -X POST "${BASE_URL}/tabs/instances/${INSTANCE_ID}/click" \
  -H "Content-Type: application/json" \
  -d '{}'
)
echo "HTTP ${NEG_MISSING_PAYLOAD_STATUS}"
assert_http_code 400 "${NEG_MISSING_PAYLOAD_STATUS}" "click missing selector/fingerprint"
jq . "${NEG_MISSING_PAYLOAD_JSON}"
echo ""

# クリーンアップ
echo -e "${BLUE}🧹 Cleanup: Destroying instance${NC}"
call_json "DELETE" "/tabs/instances/${INSTANCE_ID}" "destroy tab instance"
INSTANCE_ID=""
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

if (( FAIL_COUNT > 0 )); then
  echo -e "${RED}❌ ${FAIL_COUNT} 件のHTTP検証失敗がありました${NC}"
  exit 1
fi
