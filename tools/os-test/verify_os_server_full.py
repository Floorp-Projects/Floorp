import base64
import http.server
import json
import os
import shutil
import socketserver
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from dataclasses import dataclass

BASE_URL = "http://127.0.0.1:58261"


@dataclass
class TestResult:
    name: str
    status: str
    detail: str | None = None


TEST_RESULTS: list[TestResult] = []


def record_result(name: str, status: str, detail: str | None = None) -> None:
    TEST_RESULTS.append(TestResult(name=name, status=status, detail=detail))

TEST_PAGE_HTML = """<!doctype html>
<html>
<head>
    <meta charset=\"utf-8\" />
    <title>Floorp OS Test</title>
    <style>
        body { font-family: sans-serif; margin: 20px; }
        #spacer { height: 1200px; }
        #footer { margin-top: 20px; }
        #dragSource, #dragTarget { padding: 8px; border: 1px dashed #888; margin: 4px 0; }
        #dragTarget { min-height: 40px; }
        .box { padding: 4px; margin: 4px 0; border: 1px solid #ccc; }
    </style>
    <script>
        function preventSubmit(event) {
            event.preventDefault();
            const log = document.getElementById('log');
            if (log) { log.textContent = 'submitted'; }
        }
    </script>
</head>
<body>
    <h1 id=\"title\" data-test=\"title\">Floorp OS Test</h1>
    <p class=\"desc\">Example page for OS server API tests.</p>
    <a id=\"link\" href=\"javascript:void(0)\">Do nothing link</a>

    <form id=\"testForm\" class=\"box\" onsubmit=\"preventSubmit(event)\">
        <input id=\"name\" name=\"name\" value=\"Alice\" />
        <input id=\"email\" name=\"email\" type=\"email\" value=\"alice@example.com\" />
        <textarea id=\"message\">Hello</textarea>
        <select id=\"color\"><option value=\"red\">Red</option><option value=\"blue\">Blue</option></select>
        <input id=\"agree\" type=\"checkbox\" />
        <input id=\"fileInput\" type=\"file\" />
        <button id=\"submitBtn\" type=\"submit\">Submit</button>
        <button id=\"resetBtn\" type=\"reset\">Reset</button>
    </form>

    <div id=\"hoverTarget\" class=\"box\">Hover me</div>
    <div id=\"dblTarget\" class=\"box\">Double click me</div>
    <div id=\"contextTarget\" class=\"box\">Right click me</div>
    <div id=\"dragSource\" draggable=\"true\">Drag me</div>
    <div id=\"dragTarget\">Drop here</div>
    <div id=\"editable\" class=\"box\" contenteditable=\"true\">Editable</div>
    <div id=\"eventTarget\" class=\"box\">Event target</div>
    <div id=\"spacer\"></div>
    <div id=\"footer\" class=\"box\">Footer</div>
    <div id=\"log\" class=\"box\"></div>
</body>
</html>"""

TEST_PAGE_URL = "data:text/html;base64," + base64.b64encode(
    TEST_PAGE_HTML.encode("utf-8")
).decode("ascii")


def create_temp_upload_file() -> str:
    fd, path = tempfile.mkstemp(prefix="floorp-os-test-", suffix=".txt")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write("Floorp OS server upload test\n")
    return path


def start_test_http_server() -> tuple[http.server.ThreadingHTTPServer, str, str]:
    """Start a tiny HTTP server serving the test page. Returns (server, url, tempdir)."""

    tmpdir = tempfile.mkdtemp(prefix="floorp-os-test-http-")
    page_path = Path(tmpdir) / "test-page.html"
    page_path.write_text(TEST_PAGE_HTML, encoding="utf-8")

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=tmpdir, **kwargs)

        def log_message(self, format, *args):  # noqa: A003
            # Silence request logs for clarity
            return

    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    port = httpd.server_address[1]

    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()

    url = f"http://127.0.0.1:{port}/test-page.html"
    return httpd, url, tmpdir


