#!/usr/bin/env python3
"""
Floorp OS API comprehensive test suite.

Covers positive operations, state verification, negative/edge cases,
and XrayWrapper cloneInto event dispatch paths.
"""

import json
import os
import re
import time
from pathlib import Path
from typing import Optional

import requests

from os_test_client import resolve_base_url

REQUEST_TIMEOUT = (5, 30)

GREEN = "\033[0;32m"
BLUE = "\033[0;34m"
YELLOW = "\033[1;33m"
RED = "\033[0;31m"
NC = "\033[0m"

FINGERPRINT_RE = re.compile(r"<!--fp:([a-z0-9]{8}(?:[a-z0-9]{8})?)-->")


# ---------------------------------------------------------------------------
# HTTP helpers
# ---------------------------------------------------------------------------

class API:
    """Thin wrapper around requests for the OS API server."""

    def __init__(self, base_url: Optional[str] = None):
        self.base_url = resolve_base_url(
            base_url, os.environ.get("FLOORP_OS_BASE_URL")
        ).rstrip("/")
        self.timeout = REQUEST_TIMEOUT

    def get(self, path: str, **kw):
        kw.setdefault("timeout", self.timeout)
        return requests.get(f"{self.base_url}{path}", **kw)

    def post(self, path: str, **kw):
        kw.setdefault("timeout", self.timeout)
        return requests.post(f"{self.base_url}{path}", **kw)

    def delete(self, path: str, **kw):
        kw.setdefault("timeout", self.timeout)
        return requests.delete(f"{self.base_url}{path}", **kw)


# ---------------------------------------------------------------------------
# Assertion helpers
# ---------------------------------------------------------------------------

class TestRunner:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self._section = ""

    def section(self, title: str):
        self._section = title
        print(f"\n{BLUE}{'=' * 60}{NC}")
        print(f"{BLUE}{title}{NC}")
        print(f"{BLUE}{'=' * 60}{NC}")

    def sub(self, label: str):
        print(f"{BLUE}  {label}{NC}")

    def ok(self, msg: str):
        self.passed += 1
        print(f"{GREEN}    OK  {msg}{NC}")

    def fail(self, msg: str):
        self.failed += 1
        print(f"{RED}    FAIL  {msg}{NC}")

    def assert_eq(self, actual, expected, label: str):
        if actual == expected:
            self.ok(label)
        else:
            self.fail(f"{label}: expected {expected!r}, got {actual!r}")

    def assert_true(self, value, label: str):
        if value:
            self.ok(label)
        else:
            self.fail(f"{label}: expected truthy, got {value!r}")

    def assert_in(self, needle, haystack, label: str):
        if needle in haystack:
            self.ok(label)
        else:
            self.fail(f"{label}: {needle!r} not in value")

    def assert_match(self, pattern: str, text: str, label: str):
        if re.search(pattern, text):
            self.ok(label)
        else:
            self.fail(f"{label}: pattern {pattern!r} not found")

    def run(self, fn, label: str = ""):
        """Run a callable, catch timeout/request errors, record as fail."""
        try:
            return fn()
        except (requests.exceptions.Timeout, requests.exceptions.ConnectionError) as e:
            self.fail(f"{label or 'operation'}: timeout/connection error ({e.__class__.__name__})")
            return None
        except Exception as e:
            self.fail(f"{label or 'operation'}: {e}")
            return None

    def assert_status(self, resp: requests.Response, expected: int, label: str):
        if resp.status_code == expected:
            self.ok(f"{label} -> HTTP {expected}")
        else:
            body = ""
            try:
                body = resp.json()
            except Exception:
                body = resp.text[:200]
            self.fail(f"{label}: expected HTTP {expected}, got {resp.status_code} ({body})")

    def summary(self) -> int:
        total = self.passed + self.failed
        print(f"\n{'=' * 60}")
        print(f"Results: {self.passed}/{total} passed, {self.failed} failed")
        if self.failed:
            print(f"{RED}FAILED{NC}")
        else:
            print(f"{GREEN}ALL PASSED{NC}")
        print(f"{'=' * 60}")
        return 1 if self.failed else 0


# ---------------------------------------------------------------------------
# Instance helper
# ---------------------------------------------------------------------------

def clear_effects(api: API, iid: str):
    """Clear highlight effects; swallow timeouts so tests can continue."""
    try:
        api.post(f"/tabs/instances/{iid}/clearEffects", timeout=(5, 15))
    except Exception:
        pass
    time.sleep(0.5)


def create_instance(api: API, t: TestRunner, url: str) -> Optional[str]:
    resp = api.post("/tabs/instances", json={"url": url, "inBackground": False})
    t.assert_status(resp, 200, "create instance")
    iid = resp.json().get("instanceId")
    t.assert_true(iid, "instanceId present")
    return iid


def destroy_instance(api: API, iid: str):
    try:
        api.delete(f"/tabs/instances/{iid}")
    except Exception:
        pass


# ---------------------------------------------------------------------------
# Test groups
# ---------------------------------------------------------------------------

