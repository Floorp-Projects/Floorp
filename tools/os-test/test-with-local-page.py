#!/usr/bin/env python3
"""
Floorp Enhanced Effects テスト（ローカルHTMLページ使用）
"""

import requests
import json
import time
import os
from pathlib import Path
from typing import Optional

BASE_URL = "http://127.0.0.1:58261"

# ANSI color codes
GREEN = '\033[0;32m'
BLUE = '\033[0;34m'
YELLOW = '\033[1;33m'
PURPLE = '\033[0;35m'
ORANGE = '\033[0;33m'
RED = '\033[0;31m'
NC = '\033[0m'  # No Color


class FloorpTabManager:
    def __init__(self, base_url: str = BASE_URL):
        self.base_url = base_url
        self.instance_id: Optional[str] = None
    
    def create_instance(self, url: str, in_background: bool = False):
        """新しいタブインスタンスを作成"""
        resp = requests.post(
            f"{self.base_url}/tabs/instances",
            json={"url": url, "inBackground": in_background}
        )
        resp.raise_for_status()
        data = resp.json()
        self.instance_id = data.get("instanceId")
        print(json.dumps(data, indent=2, ensure_ascii=False))
        print(f"{GREEN}✓ Instance ID: {self.instance_id}{NC}")
        return self.instance_id
    
    def click_element(self, selector: str):
        """要素をクリック（エフェクト付き）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/click",
            json={"selector": selector}
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return result
    
    def fill_form(self, form_data: dict):
        """フォームを一括入力（エフェクト付き）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/fillForm",
            json={"formData": form_data}
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return result
    
    def submit(self, selector: str):
        """フォームを送信（エフェクト付き）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/submit",
            json={"selector": selector}
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return result
    
    def destroy_instance(self):
        """インスタンスを削除"""
        if not self.instance_id:
            return
        resp = requests.delete(
            f"{self.base_url}/tabs/instances/{self.instance_id}"
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        self.instance_id = None
        return result

    def get_html(self):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/html"
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return result

    def get_element(self, selector: str):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/element",
            params={"selector": selector},
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return result

    def get_elements(self, selector: str):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/elements",
            params={"selector": selector},
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return result

    def get_value(self, selector: str):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/value",
            params={"selector": selector},
        )
        resp.raise_for_status()
        result = resp.json()
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return result

    def get_text(self):
        """Get page content as Markdown"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/text"
        )
        resp.raise_for_status()
        result = resp.json()
        text = result.get("text", "")
        print(f"{GREEN}📄 Markdown Output ({len(text)} chars):{NC}")
        print("-" * 40)
        print(text[:500] + ("..." if len(text) > 500 else ""))
        print("-" * 40)
        return result

    def get_markdown_size_comparison(self):
        """Compare sizes of HTML vs Markdown output"""
        if not self.instance_id:
            raise ValueError("No instance created")

        # Get HTML
        resp_html = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/html"
        )
        resp_html.raise_for_status()
        html_size = len(resp_html.json().get("html", ""))

        # Get Text (Markdown)
        resp_text = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/text"
        )
        resp_text.raise_for_status()
        text_size = len(resp_text.json().get("text", ""))

        ratio = (text_size / html_size * 100) if html_size > 0 else 0
        print(f"{BLUE}📊 Size Comparison:{NC}")
        print(f"  HTML:    {html_size:,} chars")
        print(f"  Markdown: {text_size:,} chars")
        print(f"  {GREEN}Reduction: {100 - ratio:.1f}%{NC}")
        return {"html_size": html_size, "text_size": text_size, "ratio": ratio}