def stop_test_http_server(httpd: http.server.ThreadingHTTPServer, tmpdir: str):
    try:
        httpd.shutdown()
        httpd.server_close()
    except Exception:
        pass
    shutil.rmtree(tmpdir, ignore_errors=True)


def make_request(path, method="GET", data=None, *, stream=False, timeout=60):
    url = f"{BASE_URL}{path}"
    req = urllib.request.Request(url, method=method)

    json_data = None
    if data is not None:
        json_data = json.dumps(data).encode("utf-8")
        req.add_header("Content-Type", "application/json")
        req.add_header("Content-Length", str(len(json_data)))

    try:
        with urllib.request.urlopen(
            req, data=json_data if data is not None else None, timeout=timeout
        ) as response:
            status = response.status
            if stream:
                return status, "<stream>"
            body = response.read().decode("utf-8")
            try:
                json_body = json.loads(body)
                return status, json_body
            except json.JSONDecodeError:
                return status, body
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8")
    except urllib.error.URLError as e:
        print(f"Failed to connect to {url}: {e.reason}")
        return None, None
    except Exception as e:  # noqa: BLE001
        print(f"Unexpected error for {url}: {e}")
        return None, None


def _short(val):
    try:
        txt = json.dumps(val, ensure_ascii=False)
    except Exception:
        txt = str(val)
    if len(txt) > 160:
        return txt[:157] + "..."
    return txt


def run_value_test(
    name,
    path,
    *,
    key=None,
    expected=None,
    predicate=None,
    expect_description=None,
    method="GET",
    data=None,
    expected_status=200,
    timeout=8,
):
    """Run a request and validate the returned value."""

    allowed = (
        tuple(expected_status)
        if isinstance(expected_status, (list, tuple, set))
        else (expected_status,)
    )

    print(f"Testing {name} ({method} {path})... ", end="")
    status, body = make_request(path, method, data, timeout=timeout)

    if status is None:
        print("SKIPPED (Connection failed)")
        record_result(name, "SKIPPED", "connection failed")
        return None

    if status not in allowed:
        print(f"FAILED (Status: {status}, Expected: {allowed})")
        print(f"  Response: {body}")
        record_result(name, "FAILED", f"status {status}, expected {allowed}")
        return None

    actual = body
    if key is not None:
        if isinstance(body, dict):
            actual = body.get(key)
        else:
            actual = None

    ok = True
    if predicate is not None:
        try:
            ok = bool(predicate(actual))
        except Exception as e:  # noqa: BLE001
            ok = False
            print(f"FAILED (Predicate error: {e})")
            record_result(name, "FAILED", f"predicate error: {e}")
            return actual
    elif expected is not None:
        ok = actual == expected

    if ok:
        descr = expect_description
        if descr is None and expected is not None:
            descr = f"== { _short(expected) }"
        elif descr is None:
            descr = "matches"
        print(f"OK (value {_short(actual)}; {descr})")
        record_result(name, "OK", descr)
    else:
        descr = expect_description or f"Expected { _short(expected) }"
        print(f"FAILED (value {_short(actual)}; {descr})")
        record_result(name, "FAILED", descr)

    return actual


def run_test(
    name,
    path,
    method="GET",
    data=None,
    expected_status=200,
    extract_key=None,
    *,
    stream=False,
    timeout=8,
):
    allowed = (
        tuple(expected_status)
        if isinstance(expected_status, (list, tuple, set))
        else (expected_status,)
    )

    print(f"Testing {name} ({method} {path})...", end=" ")
    status, body = make_request(path, method, data, stream=stream, timeout=timeout)

    if status is None:
        print("SKIPPED (Connection failed)")
        record_result(name, "SKIPPED", "connection failed")
        return None

    if status in allowed:
        suffix = "" if len(allowed) == 1 and status == allowed[0] else f"(status {status})"
        print(f"OK {suffix}".strip())
        record_result(name, "OK", suffix or None)
        if extract_key and isinstance(body, dict):
            return body.get(extract_key)
        return body

    print(f"FAILED (Status: {status}, Expected: {allowed})")
    print(f"  Response: {body}")
    record_result(name, "FAILED", f"status {status}, expected {allowed}")
    return None