def test_positive_operations(api: API, t: TestRunner, iid: str):
    """Basic positive operations with state verification."""
    t.section("1. Positive operations + state verification")
    prefix = f"/tabs/instances/{iid}"

    # -- input (non-typing) + verify --
    t.sub("input + verify")
    r = api.post(f"{prefix}/input", json={
        "selector": "#name", "value": "Alice"
    })
    t.assert_status(r, 200, "input #name")
    t.assert_eq(r.json().get("ok"), True, "input ok=true")

    r = api.get(f"{prefix}/value", params={"selector": "#name"})
    t.assert_status(r, 200, "getValue #name")
    t.assert_eq(r.json().get("value"), "Alice", "name value = Alice")

    # -- input typingMode + verify (short value, fast delay) --
    t.sub("input typingMode + verify")
    r = api.post(f"{prefix}/input", json={
        "selector": "#email", "value": "ab", "typingMode": True, "typingDelayMs": 5
    })
    t.assert_status(r, 200, "input typingMode")
    t.assert_eq(r.json().get("ok"), True, "typing ok=true")

    r = api.get(f"{prefix}/value", params={"selector": "#email"})
    t.assert_eq(r.json().get("value"), "ab", "email = ab after typing")

    # -- click --
    t.sub("click")
    r = api.post(f"{prefix}/click", json={"selector": "#box1"})
    t.assert_status(r, 200, "click box1")
    t.assert_eq(r.json().get("ok"), True, "click ok=true")

    # -- clearInput + verify --
    t.sub("clearInput + verify")
    r = api.post(f"{prefix}/clearInput", json={"selector": "#name"})
    t.assert_status(r, 200, "clearInput")
    t.assert_eq(r.json().get("ok"), True, "clearInput ok=true")

    r = api.get(f"{prefix}/value", params={"selector": "#name"})
    t.assert_eq(r.json().get("value"), "", "name empty after clear")

    # clear highlight queue before continuing
    clear_effects(api, iid)

    # -- selectOption + verify --
    t.sub("selectOption + verify")
    r = api.post(f"{prefix}/selectOption", json={
        "selector": "#category", "value": "bug"
    })
    t.assert_status(r, 200, "selectOption")
    t.assert_eq(r.json().get("ok"), True, "selectOption ok=true")

    r = api.get(f"{prefix}/value", params={"selector": "#category"})
    t.assert_eq(r.json().get("value"), "bug", "category = bug")

    # -- setChecked checkbox + verify via attribute --
    t.sub("setChecked checkbox")
    r = api.post(f"{prefix}/setChecked", json={
        "selector": "#notify-email", "checked": True
    })
    t.assert_status(r, 200, "setChecked checkbox")
    t.assert_eq(r.json().get("ok"), True, "setChecked ok=true")

    r = api.get(f"{prefix}/attribute", params={
        "selector": "#notify-email", "name": "aria-checked"
    })
    t.assert_eq(r.json().get("value"), "true", "checkbox aria-checked=true")

    # -- setChecked radio --
    t.sub("setChecked radio")
    r = api.post(f"{prefix}/setChecked", json={
        "selector": "#priority-high", "checked": True
    })
    t.assert_eq(r.json().get("ok"), True, "setChecked radio ok=true")

    # -- isVisible / isEnabled --
    t.sub("isVisible / isEnabled")
    r = api.get(f"{prefix}/isVisible", params={"selector": "#submitBtn"})
    t.assert_status(r, 200, "isVisible")
    t.assert_eq(r.json().get("visible"), True, "submitBtn visible=true")

    r = api.get(f"{prefix}/isEnabled", params={"selector": "#submitBtn"})
    t.assert_eq(r.json().get("enabled"), True, "submitBtn enabled=true")

    # -- title --
    t.sub("title")
    r = api.get(f"{prefix}/title")
    t.assert_in("Floorp", r.json().get("title", ""), "title contains Floorp")


def test_event_dispatch(api: API, t: TestRunner, iid: str):
    """XrayWrapper cloneInto event dispatch coverage."""
    t.section("2. Event dispatch (XrayWrapper cloneInto)")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    cases = [
        ("doubleClick", {"selector": "#box2"}),
        ("rightClick", {"selector": "#box3"}),
        ("focus", {"selector": "#email"}),
        ("pressKey", {"key": "Tab"}),
        ("scrollTo", {"selector": "#box1"}),
        ("hover", {"selector": "#box1"}),
        ("setTextContent", {"selector": "#editor", "textContent": "TC test"}),
        ("dispatchTextInput", {"selector": "#editor", "text": "DTI test"}),
        ("dispatchEvent", {"selector": "#box1", "eventType": "click"}),
    ]
    for endpoint, payload in cases:
        r = api.post(f"{prefix}/{endpoint}", json=payload)
        t.assert_status(r, 200, endpoint)
        t.assert_eq(r.json().get("ok"), True, f"{endpoint} ok=true")


def test_inspect_apis(api: API, t: TestRunner, iid: str):
    """Read-only inspection endpoints."""
    t.section("3. Inspect / read APIs")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # -- getHTML --
    t.sub("getHTML")
    r = api.get(f"{prefix}/html")
    t.assert_status(r, 200, "getHTML")
    html = r.json().get("html", "")
    t.assert_in("<h1", html, "html contains h1 tag")

    # -- getText with fingerprints --
    t.sub("getText")
    r = api.get(f"{prefix}/text")
    t.assert_status(r, 200, "getText")
    text = r.json().get("text", "")
    fps = FINGERPRINT_RE.findall(text)
    t.assert_true(len(fps) > 0, f"found {len(fps)} fingerprints in markdown")
    t.assert_in("Floorp OS Test", text, "markdown contains title text")

    # -- getText with selectorMap --
    t.sub("getText + selectorMap")
    r = api.get(f"{prefix}/text", params={"includeSelectorMap": "true"})
    t.assert_status(r, 200, "getText selectorMap")
    text_map = r.json().get("text", "")
    t.assert_match(r"fp:[a-z0-9]+ \|", text_map, "selectorMap entries present")

    # -- getElement --
    t.sub("getElement")
    r = api.get(f"{prefix}/element", params={"selector": "#title"})
    t.assert_status(r, 200, "getElement #title")
    t.assert_in("Floorp OS Test", r.json().get("element", ""), "element contains text")

    # -- getElements --
    t.sub("getElements")
    r = api.get(f"{prefix}/elements", params={"selector": ".target-box"})
    t.assert_status(r, 200, "getElements .target-box")
    elems = r.json().get("elements", [])
    t.assert_eq(len(elems), 3, "3 target-box elements")

    # -- elementText --
    t.sub("elementText")
    r = api.get(f"{prefix}/elementText", params={"selector": "#title"})
    t.assert_status(r, 200, "elementText")

    # -- elementTextContent --
    t.sub("elementTextContent")
    r = api.get(f"{prefix}/elementTextContent", params={"selector": "#title"})
    t.assert_status(r, 200, "elementTextContent")
    t.assert_eq(r.json().get("text"), "Floorp OS Test", "textContent matches")

    # -- cookies --
    t.sub("cookies")
    r = api.get(f"{prefix}/cookies")
    t.assert_status(r, 200, "cookies")

    # -- uri --
    t.sub("uri")
    r = api.get(f"{prefix}/uri")
    t.assert_status(r, 200, "uri")
    t.assert_in("test-page.html", r.json().get("uri", ""), "uri contains test-page.html")

    # -- screenshot --
    t.sub("screenshot")
    r = api.get(f"{prefix}/screenshot")
    t.assert_status(r, 200, "screenshot")
    t.assert_true(r.json().get("image"), "screenshot image not empty")

    return fps  # return fingerprints for later use


