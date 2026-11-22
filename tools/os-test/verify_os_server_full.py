import base64
import concurrent.futures
import http.server
import json
import os
import re
import shutil
import sys
import tempfile
import threading
import time
import urllib.parse
from pathlib import Path
from dataclasses import dataclass

from os_test_client import (
    DEFAULT_BASE_URL,
    REQUEST_ERROR,
    OsTestClient,
    OsTestClientError,
    OsTestConfigError,
    RedactedArgumentParser,
    create_os_test_client,
    reconcile_os_test_client_args,
    resolve_base_url,
    resolve_token_file,
)

# Ambient env is read only inside main(), never at module import or --help.
BASE_URL = DEFAULT_BASE_URL
_CLIENT: OsTestClient | None = None
FINGERPRINT_COMMENT_RE = re.compile(r"<!--fp:([a-z0-9]{8}(?:[a-z0-9]{8})?)-->")


@dataclass
class TestResult:
    name: str
    status: str
    detail: str | None = None


TEST_RESULTS: list[TestResult] = []


def record_result(name: str, status: str, detail: str | None = None) -> None:
    TEST_RESULTS.append(TestResult(name=name, status=status, detail=detail))


def clear_results() -> None:
    TEST_RESULTS.clear()


def q(s: str) -> str:
    return urllib.parse.quote(s)


def extract_fingerprints_from_text(markdown_text: str) -> list[str]:
    """Extract unique fingerprints from markdown comment markers."""
    seen: set[str] = set()
    fingerprints: list[str] = []
    for fp in FINGERPRINT_COMMENT_RE.findall(markdown_text):
        if fp not in seen:
            seen.add(fp)
            fingerprints.append(fp)
    return fingerprints