def print_summary() -> None:
    if not TEST_RESULTS:
        print("Test summary: no tests were recorded.")
        return

    total = len(TEST_RESULTS)
    passed = sum(1 for r in TEST_RESULTS if r.status == "OK")
    failed = sum(1 for r in TEST_RESULTS if r.status == "FAILED")
    skipped = sum(1 for r in TEST_RESULTS if r.status == "SKIPPED")

    print("Test summary:")
    print(f"  Total: {total}")
    print(f"  Passed: {passed}")
    print(f"  Failed: {failed}")
    print(f"  Skipped: {skipped}")

    failed_results = [r for r in TEST_RESULTS if r.status == "FAILED"]
    if failed_results:
        print("  Failed tests:")
        for r in failed_results:
            detail = f" ({r.detail})" if r.detail else ""
            print(f"    - {r.name}{detail}")


def test_shared_automation(
    base_path,
    instance_id,
    *,
    include_get_element=True,
    upload_file_path=None,
    test_page_url=TEST_PAGE_URL,
):
    print(f"  [Shared Automation for {base_path}]")
    prefix = f"{base_path}/instances/{instance_id}"

    def q(s: str) -> str:
        return urllib.parse.quote(s)

    # Navigate to test page with rich elements
    run_test(
        "Navigate to test page",
        f"{prefix}/navigate",
        method="POST",
        data={"url": test_page_url},
    )
    time.sleep(2)

    # Basic getters & presence (with value verification)
    run_value_test(
        "Get URI",
        f"{prefix}/uri",
        key="uri",
        expected=test_page_url,
    )
    run_value_test(
        "Get HTML",
        f"{prefix}/html",
        key="html",
        predicate=lambda v: isinstance(v, str) and "Floorp OS Test" in v,
        expect_description="contains 'Floorp OS Test'",
    )
    run_value_test(
        "Wait For Element (#title)",
        f"{prefix}/waitForElement",
        method="POST",
        data={"selector": "#title", "timeout": 2000},
        key="found",
        expected=True,
    )
    if include_get_element:
        run_test("Get Element Handle (#title)", f"{prefix}/element?selector={q('#title')}")

    run_value_test(
        "Get Element Text (#title)",
        f"{prefix}/elementText?selector={q('#title')}",
        key="text",
        expected="Floorp OS Test",
    )
    run_value_test(
        "Get Elements (<input>)",
        f"{prefix}/elements?selector={q('input')}",
        key="elements",
        predicate=lambda v: isinstance(v, list) and len(v) >= 3,
        expect_description=">= 3 elements",
    )
    run_value_test(
        "Get Element By Text",
        f"{prefix}/elementByText?text={q('Floorp OS Test')}",
        key="element",
        predicate=lambda v: isinstance(v, str) and "Floorp OS Test" in v,
        expect_description="contains 'Floorp OS Test'",
    )
    run_value_test(
        "Get Element Text Content (#message)",
        f"{prefix}/elementTextContent?selector={q('#message')}",
        key="text",
        expected="Hello",
    )

    # Screenshots
    run_test("Viewport Screenshot", f"{prefix}/screenshot")
    run_test("Element Screenshot (#title)", f"{prefix}/elementScreenshot?selector={q('#title')}")
    run_test("Full Page Screenshot", f"{prefix}/fullPageScreenshot")
    run_test(
        "Region Screenshot",
        f"{prefix}/regionScreenshot",
        method="POST",
        data={"rect": {"x": 0, "y": 0, "width": 200, "height": 200}},
    )

    # Form/Input operations
    run_test(
        "Fill Form",
        f"{prefix}/fillForm",
        method="POST",
        data={
            "formData": {
                "#name": "山田太郎",
                "#email": "yamada@example.com",
                "#message": "Floorp OS Server API test",
            },
            "typingMode": False,
        },
        timeout=30,
    )
    run_value_test(
        "Get Value (#name)",
        f"{prefix}/value?selector={q('#name')}",
        key="value",
        expected="山田太郎",
    )
    run_value_test(
        "Get Value (#email)",
        f"{prefix}/value?selector={q('#email')}",
        key="value",
        expected="yamada@example.com",
    )
    run_value_test(
        "Get Value (#message)",
        f"{prefix}/value?selector={q('#message')}",
        key="value",
        expected="Floorp OS Server API test",
    )
    run_test(
        "Submit Form",
        f"{prefix}/submit",
        method="POST",
        data={"selector": "#testForm"},
    )
    run_test(
        "Clear Input (#name)",
        f"{prefix}/clearInput",
        method="POST",
        data={"selector": "#name"},
    )
    run_value_test(
        "Get Value (#name after clear)",
        f"{prefix}/value?selector={q('#name')}",
        key="value",
        expected="",
    )

    # Element state
    run_value_test(
        "Get Attribute (#title data-test)",
        f"{prefix}/attribute?selector={q('#title')}&name=data-test",
        key="value",
        expected="title",
    )
    run_value_test(
        "Is Visible (#title)",
        f"{prefix}/isVisible?selector={q('#title')}",
        key="visible",
        expected=True,
    )
    run_value_test(
        "Is Enabled (#name)",
        f"{prefix}/isEnabled?selector={q('#name')}",
        key="enabled",
        expected=True,
    )
    run_test(
        "Select Option (#color=red)",
        f"{prefix}/selectOption",
        method="POST",
        data={"selector": "#color", "value": "red"},
    )
    run_value_test(
        "Get Value (#color)",
        f"{prefix}/value?selector={q('#color')}",
        key="value",
        expected="red",
    )
    run_test(
        "Set Checked (#agree)",
        f"{prefix}/setChecked",
        method="POST",
        data={"selector": "#agree", "checked": True},
    )
    run_value_test(
        "Get Attribute (#agree aria-checked)",
        f"{prefix}/attribute?selector={q('#agree')}&name=aria-checked",
        key="value",
        expected="true",
    )

    # Pointer actions
    run_test(
        "Hover (#hoverTarget)",
        f"{prefix}/hover",
        method="POST",
        data={"selector": "#hoverTarget"},
    )
    run_test(
        "Scroll To (#footer)",
        f"{prefix}/scrollTo",
        method="POST",
        data={"selector": "#footer"},
    )
    run_value_test(
        "Page Title",
        f"{prefix}/title",
        key="title",
        expected="Floorp OS Test",
    )
    run_test(
        "Double Click (#dblTarget)",
        f"{prefix}/doubleClick",
        method="POST",
        data={"selector": "#dblTarget"},
    )
    run_test(
        "Right Click (#contextTarget)",
        f"{prefix}/rightClick",
        method="POST",
        data={"selector": "#contextTarget"},
    )
    run_test(
        "Focus (#name)",
        f"{prefix}/focus",
        method="POST",
        data={"selector": "#name"},
    )
    run_test(
        "Drag And Drop",
        f"{prefix}/dragAndDrop",
        method="POST",
        data={"sourceSelector": "#dragSource", "targetSelector": "#dragTarget"},
    )
    run_test(
        "Click (#link)",
        f"{prefix}/click",
        method="POST",
        data={"selector": "#link"},
    )

    # Content updates
    run_test(
        "Set InnerHTML (#editable)",
        f"{prefix}/setInnerHTML",
        method="POST",
        data={"selector": "#editable", "html": "<b>Bold</b>"},
    )
    run_value_test(
        "Get Element Text Content (#editable after innerHTML)",
        f"{prefix}/elementTextContent?selector={q('#editable')}",
        key="text",
        expected="Bold",
    )
    run_test(
        "Set TextContent (#editable)",
        f"{prefix}/setTextContent",
        method="POST",
        data={"selector": "#editable", "text": "Plain text"},
    )
    run_value_test(
        "Get Element Text Content (#editable after text)",
        f"{prefix}/elementTextContent?selector={q('#editable')}",
        key="text",
        expected="Plain text",
    )
    run_test(
        "Dispatch Event (#eventTarget)",
        f"{prefix}/dispatchEvent",
        method="POST",
        data={
            "selector": "#eventTarget",
            "eventType": "custom-event",
            "options": {"bubbles": True, "cancelable": True},
        },
    )

    # Typing & keyboard
    run_test(
        "Input (typing mode)",
        f"{prefix}/input",
        method="POST",
        data={
            "selector": "#name",
            "value": "Typed Text",
            "typingMode": True,
            "typingDelayMs": 10,
        },
        expected_status=200,
    )
    run_value_test(
        "Get Value (#name after typing)",
        f"{prefix}/value?selector={q('#name')}",
        key="value",
        expected="Typed Text",
    )

    # File upload
    if upload_file_path:
        run_test(
            "Upload File",
            f"{prefix}/uploadFile",
            method="POST",
            data={"selector": "#fileInput", "filePath": upload_file_path},
            expected_status=200,
        )
        run_value_test(
            "Get Value (#fileInput)",
            f"{prefix}/value?selector={q('#fileInput')}",
            key="value",
            predicate=lambda v: isinstance(v, str)
            and (
                not v
                or os.path.basename(upload_file_path) in v
            ),
            expect_description="empty (browser-hides) or contains uploaded filename",
        )

    # Misc automation
    run_test(
        "Wait For Network Idle",
        f"{prefix}/waitForNetworkIdle",
        method="POST",
        data={"timeout": 2000},
    )
    run_test("Accept Alert", f"{prefix}/acceptAlert", method="POST")
    run_test("Dismiss Alert", f"{prefix}/dismissAlert", method="POST")

    # Cookie operations (navigate to example.com for a real host)
    run_test(
        "Navigate to example.com",
        f"{prefix}/navigate",
        method="POST",
        data={"url": "https://example.com"},
    )
    time.sleep(2)
    run_test(
        "Wait For Network Idle (example.com)",
        f"{prefix}/waitForNetworkIdle",
        method="POST",
        data={"timeout": 5000},
    )

    # Press key on a remote page (avoid actor crash on file://)
    run_test(
        "Press Key (Enter)",
        f"{prefix}/pressKey",
        method="POST",
        data={"key": "Enter"},
        expected_status=200,
        timeout=3,
    )

    run_test("Get Cookies", f"{prefix}/cookies")
    run_test(
        "Set Cookie",
        f"{prefix}/cookie",
        method="POST",
        data={
            "name": "floorp-test",
            "value": "123",
            "domain": "example.com",
            "path": "/",
            "sameSite": "Lax",
        },
        expected_status=200,
    )
    run_value_test(
        "Get Cookies (after set)",
        f"{prefix}/cookies",
        key="cookies",
        predicate=lambda cookies: isinstance(cookies, list)
        and any(c.get("name") == "floorp-test" and c.get("value") == "123" for c in cookies),
        expect_description="contains cookie floorp-test=123",
    )