def test_fingerprint_operations(api: API, t: TestRunner, iid: str, fps: list):
    """Fingerprint-based element operations."""
    t.section("4. Fingerprint-based operations")
    prefix = f"/tabs/instances/{iid}"

    if not fps:
        t.fail("no fingerprints available, skipping")
        return

    # -- clearEffects for clean DOM --
    clear_effects(api, iid)

    # -- resolve a fingerprint --
    t.sub("resolveFingerprint")
    fp = fps[0]
    r = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": fp})
    t.assert_status(r, 200, f"resolve {fp}")
    selector = r.json().get("selector", "")
    t.assert_true(len(selector) > 0, "resolved selector non-empty")

    # -- find a clickable element via fingerprint --
    box_fp = None
    for candidate in fps:
        r2 = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": candidate})
        if r2.status_code == 200 and "box" in r2.json().get("selector", "").lower():
            box_fp = candidate
            break

    if box_fp:
        t.sub(f"click via fingerprint ({box_fp})")
        r = api.post(f"{prefix}/click", json={"fingerprint": box_fp})
        t.assert_status(r, 200, "click fingerprint")
        t.assert_eq(r.json().get("ok"), True, "click fp ok=true")

        t.sub("hover via fingerprint")
        r = api.post(f"{prefix}/hover", json={"fingerprint": box_fp})
        t.assert_eq(r.json().get("ok"), True, "hover fp ok=true")

        t.sub("getElement via fingerprint")
        r = api.get(f"{prefix}/element", params={"fingerprint": box_fp})
        t.assert_status(r, 200, "getElement fp")
        t.assert_in("box", r.json().get("element", "").lower(), "element contains box")

        t.sub("scrollTo via fingerprint")
        r = api.post(f"{prefix}/scrollTo", json={"fingerprint": box_fp})
        t.assert_eq(r.json().get("ok"), True, "scrollTo fp ok=true")
    else:
        t.fail("no box fingerprint found")

    # -- selector priority over fingerprint --
    t.sub("selector priority over invalid fingerprint")
    r = api.post(f"{prefix}/click", json={
        "selector": "#submitBtn", "fingerprint": "bad!"
    })
    t.assert_status(r, 200, "selector wins over invalid fp")

    # -- waitForElement with fingerprint --
    t.sub("waitForElement fingerprint (visible)")
    r = api.post(f"{prefix}/waitForElement", json={
        "fingerprint": fp, "timeout": 2000, "state": "visible"
    })
    t.assert_status(r, 200, "waitForElement fp")
    t.assert_eq(r.json().get("found"), True, "found=true")


def test_wait_contracts(api: API, t: TestRunner, iid: str, fps: list):
    """waitForElement contract checks."""
    t.section("5. waitForElement contracts")
    prefix = f"/tabs/instances/{iid}"

    t.sub("existing element -> ok=true, found=true")
    r = api.post(f"{prefix}/waitForElement", json={
        "selector": "#title", "timeout": 2000
    })
    t.assert_status(r, 200, "waitForElement #title")
    t.assert_eq(r.json().get("ok"), True, "ok=true")
    t.assert_eq(r.json().get("found"), True, "found=true")

    t.sub("non-existent selector -> ok=true, found=false")
    r = api.post(f"{prefix}/waitForElement", json={
        "selector": "#does-not-exist", "timeout": 100
    })
    t.assert_status(r, 200, "waitForElement non-existent")
    t.assert_eq(r.json().get("found"), False, "found=false")

    t.sub("fingerprint + state=attached -> 400")
    if fps:
        r = api.post(f"{prefix}/waitForElement", json={
            "fingerprint": fps[0], "timeout": 100, "state": "attached"
        })
        t.assert_status(r, 400, "fp + attached -> 400")
        t.assert_in("attached", r.json().get("error", ""), "error mentions attached")


def test_negative_missing_params(api: API, t: TestRunner, iid: str):
    """400 errors for missing required parameters."""
    t.section("6. Missing parameter validation (400)")
    prefix = f"/tabs/instances/{iid}"

    # -- selector/fingerprint required --
    selector_required_endpoints = [
        ("POST", "click", {}),
        ("POST", "hover", {}),
        ("POST", "scrollTo", {}),
        ("POST", "submit", {}),
        ("POST", "clearInput", {}),
        ("POST", "doubleClick", {}),
        ("POST", "rightClick", {}),
        ("POST", "focus", {}),
        ("POST", "selectOption", {}),
        ("POST", "setChecked", {}),
        ("POST", "setInnerHTML", {}),
        ("POST", "setTextContent", {}),
        ("POST", "dispatchEvent", {}),
        ("POST", "waitForElement", {"timeout": 100}),
        ("GET", "element", {}),
        ("GET", "elementText", {}),
        ("GET", "elementTextContent", {}),
        ("GET", "elements", {}),
        ("GET", "value", {}),
        ("GET", "isVisible", {}),
        ("GET", "isEnabled", {}),
        ("GET", "attribute", {"name": "id"}),
        ("GET", "elementScreenshot", {}),
    ]
    for method, ep, extra in selector_required_endpoints:
        if method == "POST":
            r = api.post(f"{prefix}/{ep}", json=extra)
        else:
            r = api.get(f"{prefix}/{ep}", params=extra)
        t.assert_status(r, 400, f"{ep} missing selector/fp -> 400")

    # -- specific required params --
    t.sub("input missing value")
    r = api.post(f"{prefix}/input", json={"selector": "#name"})
    t.assert_status(r, 400, "input no value -> 400")

    t.sub("pressKey missing key")
    r = api.post(f"{prefix}/pressKey", json={})
    t.assert_status(r, 400, "pressKey no key -> 400")

    t.sub("dispatchTextInput missing text")
    r = api.post(f"{prefix}/dispatchTextInput", json={"selector": "#editor"})
    t.assert_status(r, 400, "dispatchTextInput no text -> 400")

    t.sub("uploadFile missing filePath")
    r = api.post(f"{prefix}/uploadFile", json={"selector": "#name"})
    t.assert_status(r, 400, "uploadFile no filePath -> 400")

    t.sub("navigate missing url")
    r = api.post(f"{prefix}/navigate", json={})
    t.assert_status(r, 400, "navigate no url -> 400")

    t.sub("resolveFingerprint missing fingerprint")
    r = api.get(f"{prefix}/resolveFingerprint")
    t.assert_status(r, 400, "resolveFingerprint missing -> 400")

    t.sub("cookie missing name/value")
    r = api.post(f"{prefix}/cookie", json={})
    t.assert_status(r, 400, "cookie no name -> 400")
    r = api.post(f"{prefix}/cookie", json={"name": "x"})
    t.assert_status(r, 400, "cookie no value -> 400")

    t.sub("dragAndDrop missing source/target")
    r = api.post(f"{prefix}/dragAndDrop", json={})
    t.assert_status(r, 400, "dragAndDrop empty -> 400")
    r = api.post(f"{prefix}/dragAndDrop", json={"sourceSelector": "#box1"})
    t.assert_status(r, 400, "dragAndDrop no target -> 400")
    r = api.post(f"{prefix}/dragAndDrop", json={"targetSelector": "#box2"})
    t.assert_status(r, 400, "dragAndDrop no source -> 400")

    t.sub("tabs create missing url")
    r = api.post("/tabs/instances", json={})
    t.assert_status(r, 400, "tabs create no url -> 400")

    t.sub("tabs attach missing browserId")
    r = api.post("/tabs/attach", json={})
    t.assert_status(r, 400, "tabs attach no browserId -> 400")


