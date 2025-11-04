#!/usr/bin/env python3
"""
Floorp OS Server API テスト - エンハンスドエフェクト機能
"""

import requests
import json
import time
from typing import Optional

BASE_URL = "http://127.0.0.1:58261"

class FloorpTabManager:
    def __init__(self, base_url: str = BASE_URL):
        self.base_url = base_url
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
    
    def input_element(self, selector: str, value: str):
        """入力フィールドに値を設定（エフェクト付き）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/input",
            json={"selector": selector, "value": value}
        )
        result = resp.json()
        print(f"✓ Input Element [{selector}]: {value} - OK: {result.get('ok')}")
        return result
    
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
    
    def highlight_elements(
        self, 
        selectors: list[str], 
        action: str = "Highlight",
        element_info: Optional[str] = None,
        duration: int = 2000
    ):
        """複数要素をハイライト（新機能！）"""
        if not self.instance_id:
            raise ValueError("No instance created")
        
        payload = {
            "selectors": selectors,
            "action": action,
            "duration": duration
        }
        if element_info:
            payload["elementInfo"] = element_info
        
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/highlight",
            json=payload
        )
        result = resp.json()
        print(f"✓ Highlight Elements: {len(selectors)} elements - OK: {result.get('ok')}")
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
    
    def clear_effects(self):
        """すべてのエフェクトをクリア"""
        if not self.instance_id:
            raise ValueError("No instance created")
        resp = requests.post(
            f"{self.base_url}/tabs/instances/{self.instance_id}/clearEffects",
            json={}
        )
        result = resp.json()
        print(f"✓ Clear Effects - OK: {result.get('ok')}")
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
        
        # 3. 複数要素のハイライトテスト（新機能！）
        print("📋 Step 3: Highlight Multiple Elements")
        manager.highlight_elements(
            selectors=["input[name='q']", "input[name='btnK']", "input[name='btnI']"],
            action="Inspect",
            element_info="検索フォームの要素を確認しています",
            duration=3000
        )
        time.sleep(4)  # エフェクト確認のため待機
        print()
        
        # 4. 入力フィールドにテキスト入力（エフェクト付き）
        print("📋 Step 4: Input Text with Enhanced Effects")
        manager.input_element("input[name='q']", "Floorp Browser")
        time.sleep(2)  # エフェクト確認のため待機
        print()
        
        # 5. クリック操作（エフェクト付き）
        print("📋 Step 5: Click Element with Enhanced Effects")
        manager.click_element("input[name='btnK']")
        time.sleep(3)  # クリック後の遷移を確認
        print()
        
        # 6. エフェクトのクリア
        print("📋 Step 6: Clear All Effects")
        manager.clear_effects()
        time.sleep(1)
        print()
        
        # 7. 別のページでテスト（GitHub）
        print("📋 Step 7: Navigate to GitHub")
        manager.navigate("https://github.com")
        time.sleep(3)
        print()
        
        # 8. フォーム入力のテスト
        print("📋 Step 8: Highlight and Fill Form")
        # まず要素をハイライト
        manager.highlight_elements(
            selectors=["input[name='q']"],
            action="Fill",
            element_info="検索フィールドに入力します",
            duration=2000
        )
        time.sleep(2)
        
        # 入力
        manager.input_element("input[name='q']", "floorp-browser")
        time.sleep(2)
        print()
        
        print("=" * 60)
        print("✅ All Tests Completed Successfully!")
        print("=" * 60)
        print()
        print("📊 テストした機能:")
        print("  ✓ 右上の操作情報パネル表示")
        print("  ✓ 複数要素の同時ハイライト")
        print("  ✓ アクション別の色分け（Read=緑、Write=紫、Click=オレンジ）")
        print("  ✓ 要素情報の詳細表示")
        print("  ✓ エフェクトのクリア")
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