def test_browser_info():
    print("\n[Browser Info]")
    run_test("Get Tabs", "/browser/tabs")
    run_test("Get History", "/browser/history")
    run_test("Get Downloads", "/browser/downloads")
    run_test("Get Context", "/browser/context")
    run_test("Browser Events Stream", "/browser/events", stream=True)


def test_workspaces():
    print("\n[Workspaces]")
    workspaces = run_test("List Workspaces", "/workspaces", extract_key="workspaces")
    run_test("Current Workspace", "/workspaces/current")
    run_test("Next Workspace", "/workspaces/next", method="POST")
    run_test("Previous Workspace", "/workspaces/previous", method="POST")

    if workspaces and len(workspaces) > 0:
        first_ws_id = workspaces[0].get("id")
        if first_ws_id:
            run_test(
                f"Switch to Workspace {first_ws_id}",
                f"/workspaces/{first_ws_id}/switch",
                method="POST",
            )


def test_tab_manager(upload_file_path: str, test_page_url: str):
    print("\n[Tab Manager]")

    run_test("List Managed Tabs", "/tabs/list")

    browser_tabs = run_test("Get Browser Tabs for Attach", "/browser/tabs")
    if browser_tabs and isinstance(browser_tabs, list) and len(browser_tabs) > 0:
        target_tab = browser_tabs[0]
        tab_id = target_tab.get("id")
        if tab_id:
            print(f"  Attempting to attach to browser tab {tab_id}...")
            attached_id = run_test(
                "Attach to Tab",
                "/tabs/attach",
                method="POST",
                data={"browserId": tab_id},
                extract_key="instanceId",
            )
            if attached_id:
                run_test("Destroy Attached Instance", f"/tabs/instances/{attached_id}", method="DELETE")

    tab_id = run_test(
        "Create Tab Instance",
        "/tabs/instances",
        method="POST",
        data={"url": test_page_url, "inBackground": True},
        extract_key="instanceId",
    )

    if not tab_id:
        print("Skipping remaining Tab Manager tests due to creation failure.")
        return

    try:
        run_test("Check Exists", f"/tabs/instances/{tab_id}/exists")
        run_test("Get Info", f"/tabs/instances/{tab_id}")

        run_test(
            "Navigate",
            f"/tabs/instances/{tab_id}/navigate",
            method="POST",
            data={"url": "https://example.com"},
        )
        time.sleep(2)

        run_test("Get URI", f"/tabs/instances/{tab_id}/uri")

        test_shared_automation(
            "/tabs",
            tab_id,
            include_get_element=True,
            upload_file_path=upload_file_path,
            test_page_url=test_page_url,
        )

    finally:
        run_test("Destroy Tab Instance", f"/tabs/instances/{tab_id}", method="DELETE")