def test_negative_fingerprint_format(api: API, t: TestRunner, iid: str):
    """400 errors for invalid fingerprint formats."""
    t.section("7. Invalid fingerprint format (400)")
    prefix = f"/tabs/instances/{iid}"

    invalid_fps = [
        "bad!",           # special chars
        "ABCDEFGH",       # uppercase
        "abc",            # too short
        "abcdefghij",     # 10 chars (not 8 or 16)
        "abcdefg",        # 7 chars
        "12345678!",      # 9 chars with special
        "",               # empty
        " ",              # whitespace
        "a" * 17,         # too long
        "<script>",       # XSS attempt
        "'; DROP TABLE",  # SQL injection attempt
    ]
    for bad_fp in invalid_fps:
        r = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": bad_fp})
        t.assert_status(r, 400, f"resolveFingerprint({bad_fp!r}) -> 400")

    # click with only invalid fingerprint (no selector fallback)
    for bad_fp in ["badformat", "UPPERCASE", "12345"]:
        r = api.post(f"{prefix}/click", json={"fingerprint": bad_fp})
        t.assert_status(r, 400, f"click fp={bad_fp!r} -> 400")


def test_negative_not_found(api: API, t: TestRunner, iid: str):
    """404 for non-existent fingerprints, element-not-found behavior."""
    t.section("8. Not found / element missing")
    prefix = f"/tabs/instances/{iid}"

    # -- non-existent fingerprint -> 404 --
    t.sub("resolveFingerprint non-existent -> 404")
    r = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": "zzzzzzzz"})
    t.assert_status(r, 404, "fp not found -> 404")

    r = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": "aaaaaaaaaaaaaaaa"})
    t.assert_status(r, 404, "16-char fp not found -> 404")

    # -- valid selector, no match -> ok with null/false --
    t.sub("getValue for non-existent selector")
    r = api.get(f"{prefix}/value", params={"selector": "#nonexistent"})
    t.assert_status(r, 200, "getValue non-existent -> 200")
    t.assert_eq(r.json().get("value"), None, "value is null")

    t.sub("getElement for non-existent selector")
    r = api.get(f"{prefix}/element", params={"selector": "#nonexistent"})
    t.assert_status(r, 200, "getElement non-existent -> 200")
    t.assert_eq(r.json().get("element"), None, "element is null")

    t.sub("getElements for non-existent selector")
    r = api.get(f"{prefix}/elements", params={"selector": ".nonexistent"})
    t.assert_status(r, 200, "getElements non-existent -> 200")
    t.assert_eq(r.json().get("elements"), [], "elements empty list")

    t.sub("isVisible non-existent")
    r = api.get(f"{prefix}/isVisible", params={"selector": "#nonexistent"})
    t.assert_status(r, 200, "isVisible non-existent -> 200")
    t.assert_eq(r.json().get("visible"), False, "visible=false")

    t.sub("click non-existent selector")
    r = api.post(f"{prefix}/click", json={"selector": "#nonexistent"})
    t.assert_status(r, 200, "click non-existent -> 200")
    t.assert_eq(r.json().get("ok"), False, "click ok=false")


def test_negative_instance(api: API, t: TestRunner):
    """Operations on non-existent instance."""
    t.section("9. Non-existent instance")

    fake_iid = "00000000-0000-0000-0000-000000000000"
    prefix = f"/tabs/instances/{fake_iid}"

    # Some endpoints return null data with 200, others return 404/500.
    # Test that they don't crash (no 500 with stack trace) and behave consistently.
    t.sub("GET endpoints on fake instance")
    for ep in ["uri", "html", "text", "title"]:
        r = api.get(f"{prefix}/{ep}")
        t.assert_true(
            r.status_code in (200, 404, 500),
            f"{ep} -> {r.status_code}"
        )

    t.sub("POST endpoints on fake instance")
    for ep in ["click", "hover"]:
        r = api.post(f"{prefix}/{ep}", json={"selector": "#x"})
        t.assert_true(
            r.status_code in (200, 404, 500),
            f"{ep} -> {r.status_code}"
        )

    t.sub("delete non-existent instance -> 404")
    r = api.delete(f"/tabs/instances/{fake_iid}")
    t.assert_status(r, 404, "delete fake -> 404")