def main():
    print("=" * 42)
    print("🎨 Floorp Enhanced Effects デモ")
    print("=" * 42)
    print()
    
    # テストページのパスを取得
    script_dir = Path(__file__).parent
    test_page_path = script_dir / "test-page.html"
    test_page_url = f"file://{test_page_path.absolute()}"

    print(f"📄 Test Page: {test_page_url}")
    print()
    
    manager = FloorpTabManager()
    
    try:
        # Step 1: タブインスタンスを作成
        print(f"{BLUE}📋 Step 1: Create Tab Instance with Test Page{NC}")
        manager.create_instance(test_page_url, in_background=False)
        print()
        time.sleep(2)
        
        # Step 2: フォーム入力テスト（自動的に紫色のエフェクト + 3秒インターバル）
        print(f"{BLUE}📋 Step 2: Fill Form with Enhanced Effects{NC}")
        manager.fill_form({
            "#name": "山田太郎",
            "#email": "yamada@floorp.app",
            "#message": "Floorp のエンハンスドエフェクトは素晴らしいです！"
        })
        print(f"{PURPLE}✓ フォーム入力完了（紫色のエフェクト + 各フィールドの進捗表示 + 3秒表示）{NC}")
        print(f"{YELLOW}👀 ブラウザを確認：右上に情報パネルと進捗、各フィールドに紫色のエフェクトが表示されます{NC}")
        print()
        
        # Step 3: 送信ボタンをクリック（自動的にオレンジ色のエフェクト + 3秒インターバル）
        print(f"{BLUE}📋 Step 3: Click Submit Button with Enhanced Effects{NC}")
        manager.click_element("#submitBtn")
        print(f"{ORANGE}✓ 送信ボタンをクリック（オレンジ色のエフェクト + 情報パネル + 3秒表示）{NC}")
        print(f"{YELLOW}👀 送信ボタンにオレンジ色のハイライトが表示されました{NC}")
        print()
        
        # Step 4: リセットボタンをクリック（自動的に3秒インターバル）
        print(f"{BLUE}📋 Step 4: Click Reset Button{NC}")
        manager.click_element("#resetBtn")
        print(f"{GREEN}✓ リセットボタンをクリック（オレンジ色のエフェクト + 3秒表示）{NC}")
        print()
        
        # Step 5: フォームを再入力してSubmit（赤色のエフェクト + 自動的に3秒インターバル）
        print(f"{BLUE}📋 Step 5: Fill and Submit Form{NC}")
        manager.fill_form({
            "#name": "佐藤花子",
            "#email": "sato@floorp.app",
            "#message": "テスト送信"
        })
        
        manager.submit("#testForm")
        print(f"{RED}✓ フォーム送信（赤色のエフェクト + 情報パネル + 3秒表示）{NC}")
        print(f"{YELLOW}👀 フォーム全体に赤色のハイライトが表示されました{NC}")
        print()

        # Step 6: 取得系 API（Inspect ハイライト）の確認
        print(f"{BLUE}📋 Step 6: Inspect APIs (highlight only){NC}")
        print(f"{BLUE}  └ getHTML{NC}")
        manager.get_html()
        time.sleep(2.2)

        print(f"{BLUE}  └ getText (Markdown output){NC}")
        manager.get_text()
        time.sleep(2.2)

        print(f"{BLUE}  └ Size Comparison (HTML vs Markdown){NC}")
        manager.get_markdown_size_comparison()
        time.sleep(2.2)

        print(f"{BLUE}  └ getElement (Submit Button){NC}")
        manager.get_element("#submitBtn")
        time.sleep(2.2)

        print(f"{BLUE}  └ getElements (Input fields){NC}")
        manager.get_elements("form input")
        time.sleep(2.2)

        print(f"{BLUE}  └ getValue (Name field){NC}")
        manager.get_value("#name")
        time.sleep(2.2)
        print(f"{GREEN}✓ 取得系 API を呼び出し、Inspect ハイライトを確認{NC}")
        print()
        
        # クリーンアップ
        print(f"{BLUE}🧹 Cleanup: Destroying instance{NC}")
        manager.destroy_instance()
        print()
        
        print("=" * 42)
        print(f"{GREEN}✅ デモ完了！{NC}")
        print("=" * 42)
        print()
        print("📊 確認できた機能:")
        print(f"  {GREEN}✓{NC} 右上の操作情報パネル（アクション、要素情報、進捗表示）")
        print(f"  {GREEN}✓{NC} アクション別の色分け（自動適用）:")
        print(f"      {PURPLE}■{NC} Fill/Input = 紫色")
        print(f"      {ORANGE}■{NC} Click = オレンジ色")
        print(f"      {RED}■{NC} Submit = 赤色")
        print(f"  {GREEN}✓{NC} 各操作での詳細な情報表示（要素名、進捗など）")
        print(f"  {GREEN}✓{NC} 既存APIの自動エフェクト化（新規エンドポイント不要）")
        print()
        
    except requests.exceptions.RequestException as e:
        print(f"{RED}❌ HTTP Error: {e}{NC}")
        if hasattr(e, 'response') and e.response is not None:
            try:
                print(json.dumps(e.response.json(), indent=2, ensure_ascii=False))
            except:
                print(e.response.text)
        import traceback
        traceback.print_exc()
    except Exception as e:
        print(f"{RED}❌ Error: {e}{NC}")
        import traceback
        traceback.print_exc()
    finally:
        # クリーンアップ（エラー時も実行）
        if manager.instance_id:
            print(f"{BLUE}🧹 Cleanup: Destroying instance...{NC}")
            try:
                manager.destroy_instance()
            except:
                pass


if __name__ == "__main__":
    main()