def wait_until(
    condition,
    *,
    timeout_s: float = 10.0,
    interval_s: float = 0.2,
) -> bool:
    """Poll until condition returns truthy or timeout expires."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            if condition():
                return True
        except Exception:
            pass
        time.sleep(interval_s)
    return False

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


def make_request(
    path,
    method="GET",
    data=None,
    *,
    stream=False,
    timeout=60,
    retries=1,
    retry_interval=0.2,
):
    """Request through the shared authenticated client.

    Deterministic API responses (including 401 authentication failures) are
    returned once and never retried; only transport/redirect/origin failures
    retry. The client error text is bounded and carries no URL or secret.
    """
    client = _CLIENT
    if client is None:
        return None, None

    attempts = max(1, retries + 1)
    for attempt in range(attempts):
        try:
            return client.request(
                path,
                method=method,
                data=data,
                stream=stream,
                timeout=timeout,
            )
        except OsTestClientError:
            if attempt < attempts - 1:
                time.sleep(retry_interval)
                continue
            print(f"Failed to connect to OS API target: {REQUEST_ERROR}")
            return None, None

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
    skip_statuses=(),
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
        print("FAILED (Connection failed)")
        record_result(name, "FAILED", "connection failed")
        return None

    if status in tuple(skip_statuses):
        print(f"SKIPPED (Status: {status})")
        record_result(name, "SKIPPED", f"status {status}")
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
    predicate=None,
    expect_description=None,
    *,
    stream=False,
    skip_statuses=(),
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
        print("FAILED (Connection failed)")
        record_result(name, "FAILED", "connection failed")
        return None

    if status in tuple(skip_statuses):
        print(f"SKIPPED (Status: {status})")
        record_result(name, "SKIPPED", f"status {status}")
        return None

    if status in allowed:
        if predicate is not None:
            ok = False
            try:
                ok = bool(predicate(body))
            except Exception as e:  # noqa: BLE001
                print(f"FAILED (Predicate error: {e})")
                record_result(name, "FAILED", f"predicate error: {e}")
                return None
            if not ok:
                descr = expect_description or "predicate did not match"
                print(f"FAILED ({descr})")
                record_result(name, "FAILED", descr)
                return None
            suffix = expect_description or "predicate matched"
            print(f"OK ({suffix})")
            record_result(name, "OK", suffix)
        else:
            suffix = (
                "" if len(allowed) == 1 and status == allowed[0] else f"(status {status})"
            )
            print(f"OK {suffix}".strip())
            record_result(name, "OK", suffix or None)
        if extract_key and isinstance(body, dict):
            return body.get(extract_key)
        return body

    print(f"FAILED (Status: {status}, Expected: {allowed})")
    print(f"  Response: {body}")
    record_result(name, "FAILED", f"status {status}, expected {allowed}")
    return None


def print_summary() -> tuple[int, int, int, int]:
    if not TEST_RESULTS:
        print("Test summary: no tests were recorded.")
        return (0, 0, 0, 0)

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

    return total, passed, failed, skipped


def resolve_fingerprint(prefix: str, instance_id: str, fingerprint: str):
    path = f"{prefix}/instances/{instance_id}/resolveFingerprint?fingerprint={q(fingerprint)}"
    return make_request(path, timeout=8)


def find_resolvable_fingerprint(
    prefix: str,
    instance_id: str,
    markdown_text: str,
    *,
    selector_hint: str | None = None,
) -> tuple[str | None, str | None]:
    fingerprints = extract_fingerprints_from_text(markdown_text)
    first_match: tuple[str | None, str | None] = (None, None)
    for fp in fingerprints[:120]:
        status, body = resolve_fingerprint(prefix, instance_id, fp)
        if status != 200 or not isinstance(body, dict):
            continue
        selector = body.get("selector")
        if not isinstance(selector, str) or not selector:
            continue
        if first_match[0] is None:
            first_match = (fp, selector)
        if selector_hint and selector_hint in selector:
            return fp, selector
    return first_match


def find_nonexistent_fingerprint(prefix: str, instance_id: str) -> str | None:
    fixed_candidates = [
        "deadbeef",
        "deadbeefdeadbeef",
        "cafebabe",
        "cafebabecafebabe",
        "aaaaaaaa",
        "aaaaaaaaaaaaaaaa",
    ]
    for candidate in fixed_candidates:
        status, _ = resolve_fingerprint(prefix, instance_id, candidate)
        if status == 404:
            return candidate

    deterministic_candidates = [f"{i:08x}" for i in range(1, 65)]
    for candidate in deterministic_candidates:
        status, _ = resolve_fingerprint(prefix, instance_id, candidate)
        if status == 404:
            return candidate
    return None


def test_wait_contract(prefix: str, instance_id: str):
    print("  [Wait Contract]")
    base = f"{prefix}/instances/{instance_id}"

    run_test(
        "WaitForElement returns {ok,found}=true when element exists",
        f"{base}/waitForElement",
        method="POST",
        data={"selector": "#title", "timeout": 2000},
        predicate=lambda body: isinstance(body, dict)
        and body.get("ok") is True
        and body.get("found") is True
        and body.get("ok") == body.get("found"),
        expect_description="ok=true and found=true",
    )

    run_test(
        "WaitForElement timeout returns {ok,found}=false",
        f"{base}/waitForElement",
        method="POST",
        data={"selector": "#does-not-exist", "timeout": 100},
        predicate=lambda body: isinstance(body, dict)
        and body.get("ok") is False
        and body.get("found") is False
        and body.get("ok") == body.get("found"),
        expect_description="ok=false and found=false",
    )


def test_negative_validation_matrix(prefix: str, instance_id: str):
    print("  [Negative Validation]")
    base = f"{prefix}/instances/{instance_id}"

    run_test(
        "Click missing selector/fingerprint -> 400",
        f"{base}/click",
        method="POST",
        data={},
        expected_status=400,
    )
    run_test(
        "Click invalid fingerprint format -> 400",
        f"{base}/click",
        method="POST",
        data={"fingerprint": "bad!"},
        expected_status=400,
    )
    run_test(
        "WaitForElement missing selector/fingerprint -> 400",
        f"{base}/waitForElement",
        method="POST",
        data={"timeout": 100},
        expected_status=400,
    )
    run_test(
        "Input missing value -> 400",
        f"{base}/input",
        method="POST",
        data={"selector": "#name"},
        expected_status=400,
    )
    run_test(
        "UploadFile missing filePath -> 400",
        f"{base}/uploadFile",
        method="POST",
        data={"selector": "#fileInput"},
        expected_status=400,
    )
    run_test(
        "PressKey missing key -> 400",
        f"{base}/pressKey",
        method="POST",
        data={},
        expected_status=400,
    )
    run_test(
        "SetCookie missing value -> 400",
        f"{base}/cookie",
        method="POST",
        data={"name": "missing-value"},
        expected_status=400,
    )


def test_fingerprint_selector_compatibility(
    prefix: str,
    instance_id: str,
    *,
    include_get_element: bool,
):
    print("  [Fingerprint / Selector Compatibility]")
    base = f"{prefix}/instances/{instance_id}"
    text_body = run_test(
        "Extract markdown for fingerprints",
        f"{base}/text",
    )
    if (
        not isinstance(text_body, dict)
        or not isinstance(text_body.get("text"), str)
        or not text_body.get("text")
    ):
        record_result(
            "Fingerprint extraction",
            "SKIPPED",
            "text response not available",
        )
        return

    markdown_text = text_body.get("text", "")
    if not isinstance(markdown_text, str) or not markdown_text:
        record_result("Fingerprint extraction", "SKIPPED", "empty text output")
        return

    fp_title, selector = find_resolvable_fingerprint(
        prefix,
        instance_id,
        markdown_text,
        selector_hint="#title",
    )
    if not fp_title:
        record_result(
            "Fingerprint extraction",
            "SKIPPED",
            "no resolvable fingerprint found",
        )
        return

    print(f"    Resolved fingerprint: {fp_title} -> {selector}")

    run_value_test(
        "Resolve fingerprint returns selector",
        f"{base}/resolveFingerprint?fingerprint={q(fp_title)}",
        key="selector",
        predicate=lambda v: isinstance(v, str) and len(v.strip()) > 0,
        expect_description="non-empty selector",
    )

    if include_get_element:
        run_value_test(
            "Get Element via fingerprint",
            f"{base}/element?fingerprint={q(fp_title)}",
            key="element",
            predicate=lambda v: isinstance(v, str) and "<" in v,
            expect_description="returns element HTML",
        )

    run_test(
        "Click via fingerprint",
        f"{base}/click",
        method="POST",
        data={"fingerprint": fp_title},
        predicate=lambda body: isinstance(body, dict) and isinstance(body.get("ok"), bool),
        expect_description="response has boolean ok",
    )
    run_test(
        "Click with selector priority over invalid fingerprint",
        f"{base}/click",
        method="POST",
        data={"selector": "#link", "fingerprint": "bad!"},
        predicate=lambda body: isinstance(body, dict) and isinstance(body.get("ok"), bool),
        expect_description="selector path works even with bad fingerprint",
    )
    run_test(
        "Click with blank selector uses fingerprint fallback",
        f"{base}/click",
        method="POST",
        data={"selector": "   ", "fingerprint": fp_title},
        predicate=lambda body: isinstance(body, dict) and isinstance(body.get("ok"), bool),
        expect_description="blank selector falls back to fingerprint",
    )

    fp_name, _ = find_resolvable_fingerprint(
        prefix,
        instance_id,
        markdown_text,
        selector_hint="#name",
    )
    if fp_name:
        run_value_test(
            "Get Value via fingerprint",
            f"{base}/value?fingerprint={q(fp_name)}",
            key="value",
            predicate=lambda v: isinstance(v, str),
            expect_description="returns string value",
        )

    missing_fp = find_nonexistent_fingerprint(prefix, instance_id)
    if missing_fp:
        run_test(
            "Click with non-existent fingerprint -> 404",
            f"{base}/click",
            method="POST",
            data={"fingerprint": missing_fp},
            expected_status=404,
        )


def test_parallel_tab_stability(
    test_page_url: str,
    *,
    workers: int,
    loops: int,
):
    print("\n[Parallel Stability]")
    if workers <= 0 or loops <= 0:
        record_result("Parallel tab stability", "SKIPPED", "workers/loops set to 0")
        return

    def _worker(worker_idx: int) -> list[str]:
        errors: list[str] = []
        for loop_idx in range(loops):
            status, body = make_request(
                "/tabs/instances",
                method="POST",
                data={"url": test_page_url, "inBackground": True},
                timeout=20,
                retries=2,
            )
            if status != 200 or not isinstance(body, dict):
                errors.append(
                    f"worker={worker_idx} loop={loop_idx} create failed status={status}"
                )
                continue
            tab_id = body.get("instanceId")
            if not isinstance(tab_id, str) or not tab_id:
                errors.append(f"worker={worker_idx} loop={loop_idx} missing instanceId")
                continue
            try:
                s_wait, b_wait = make_request(
                    f"/tabs/instances/{tab_id}/waitForElement",
                    method="POST",
                    data={"selector": "#title", "timeout": 3000},
                    timeout=20,
                    retries=2,
                )
                if s_wait != 200 or not isinstance(b_wait, dict) or b_wait.get("ok") is not True:
                    errors.append(
                        f"worker={worker_idx} loop={loop_idx} wait failed status={s_wait} body={_short(b_wait)}"
                    )

                s_value, b_value = make_request(
                    f"/tabs/instances/{tab_id}/value?selector={q('#name')}",
                    timeout=20,
                    retries=2,
                )
                if s_value != 200 or not isinstance(b_value, dict) or "value" not in b_value:
                    errors.append(
                        f"worker={worker_idx} loop={loop_idx} value failed status={s_value} body={_short(b_value)}"
                    )
            finally:
                make_request(
                    f"/tabs/instances/{tab_id}",
                    method="DELETE",
                    timeout=20,
                    retries=1,
                )
        return errors

    all_errors: list[str] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(_worker, i) for i in range(workers)]
        for fut in concurrent.futures.as_completed(futures):
            all_errors.extend(fut.result())

    if all_errors:
        first = all_errors[0]
        record_result(
            "Parallel tab stability",
            "FAILED",
            f"{len(all_errors)} issues; first: {first}",
        )
        print(f"FAILED ({len(all_errors)} issues)")
    else:
        record_result(
            "Parallel tab stability",
            "OK",
            f"workers={workers}, loops={loops}",
        )
        print(f"OK (workers={workers}, loops={loops})")


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
    parsed_test_page = urllib.parse.urlparse(test_page_url)
    cookie_domain = parsed_test_page.hostname or "127.0.0.1"

    def wait_for_selector(selector: str, timeout_s: float = 8.0) -> bool:
        def _ready() -> bool:
            status, body = make_request(
                f"{prefix}/waitForElement",
                method="POST",
                data={"selector": selector, "timeout": 400},
                timeout=4,
                retries=1,
            )
            return (
                status == 200
                and isinstance(body, dict)
                and body.get("ok") is True
                and body.get("found") is True
            )

        return wait_until(_ready, timeout_s=timeout_s, interval_s=0.3)

    # Navigate to test page with rich elements
    run_test(
        "Navigate to test page",
        f"{prefix}/navigate",
        method="POST",
        data={"url": test_page_url},
    )
    if not wait_for_selector("#title", timeout_s=10):
        record_result(
            "Wait after navigate (#title)",
            "FAILED",
            "page did not become ready in time",
        )

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
        "Get Text (Markdown)",
        f"{prefix}/text",
        key="text",
        predicate=lambda v: isinstance(v, str) and len(v) > 0,
        expect_description="non-empty Markdown output",
    )

    test_wait_contract(base_path, instance_id)
    test_fingerprint_selector_compatibility(
        base_path,
        instance_id,
        include_get_element=include_get_element,
    )
    test_negative_validation_matrix(base_path, instance_id)

    run_value_test(
        "Wait For Element (#title)",
        f"{prefix}/waitForElement",
        method="POST",
        data={"selector": "#title", "timeout": 2000},
        key="ok",
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
    run_test(
        "Get Attribute (#agree checked)",
        f"{prefix}/attribute?selector={q('#agree')}&name=checked",
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
            skip_statuses=(501,),
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
        skip_statuses=(501,),
    )
    run_test("Accept Alert", f"{prefix}/acceptAlert", method="POST", skip_statuses=(501,))
    run_test("Dismiss Alert", f"{prefix}/dismissAlert", method="POST", skip_statuses=(501,))

    # Cookie operations on the local test host (no external dependency)
    run_test(
        "Navigate to test page (cookie host)",
        f"{prefix}/navigate",
        method="POST",
        data={"url": test_page_url},
    )
    if not wait_for_selector("#title", timeout_s=10):
        record_result(
            "Wait before cookie tests (#title)",
            "FAILED",
            "test page did not become ready in time",
        )
    run_test(
        "Wait For Network Idle (test page)",
        f"{prefix}/waitForNetworkIdle",
        method="POST",
        data={"timeout": 5000},
        skip_statuses=(501,),
    )

    # Press key on a remote page (avoid actor crash on file://)
    run_test(
        "Press Key (Enter)",
        f"{prefix}/pressKey",
        method="POST",
        data={"key": "Enter"},
        expected_status=200,
        skip_statuses=(501,),
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
            "domain": cookie_domain,
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

    if isinstance(workspaces, list) and workspaces:
        first_item = workspaces[0]
        if isinstance(first_item, dict):
            first_ws_id = first_item.get("id")
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
            data={"url": test_page_url},
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


def main(argv: list[str] | None = None) -> int:
    global BASE_URL, _CLIENT

    args_list = list(sys.argv[1:] if argv is None else argv)
    try:
        target_args = reconcile_os_test_client_args([], args_list)
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2

    parser = RedactedArgumentParser(description="Verify Floorp OS server APIs")
    parser.add_argument(
        "--base-url",
        default=None,
        help="OS server base URL (default: FLOORP_OS_BASE_URL or "
        + DEFAULT_BASE_URL
        + ")",
    )
    parser.add_argument(
        "--token-file",
        default=None,
        help="Explicit OS-Server bearer token file (default: FLOORP_OS_TOKEN_FILE)",
    )
    parser.add_argument("--quick", action="store_true", help="Run a quicker subset")
    parser.add_argument(
        "--skip-browser-info",
        action="store_true",
        help="Skip /browser endpoints",
    )
    parser.add_argument(
        "--skip-workspaces",
        action="store_true",
        help="Skip /workspaces endpoints",
    )
    parser.add_argument(
        "--skip-tabs",
        action="store_true",
        help="Skip tab manager tests",
    )
    parser.add_argument(
        "--skip-scraper",
        action="store_true",
        help="Skip scraper tests",
    )
    parser.add_argument(
        "--skip-concurrency",
        action="store_true",
        help="Skip parallel stability tests",
    )
    parser.add_argument(
        "--concurrency-workers",
        type=int,
        default=3,
        help="Number of workers for parallel stability test",
    )
    parser.add_argument(
        "--concurrency-loops",
        type=int,
        default=2,
        help="Loops per worker in parallel stability test",
    )
    try:
        args = parser.parse_args(target_args.verifier_args)
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2

    # Ambient env is read only after --help has exited, never at module import.
    base_url = resolve_base_url(
        target_args.base_url_flag,
        os.environ.get("FLOORP_OS_BASE_URL"),
    )
    try:
        token_file = resolve_token_file(
            target_args.token_file_flag,
            os.environ.get("FLOORP_OS_TOKEN_FILE"),
        )
        # Validated authenticated target precedes token-file IO and any fixture.
        _CLIENT = create_os_test_client(base_url, token_file)
    except OsTestConfigError as error:
        print(str(error), file=sys.stderr)
        return 2
    BASE_URL = _CLIENT.base_url
    clear_results()

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
            total, passed, failed, skipped = print_summary()
            return 1 if failed > 0 else 0

        if run_test("Health Check", "/health") is None:
            print("\nServer seems to be down or unreachable.")
            record_result("Health Check Reachability", "FAILED", "server unreachable")
            total, passed, failed, skipped = print_summary()
            return 1 if failed > 0 else 0

        try:
            if not args.skip_browser_info:
                test_browser_info()
            if not args.skip_workspaces:
                test_workspaces()
            if not args.skip_tabs:
                test_tab_manager(upload_file_path, test_page_url)
            if not args.skip_scraper:
                test_scraper(upload_file_path, test_page_url)

            if not args.skip_concurrency and not args.skip_tabs:
                workers = 2 if args.quick else args.concurrency_workers
                loops = 1 if args.quick else args.concurrency_loops
                test_parallel_tab_stability(
                    test_page_url,
                    workers=workers,
                    loops=loops,
                )
        finally:
            if httpd and tmpdir:
                stop_test_http_server(httpd, tmpdir)
    finally:
        try:
            os.remove(upload_file_path)
        except OSError:
            pass
        print("-" * 50)
        print("Verification complete.")
        total, passed, failed, skipped = print_summary()

    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