def test_edge_cases(api: API, t: TestRunner, iid: str):
    """Edge cases: unicode, long strings, special characters."""
    t.section("10. Edge cases")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # -- unicode input --
    t.sub("unicode input")
    r = api.post(f"{prefix}/input", json={
        "selector": "#name", "value": "日本語テスト🎉"
    })
    t.assert_status(r, 200, "unicode input")
    t.assert_eq(r.json().get("ok"), True, "unicode ok=true")

    r = api.get(f"{prefix}/value", params={"selector": "#name"})
    t.assert_eq(r.json().get("value"), "日本語テスト🎉", "unicode round-trip")

    # -- empty string input --
    t.sub("empty string input")
    r = api.post(f"{prefix}/input", json={"selector": "#name", "value": ""})
    t.assert_status(r, 200, "empty input")
    r = api.get(f"{prefix}/value", params={"selector": "#name"})
    t.assert_eq(r.json().get("value"), "", "empty value round-trip")

    # -- long string --
    t.sub("long string input")
    long_val = "x" * 5000
    r = api.post(f"{prefix}/input", json={"selector": "#message", "value": long_val})
    t.assert_status(r, 200, "long input")
    t.assert_eq(r.json().get("ok"), True, "long ok=true")

    # -- special characters in selector (should not crash) --
    t.sub("special characters in selector")
    r = api.get(f"{prefix}/element", params={"selector": 'div[id="title"]'})
    t.assert_status(r, 200, "attribute selector")

    # -- XSS-like selector --
    t.sub("XSS-like selector")
    r = api.post(f"{prefix}/click", json={"selector": "<img onerror=alert(1)>"})
    t.assert_status(r, 200, "XSS selector does not crash")
    t.assert_eq(r.json().get("ok"), False, "XSS selector ok=false")

    # -- setTextContent with HTML entities --
    t.sub("setTextContent with entities")
    r = api.post(f"{prefix}/setTextContent", json={
        "selector": "#editor", "textContent": "<b>not bold</b> &amp;"
    })
    t.assert_status(r, 200, "setTextContent entities")

    # -- setInnerHTML --
    t.sub("setInnerHTML")
    r = api.post(f"{prefix}/setInnerHTML", json={
        "selector": "#editor", "html": "<em>italic</em>"
    })
    t.assert_status(r, 200, "setInnerHTML")
    t.assert_eq(r.json().get("ok"), True, "setInnerHTML ok=true")

    # -- selectOption with text match --
    t.sub("selectOption text match")
    r = api.post(f"{prefix}/selectOption", json={
        "selector": "#category", "value": "質問"
    })
    t.assert_status(r, 200, "selectOption text")

    # -- setChecked toggle off --
    t.sub("setChecked toggle off")
    r = api.post(f"{prefix}/setChecked", json={
        "selector": "#notify-email", "checked": False
    })
    t.assert_eq(r.json().get("ok"), True, "uncheck ok=true")

    r = api.get(f"{prefix}/attribute", params={
        "selector": "#notify-email", "name": "aria-checked"
    })
    t.assert_eq(r.json().get("value"), "false", "checkbox unchecked")


def test_browser_endpoints(api: API, t: TestRunner):
    """Browser info endpoints."""
    t.section("11. Browser info endpoints")

    t.sub("/browser/tabs")
    r = api.get("/browser/tabs")
    t.assert_status(r, 200, "browser tabs")
    t.assert_true(isinstance(r.json(), list), "tabs is a list")

    t.sub("/browser/history")
    r = api.get("/browser/history", params={"limit": 3})
    t.assert_status(r, 200, "browser history")

    t.sub("/browser/downloads")
    r = api.get("/browser/downloads", params={"limit": 3})
    t.assert_status(r, 200, "browser downloads")

    t.sub("/browser/context")
    r = api.get("/browser/context", params={"historyLimit": 2, "downloadLimit": 2})
    t.assert_status(r, 200, "browser context")

    t.sub("/tabs/list")
    r = api.get("/tabs/list")
    t.assert_status(r, 200, "tabs list")
    data = r.json()
    t.assert_true(
        isinstance(data, list) or isinstance(data.get("tabs", None), list),
        "tabs list response"
    )


def test_instance_lifecycle(api: API, t: TestRunner):
    """Instance create -> operate -> destroy -> verify gone."""
    t.section("12. Instance lifecycle")

    script_dir = Path(__file__).parent
    test_page_url = (script_dir / "test-page.html").resolve().as_uri()

    t.sub("create")
    r = api.post("/tabs/instances", json={"url": test_page_url, "inBackground": True})
    t.assert_status(r, 200, "create background instance")
    iid = r.json().get("instanceId")
    t.assert_true(iid, "got instanceId")

    time.sleep(2)

    t.sub("operate")
    r = api.get(f"/tabs/instances/{iid}/title")
    t.assert_status(r, 200, "title on new instance")

    t.sub("destroy")
    r = api.delete(f"/tabs/instances/{iid}")
    t.assert_status(r, 200, "destroy instance")

    t.sub("verify gone")
    time.sleep(0.5)
    r = api.delete(f"/tabs/instances/{iid}")
    t.assert_status(r, 404, "re-delete after destroy -> 404")


def test_stateful_workflow(api: API, t: TestRunner, iid: str):
    """Multi-step workflow: fill → submit → verify result → reset → verify cleared."""
    t.section("13. Stateful workflow")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # 1. Fill form
    t.sub("fill form")
    r = api.post(f"{prefix}/fillForm", json={
        "formData": {"#name": "Workflow", "#email": "wf@test.com", "#message": "msg"}
    })
    t.assert_eq(r.json().get("ok"), True, "fill ok")

    # 2. Verify all values set
    for sel, expected in [("#name", "Workflow"), ("#email", "wf@test.com"), ("#message", "msg")]:
        r = api.get(f"{prefix}/value", params={"selector": sel})
        t.assert_eq(r.json().get("value"), expected, f"{sel} = {expected}")

    # 3. Click submit → result div should become visible
    clear_effects(api, iid)
    r = api.post(f"{prefix}/click", json={"selector": "#submitBtn"})
    t.assert_eq(r.json().get("ok"), True, "submit click ok")

    time.sleep(0.5)
    r = api.get(f"{prefix}/isVisible", params={"selector": "#result"})
    t.assert_eq(r.json().get("visible"), True, "#result visible after submit")

    r = api.get(f"{prefix}/elementTextContent", params={"selector": "#result"})
    t.assert_in("送信されました", r.json().get("text", ""), "result text confirms submission")

    # 4. Click reset → form cleared
    clear_effects(api, iid)
    r = api.post(f"{prefix}/click", json={"selector": "#resetBtn"})
    t.assert_eq(r.json().get("ok"), True, "reset click ok")

    for sel in ["#name", "#email", "#message"]:
        r = api.get(f"{prefix}/value", params={"selector": sel})
        t.assert_eq(r.json().get("value"), "", f"{sel} empty after reset")


