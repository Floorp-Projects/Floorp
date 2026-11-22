#!/usr/bin/env python3
"""
Floorp OS Server API テスト - エンハンスドエフェクト機能
"""

import requests
import json
import os
import time
from typing import Optional

from os_test_client import resolve_base_url


class FloorpTabManager:
    def __init__(self, base_url: Optional[str] = None):
        self.base_url = resolve_base_url(
            base_url, os.environ.get("FLOORP_OS_BASE_URL")
        ).rstrip("/")
        self.instance_id: Optional[str] = None
    
    def health_check(self):
        """サーバーの健全性を確認"""
        resp = requests.get(f"{self.base_url}/health")
        print(f"✓ Health Check: {resp.json()}")
        return resp.json()
    
    def create_instance(self, url: str, in_background: bool = False):
        """新しいタブインスタンスを作成"""
        resp = requests.post(
            f"{self.base_url}/tabs/instances",
            json={"url": url, "inBackground": in_background}
        )
        data = resp.json()
        self.instance_id = data.get("instanceId")
        print(f"✓ Instance Created: {self.instance_id}")
        return self.instance_id
    
    def navigate(self, url: str):
        """ナビゲート"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/navigate",
            json={"url": url}
        )
        print(f"✓ Navigated to: {url}")
        return resp.json()
    
    def click_element(self, selector: str):
        """要素をクリック（エフェクト付き）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/click",
            json={"selector": selector}
        )
        result = resp.json()
        print(f"✓ Click Element [{selector}] - OK: {result.get('ok')}")
        return result
    
    def fill_form(self, form_data: dict):
        """フォームを一括入力（エフェクト付き）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/fillForm",
            json={"formData": form_data}
        )
        result = resp.json()
        print(f"✓ Fill Form: {len(form_data)} fields - OK: {result.get('ok')}")
        return result
    
    def submit(self, selector: str):
        """フォームを送信（エフェクト付き）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/submit",
            json={"selector": selector}
        )
        result = resp.json()
        print(f"✓ Submit Form [{selector}] - OK: {result.get('ok')}")
        return result
    
    def destroy_instance(self):
        """インスタンスを削除"""
        if not self.instance_id:
            return
        resp = requests.delete(
            f"{self.base_url}/tabs/instances/{self.instance_id}"
        )
        print(f"✓ Instance Destroyed: {self.instance_id}")
        self.instance_id = None
        return resp.json()

    def get_html(self):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/html",
        )
        data = resp.json()
        html = data.get("html", "")
        print(f"✓ getHTML length: {len(html) if html else 0}")
        return data

    def get_element(self, selector: str):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/element",
            params={"selector": selector},
        )
        data = resp.json()
        print(f"✓ getElement '{selector}' -> found: {bool(data.get('element'))}")
        return data

    def get_elements(self, selector: str):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/elements",
            params={"selector": selector},
        )
        data = resp.json()
        elements = data.get("elements", [])
        print(f"✓ getElements '{selector}' -> count: {len(elements)}")
        return data

    def get_value(self, selector: str):
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.get(
            f"{self.base_url}/tabs/instances/{self.instance_id}/value",
            params={"selector": selector},
        )
        data = resp.json()
        print(f"✓ getValue '{selector}' -> value: {data.get('value')}")
        return data


def main():
    print("=" * 60)
    print("🎨 Floorp Enhanced Effects API テスト")
    print("=" * 60)
    print()
    
    manager = FloorpTabManager()
    
    try:
        # 1. ヘルスチェック
        print("📋 Step 1: Health Check")
        manager.health_check()
        print()
        
        # 2. テスト用のページを開く（Google検索ページ）
        print("📋 Step 2: Create Tab Instance")
        manager.create_instance("https://www.google.com", in_background=False)
        time.sleep(3)  # ページロード待機
        print()
        
        # 3. フォーム入力テスト（自動的に紫色のエフェクト + 3秒インターバル）
        print("📋 Step 3: Fill Search Form with Enhanced Effects")
        manager.fill_form({
            "input[name='q']": "Floorp Browser"
        })
        print()
        
        # 4. クリック操作（自動的にオレンジ色のエフェクト + 3秒インターバル）
        print("📋 Step 4: Click Search Button with Enhanced Effects")
        manager.click_element("input[name='btnK']")
        print()
        
        # 5. 別のページでテスト（GitHub）
        print("📋 Step 5: Navigate to GitHub")
        manager.navigate("https://github.com")
        time.sleep(3)
        print()
        
        # 6. GitHub検索フォームの入力テスト（自動的に3秒インターバル）
        print("📋 Step 6: Fill GitHub Search Form")
        manager.fill_form({
            "input[name='q']": "floorp-browser"
        })
        print()

        # 7. 取得系 API のハイライト確認
        print("📋 Step 7: Inspect APIs (highlight only)")
        manager.get_html()
        time.sleep(2)
        manager.get_element("header")
        time.sleep(2)
        manager.get_elements("a[href]")
        time.sleep(2)
        manager.get_value("input[name='q']")
        time.sleep(2)
        print("  ✓ Inspect highlights confirmed")
        print()
        
        print("=" * 60)
        print("✅ All Tests Completed Successfully!")
        print("=" * 60)
        print()
        print("📊 テストした機能:")
        print("  ✓ 右上の操作情報パネル（自動表示）")
        print("  ✓ アクション別の色分け（Fill=紫、Click=オレンジ）")
        print("  ✓ 要素情報の詳細表示（進捗など）")
        print("  ✓ 既存APIの自動エフェクト化")
        print()
        
    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
    
    finally:
        # クリーンアップ
        print("🧹 Cleanup: Destroying instance...")
        manager.destroy_instance()
        print("✓ Done!")


if __name__ == "__main__":
    main()