def test_scraper(upload_file_path: str, test_page_url: str):
    print("\n[Scraper]")
    scraper_id = run_test("Create Scraper Instance", "/scraper/instances", method="POST", extract_key="instanceId")

    if not scraper_id:
        print("Skipping remaining Scraper tests due to creation failure.")
        return

    try:
        run_test("Check Exists", f"/scraper/instances/{scraper_id}/exists")

        test_shared_automation(
            "/scraper",
            scraper_id,
            include_get_element=False,
            upload_file_path=upload_file_path,
            test_page_url=test_page_url,
        )

    finally:
        run_test("Destroy Scraper Instance", f"/scraper/instances/{scraper_id}", method="DELETE")


def main():
    print(f"Verifying Floorp OS Server API at {BASE_URL}")
    print("Ensure 'floorp.os.enabled' is set to true in about:config")
    print("-" * 50)

    upload_file_path = create_temp_upload_file()
    httpd = None
    test_page_url = None
    tmpdir = None
    try:
        try:
            httpd, test_page_url, tmpdir = start_test_http_server()
        except Exception as e:  # noqa: BLE001
            print(f"Failed to start local test HTTP server: {e}")
            record_result("Start local test HTTP server", "FAILED", str(e))
            return

        if run_test("Health Check", "/health") is None:
            print("\nServer seems to be down or unreachable.")
            return

        try:
            test_browser_info()
            test_workspaces()
            test_tab_manager(upload_file_path, test_page_url)
            test_scraper(upload_file_path, test_page_url)
        finally:
            try:
                os.remove(upload_file_path)
            except OSError:
                pass
            if httpd and tmpdir:
                stop_test_http_server(httpd, tmpdir)
    finally:
        print("-" * 50)
        print("Verification complete.")
        print_summary()


if __name__ == "__main__":
    main()