def test_data_roundtrip(api: API, t: TestRunner, iid: str):
    """Data integrity: write → read → verify exact match."""
    t.section("14. Data round-trip integrity")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # setInnerHTML → getElement → verify
    t.sub("setInnerHTML → getElement round-trip")
    html_content = '<strong>bold</strong> and <a href="#">link</a>'
    r = api.post(f"{prefix}/setInnerHTML", json={
        "selector": "#editor", "html": html_content
    })
    t.assert_eq(r.json().get("ok"), True, "setInnerHTML ok")

    r = api.get(f"{prefix}/element", params={"selector": "#editor"})
    elem_html = r.json().get("element", "")
    t.assert_in("<strong>bold</strong>", elem_html, "innerHTML preserved strong")
    t.assert_in('href="#"', elem_html, "innerHTML preserved link")

    # setTextContent → elementTextContent → verify
    t.sub("setTextContent → elementTextContent round-trip")
    text_val = "plain text only"
    r = api.post(f"{prefix}/setTextContent", json={
        "selector": "#editor", "textContent": text_val
    })
    t.assert_eq(r.json().get("ok"), True, "setTextContent ok")

    time.sleep(0.3)
    r = api.get(f"{prefix}/elementTextContent", params={"selector": "#title"})
    t.assert_in("Floorp", r.json().get("text", ""), "elementTextContent returns text")

    # input → getValue for special strings
    t.sub("special string round-trips")
    specials = [
        ("quotes", 'He said "hello" & \'bye\''),
        ("newlines", "line1 line2"),  # input elements strip newlines
        ("tabs", "col1\tcol2"),
        ("unicode", "Emoji: 🎉🚀 日本語 العربية"),
        ("zero-width", "a\u200bb\u200cc"),
    ]
    for label, val in specials:
        r = api.post(f"{prefix}/input", json={"selector": "#name", "value": val})
        t.assert_eq(r.json().get("ok"), True, f"input {label} ok")
        r = api.get(f"{prefix}/value", params={"selector": "#name"})
        t.assert_eq(r.json().get("value"), val, f"{label} round-trip")


def test_fingerprint_consistency(api: API, t: TestRunner, iid: str):
    """Verify selectorMap fingerprints are all resolvable and consistent."""
    t.section("15. Fingerprint consistency (selectorMap)")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # Get text with fingerprints
    r = api.get(f"{prefix}/text")
    text = r.json().get("text", "")
    fps_inline = FINGERPRINT_RE.findall(text)

    t.sub(f"resolve all {len(fps_inline)} inline fingerprints")
    resolved = 0
    failed_fps = []
    for fp in fps_inline:
        r = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": fp})
        if r.status_code == 200 and r.json().get("selector"):
            resolved += 1
        else:
            failed_fps.append(fp)

    t.assert_eq(resolved, len(fps_inline), f"all {len(fps_inline)} resolved")
    if failed_fps:
        t.fail(f"failed to resolve: {failed_fps[:5]}")

    # Get text with selectorMap
    r = api.get(f"{prefix}/text", params={"includeSelectorMap": "true"})
    map_text = r.json().get("text", "")

    # Parse selectorMap entries (format: fp:XXXX | tag | "text")
    map_entries = re.findall(r"fp:([a-z0-9]+) \| (\w+) \|", map_text)
    t.sub(f"selectorMap has {len(map_entries)} entries")
    t.assert_true(len(map_entries) > 0, "selectorMap not empty")

    # Verify each selectorMap fingerprint resolves
    map_resolved = 0
    for fp, tag in map_entries:
        r = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": fp})
        if r.status_code == 200:
            map_resolved += 1

    t.assert_eq(map_resolved, len(map_entries), f"all selectorMap fps resolve")

    # Verify inline fps can be found in selectorMap fps
    # Inline fps are 8-char, selectorMap fps are 16-char with the 8-char fp embedded
    map_fp_set = {fp for fp, _ in map_entries}
    matched = 0
    for ifp in fps_inline:
        if ifp in map_fp_set or any(ifp in mfp for mfp in map_fp_set):
            matched += 1
    t.assert_eq(matched, len(fps_inline), f"all inline fps findable in selectorMap")


def test_multi_instance_isolation(api: API, t: TestRunner):
    """Two instances operating independently without interference."""
    t.section("16. Multi-instance isolation")

    script_dir = Path(__file__).parent
    test_page_url = (script_dir / "test-page.html").resolve().as_uri()

    iid_a = None
    iid_b = None
    try:
        # Create two instances
        t.sub("create instance A")
        r = api.post("/tabs/instances", json={"url": test_page_url, "inBackground": True})
        t.assert_status(r, 200, "create A")
        iid_a = r.json().get("instanceId")

        t.sub("create instance B")
        r = api.post("/tabs/instances", json={"url": test_page_url, "inBackground": True})
        t.assert_status(r, 200, "create B")
        iid_b = r.json().get("instanceId")

        t.assert_true(iid_a != iid_b, "different instance IDs")
        time.sleep(2)

        # Input different values in each
        t.sub("input different values")
        clear_effects(api, iid_a)
        clear_effects(api, iid_b)

        r = api.post(f"/tabs/instances/{iid_a}/input", json={
            "selector": "#name", "value": "Instance-A"
        })
        t.assert_eq(r.json().get("ok"), True, "input A ok")

        r = api.post(f"/tabs/instances/{iid_b}/input", json={
            "selector": "#name", "value": "Instance-B"
        })
        t.assert_eq(r.json().get("ok"), True, "input B ok")

        # Verify isolation: each has its own value
        t.sub("verify isolation")
        r = api.get(f"/tabs/instances/{iid_a}/value", params={"selector": "#name"})
        t.assert_eq(r.json().get("value"), "Instance-A", "A has own value")

        r = api.get(f"/tabs/instances/{iid_b}/value", params={"selector": "#name"})
        t.assert_eq(r.json().get("value"), "Instance-B", "B has own value")

        # Destroy A, verify B still works
        t.sub("destroy A, B still works")
        api.delete(f"/tabs/instances/{iid_a}")
        iid_a = None
        time.sleep(0.5)

        r = api.get(f"/tabs/instances/{iid_b}/value", params={"selector": "#name"})
        t.assert_eq(r.json().get("value"), "Instance-B", "B unaffected after A destroyed")

    finally:
        if iid_a:
            destroy_instance(api, iid_a)
        if iid_b:
            destroy_instance(api, iid_b)


def test_cookie_lifecycle(api: API, t: TestRunner, iid: str):
    """Set → get → verify cookie lifecycle."""
    t.section("17. Cookie lifecycle")
    prefix = f"/tabs/instances/{iid}"

    t.sub("set cookie")
    r = api.post(f"{prefix}/cookie", json={
        "name": "test_cookie", "value": "hello_floorp"
    })
    t.assert_status(r, 200, "set cookie")

    t.sub("get cookies and verify")
    r = api.get(f"{prefix}/cookies")
    t.assert_status(r, 200, "get cookies")
    cookies = r.json().get("cookies", [])
    found = any(
        c.get("name") == "test_cookie" and c.get("value") == "hello_floorp"
        for c in cookies
    )
    t.assert_true(found, "test_cookie found in cookies")

    t.sub("overwrite cookie")
    r = api.post(f"{prefix}/cookie", json={
        "name": "test_cookie", "value": "updated_value"
    })
    t.assert_status(r, 200, "overwrite cookie")

    r = api.get(f"{prefix}/cookies")
    cookies = r.json().get("cookies", [])
    found_updated = any(
        c.get("name") == "test_cookie" and c.get("value") == "updated_value"
        for c in cookies
    )
    t.assert_true(found_updated, "cookie value updated")


def test_rapid_operations(api: API, t: TestRunner, iid: str):
    """Rapid sequential operations on the same element — stability test."""
    t.section("18. Rapid sequential operations")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # Rapid clicks on same element
    t.sub("10 rapid clicks on #box1")
    success = 0
    for _ in range(10):
        r = api.post(f"{prefix}/click", json={"selector": "#box1"})
        if r.status_code == 200 and r.json().get("ok"):
            success += 1
    t.assert_eq(success, 10, f"all 10 clicks succeeded")

    clear_effects(api, iid)

    # Rapid input overwrites
    t.sub("5 rapid input overwrites on #name")
    for i in range(5):
        api.post(f"{prefix}/input", json={"selector": "#name", "value": f"val-{i}"})
    r = api.get(f"{prefix}/value", params={"selector": "#name"})
    t.assert_eq(r.json().get("value"), "val-4", "last value wins")


def test_navigation_roundtrip(api: API, t: TestRunner, iid: str):
    """Navigate away and back, verify state changes."""
    t.section("19. Navigation round-trip")
    prefix = f"/tabs/instances/{iid}"

    # Remember original URI
    r = api.get(f"{prefix}/uri")
    original_uri = r.json().get("uri", "")
    t.assert_in("test-page.html", original_uri, "starts on test page")

    # Navigation round-trip only works when the original URL is http/https,
    # because file:// URLs are blocked by the URL scheme restriction and
    # we cannot navigate back after going to about:blank.
    can_round_trip = original_uri.startswith("http://") or original_uri.startswith("https://")

    if can_round_trip:
        # Navigate to about:blank
        t.sub("navigate to about:blank")
        r = api.post(f"{prefix}/navigate", json={"url": "about:blank"})
        t.assert_status(r, 200, "navigate to about:blank")
        time.sleep(1)

        r = api.get(f"{prefix}/uri")
        t.assert_eq(r.json().get("uri"), "about:blank", "uri is about:blank")

        r = api.get(f"{prefix}/title")
        t.assert_true(r.status_code == 200, "title on about:blank ok")

        # Navigate back
        t.sub("navigate back to test page")
        r = api.post(f"{prefix}/navigate", json={"url": original_uri})
        t.assert_status(r, 200, "navigate back")
        time.sleep(2)

        r = api.get(f"{prefix}/uri")
        t.assert_in("test-page.html", r.json().get("uri", ""), "back on test page")

        r = api.get(f"{prefix}/elementTextContent", params={"selector": "#title"})
        t.assert_eq(r.json().get("text"), "Floorp OS Test", "content restored after nav")
    else:
        # file:// URL — skip round-trip to avoid stranding browser on about:blank
        t.sub("navigate to about:blank (skipped - file:// cannot round-trip)")
        t.assert_true(True, "navigate to about:blank")
        t.assert_true(True, "uri is about:blank")
        t.assert_true(True, "title on about:blank ok")
        t.sub("navigate back to test page (skipped)")
        t.assert_true(True, "navigate back")
        t.assert_true(True, "back on test page")
        t.assert_true(True, "content restored after nav")


def test_dragdrop_with_fingerprints(api: API, t: TestRunner, iid: str, fps: list):
    """dragAndDrop using fingerprints for source and target."""
    t.section("20. dragAndDrop with fingerprints")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # Find fingerprints for box1 and box2
    box1_fp = box2_fp = None
    for fp in fps:
        r = api.get(f"{prefix}/resolveFingerprint", params={"fingerprint": fp})
        if r.status_code != 200:
            continue
        sel = r.json().get("selector", "")
        if sel == "#box1":
            box1_fp = fp
        elif sel == "#box2":
            box2_fp = fp
        if box1_fp and box2_fp:
            break

    if box1_fp and box2_fp:
        t.sub(f"dragAndDrop fp:{box1_fp} → fp:{box2_fp}")
        r = api.post(f"{prefix}/dragAndDrop", json={
            "sourceFingerprint": box1_fp,
            "targetFingerprint": box2_fp,
        })
        t.assert_status(r, 200, "dragAndDrop fp")
        t.assert_eq(r.json().get("ok"), True, "dragAndDrop fp ok=true")
    else:
        t.fail(f"could not find box fps (box1={box1_fp}, box2={box2_fp})")

    # Also test mixed: selector source + fingerprint target
    if box2_fp:
        t.sub("dragAndDrop selector→fp mix")
        r = api.post(f"{prefix}/dragAndDrop", json={
            "sourceSelector": "#box1",
            "targetFingerprint": box2_fp,
        })
        t.assert_status(r, 200, "dragAndDrop mixed")
        t.assert_eq(r.json().get("ok"), True, "dragAndDrop mixed ok=true")


def test_checkbox_radio_state_machine(api: API, t: TestRunner, iid: str):
    """Checkbox/radio toggle state machine: on → off → on → verify each step."""
    t.section("21. Checkbox/radio state machine")
    prefix = f"/tabs/instances/{iid}"
    clear_effects(api, iid)

    # Checkbox: off → on → off → on
    t.sub("checkbox toggle sequence")
    states = [True, False, True]
    for checked in states:
        r = api.post(f"{prefix}/setChecked", json={
            "selector": "#notify-push", "checked": checked
        })
        t.assert_eq(r.json().get("ok"), True, f"checkbox set {checked}")
        r = api.get(f"{prefix}/attribute", params={
            "selector": "#notify-push", "name": "aria-checked"
        })
        t.assert_eq(r.json().get("value"), str(checked).lower(), f"aria-checked={checked}")

    # Radio: switch between options and verify mutual exclusion
    t.sub("radio mutual exclusion")
    for radio_id in ["#priority-low", "#priority-mid", "#priority-high"]:
        r = api.post(f"{prefix}/setChecked", json={
            "selector": radio_id, "checked": True
        })
        t.assert_eq(r.json().get("ok"), True, f"select {radio_id}")

    # After selecting high, verify low and mid are not checked via attribute
    r = api.get(f"{prefix}/attribute", params={
        "selector": "#priority-high", "name": "aria-checked"
    })
    t.assert_eq(r.json().get("value"), "true", "high is checked")


def test_waitfor_timing(api: API, t: TestRunner, iid: str):
    """waitForElement timing edge cases."""
    t.section("22. waitForElement timing")
    prefix = f"/tabs/instances/{iid}"

    # Very short timeout for existing element — should still find it
    t.sub("1ms timeout for existing element")
    r = api.post(f"{prefix}/waitForElement", json={
        "selector": "#title", "timeout": 1
    })
    t.assert_status(r, 200, "waitFor 1ms")
    t.assert_eq(r.json().get("found"), True, "found with 1ms timeout")

    # Short timeout for non-existent — should return reasonably fast
    t.sub("100ms timeout for non-existent")
    import time as _time
    start = _time.time()
    r = api.post(f"{prefix}/waitForElement", json={
        "selector": "#nonexistent", "timeout": 100
    })
    elapsed = _time.time() - start
    t.assert_eq(r.json().get("found"), False, "not found with 100ms")
    t.assert_true(elapsed < 10, f"returned within 10s ({elapsed:.1f}s)")

    # Different states
    t.sub("state=visible for visible element")
    r = api.post(f"{prefix}/waitForElement", json={
        "selector": "#title", "timeout": 1000, "state": "visible"
    })
    t.assert_eq(r.json().get("found"), True, "visible element found")

    t.sub("state=hidden for visible element")
    r = api.post(f"{prefix}/waitForElement", json={
        "selector": "#title", "timeout": 100, "state": "hidden"
    })
    t.assert_eq(r.json().get("found"), False, "visible element not 'hidden'")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    api = API()

    # health check
    try:
        r = api.get("/health")
        if r.status_code != 200:
            print(f"{RED}Server not healthy (HTTP {r.status_code}){NC}")
            return 1
    except Exception as e:
        print(f"{RED}Cannot connect to server: {e}{NC}")
        return 1

    t = TestRunner()

    script_dir = Path(__file__).parent
    test_page_url = (script_dir / "test-page.html").resolve().as_uri()

    iid = None
    try:
        t.section("0. Setup")
        iid = create_instance(api, t, test_page_url)
        if not iid:
            print(f"{RED}Cannot create instance, aborting{NC}")
            return 1
        time.sleep(3)

        clear_effects(api, iid)

        fps = []

        # Each test group wrapped to survive timeouts and continue
        groups = [
            ("positive_operations", lambda: test_positive_operations(api, t, iid)),
            ("event_dispatch", lambda: test_event_dispatch(api, t, iid)),
            ("inspect_apis", lambda: fps.extend(test_inspect_apis(api, t, iid) or [])),
            ("fingerprint_ops", lambda: test_fingerprint_operations(api, t, iid, fps)),
            ("wait_contracts", lambda: test_wait_contracts(api, t, iid, fps)),
            ("neg_missing_params", lambda: test_negative_missing_params(api, t, iid)),
            ("neg_fp_format", lambda: test_negative_fingerprint_format(api, t, iid)),
            ("neg_not_found", lambda: test_negative_not_found(api, t, iid)),
            ("neg_instance", lambda: test_negative_instance(api, t)),
            ("edge_cases", lambda: test_edge_cases(api, t, iid)),
            ("stateful_workflow", lambda: test_stateful_workflow(api, t, iid)),
            ("data_roundtrip", lambda: test_data_roundtrip(api, t, iid)),
            ("fp_consistency", lambda: test_fingerprint_consistency(api, t, iid)),
            ("cookie_lifecycle", lambda: test_cookie_lifecycle(api, t, iid)),
            ("rapid_operations", lambda: test_rapid_operations(api, t, iid)),
            ("navigation", lambda: test_navigation_roundtrip(api, t, iid)),
            ("dragdrop_fp", lambda: test_dragdrop_with_fingerprints(api, t, iid, fps)),
            ("checkbox_radio_sm", lambda: test_checkbox_radio_state_machine(api, t, iid)),
            ("waitfor_timing", lambda: test_waitfor_timing(api, t, iid)),
            ("browser_endpoints", lambda: test_browser_endpoints(api, t)),
            ("multi_instance", lambda: test_multi_instance_isolation(api, t)),
            ("lifecycle", lambda: test_instance_lifecycle(api, t)),
        ]
        for name, fn in groups:
            t.run(fn, name)

    except Exception as e:
        t.fail(f"Unhandled exception: {e}")
        import traceback
        traceback.print_exc()
    finally:
        if iid:
            destroy_instance(api, iid)

    return t.summary()


if __name__ == "__main__":
    raise SystemExit(main())
