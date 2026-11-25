import argparse
import http.server
import json
import os
import socketserver
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

from os_test_client import resolve_base_url

# Contract-focused test page:
# - multiple delayed resources
# - explicit DOM marker (#done) after async fetch chain completes
TEST_PAGE_HTML = """
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8" />
    <title>Network Idle Test</title>
    <link rel="stylesheet" href="/slow-css">
</head>
<body>
    <h1 id="title">Network Idle Test</h1>
    <img src="/slow-image">
    <div id="logs"></div>
    <script>
        Promise.all([
            fetch('/slow-resource-1'),
            fetch('/slow-resource-2')
        ]).then(() => {
            const done = document.createElement('p');
            done.id = 'done';
            done.textContent = 'done';
            document.body.appendChild(done);
        });
    </script>
</body>
</html>
"""


class SlowHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):  # noqa: A003
        # Suppress logging to keep output clean
        return

    def do_GET(self):
        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(TEST_PAGE_HTML.encode("utf-8"))
            return

        if self.path == "/slow-css":
            time.sleep(0.6)
            self.send_response(200)
            self.send_header("Content-type", "text/css")
            self.end_headers()
            self.wfile.write(b"body { color: #222; }")
            return

        if self.path == "/slow-image":
            time.sleep(0.9)
            self.send_response(200)
            self.send_header("Content-type", "image/png")
            self.end_headers()
            # Minimal 1x1 transparent PNG
            self.wfile.write(
                b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15\xc4\x89\x00\x00\x00\nIDATx\x9cc\x00\x01\x00\x00\x05\x00\x01\r\n\x2e\xae\x00\x00\x00\x00IEND\xaeB`\x82"
            )
            return

        if self.path == "/slow-resource-1":
            time.sleep(1.2)
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Slow resource 1")
            return

        if self.path == "/slow-resource-2":
            time.sleep(0.7)
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Slow resource 2")
            return

        self.send_error(404)


class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True

def make_request(base_url: str, path: str, method: str = "GET", data=None, timeout: int = 15):
    url = f"{base_url}{path}"
    req = urllib.request.Request(url, method=method)
    payload = None
    if data is not None:
        payload = json.dumps(data).encode("utf-8")
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, data=payload, timeout=timeout) as res:
            body = res.read().decode("utf-8")
            try:
                return res.status, json.loads(body)
            except json.JSONDecodeError:
                return res.status, body
    except urllib.error.HTTPError as e:
        text = e.read().decode("utf-8")
        try:
            return e.code, json.loads(text)
        except json.JSONDecodeError:
            return e.code, text
    except Exception as e:  # noqa: BLE001
        return None, str(e)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Rigorous network idle verification")
    parser.add_argument("--base-url")
    parser.add_argument("--port", type=int, default=0, help="Local test server port (0 = auto)")
    parser.add_argument("--timeout", type=int, default=10000, help="waitForNetworkIdle timeout(ms)")
    args = parser.parse_args(argv)
    base_url = resolve_base_url(
        args.base_url, os.environ.get("FLOORP_OS_BASE_URL")
    ).rstrip("/")

    failures = 0
    httpd = ThreadedTCPServer(("127.0.0.1", args.port), SlowHandler)
    port = int(httpd.server_address[1])
    server_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    server_thread.start()
    time.sleep(0.2)

    test_url = f"http://127.0.0.1:{port}/"
    print(f"Testing Network Idle with {test_url}")

    def assert_case(name: str, condition: bool, detail: str = ""):
        nonlocal failures
        if condition:
            print(f"[OK] {name}")
        else:
            failures += 1
            suffix = f" - {detail}" if detail else ""
            print(f"[FAIL] {name}{suffix}")

    try:
        print("Creating scraper instance...")
        status, body = make_request(base_url, "/scraper/instances", "POST")
        assert_case("Create instance", status == 200 and isinstance(body, dict) and "instanceId" in body, str(body))
        if not (status == 200 and isinstance(body, dict) and "instanceId" in body):
            return 1

        instance_id = body["instanceId"]
        print(f"Instance: {instance_id}")

        status, body = make_request(
            base_url,
            f"/scraper/instances/{instance_id}/navigate",
            "POST",
            {"url": test_url},
        )
        assert_case("Navigate", status == 200, str(body))
        if status != 200:
            return 1

        # ensure async fetch chain has a chance to complete and element appears
        status, body = make_request(
            base_url,
            f"/scraper/instances/{instance_id}/waitForElement",
            "POST",
            {"selector": "#done", "timeout": 10000},
            timeout=20,
        )
        assert_case(
            "waitForElement(#done) contract",
            status == 200 and isinstance(body, dict) and body.get("ok") is True and body.get("found") is True,
            str(body),
        )

        start = time.time()
        status, body = make_request(
            base_url,
            f"/scraper/instances/{instance_id}/waitForNetworkIdle",
            "POST",
            {"timeout": args.timeout},
            timeout=20,
        )
        first_duration = time.time() - start
        print(f"First waitForNetworkIdle duration: {first_duration:.2f}s")
        assert_case(
            "waitForNetworkIdle contract (first)",
            status == 200 and isinstance(body, dict) and isinstance(body.get("ok"), bool),
            str(body),
        )
        assert_case(
            "waitForNetworkIdle should succeed after done",
            isinstance(body, dict) and body.get("ok") is True,
            str(body),
        )

        start = time.time()
        status2, body2 = make_request(
            base_url,
            f"/scraper/instances/{instance_id}/waitForNetworkIdle",
            "POST",
            {"timeout": args.timeout},
            timeout=20,
        )
        second_duration = time.time() - start
        print(f"Second waitForNetworkIdle duration: {second_duration:.2f}s")
        assert_case(
            "waitForNetworkIdle contract (second)",
            status2 == 200 and isinstance(body2, dict) and isinstance(body2.get("ok"), bool),
            str(body2),
        )
        assert_case(
            "second wait should not be dramatically slower",
            second_duration <= max(first_duration + 6.0, args.timeout / 1000.0 + 2.0, 12.0),
            f"first={first_duration:.2f}s second={second_duration:.2f}s",
        )

        # Short-timeout call: only contract check (env-dependent true/false)
        status3, body3 = make_request(
            args.base_url,
            f"/scraper/instances/{instance_id}/waitForNetworkIdle",
            "POST",
            {"timeout": 10},
            timeout=20,
        )
        assert_case(
            "waitForNetworkIdle short-timeout contract",
            status3 == 200 and isinstance(body3, dict) and isinstance(body3.get("ok"), bool),
            str(body3),
        )

    finally:
        if "instance_id" in locals():
            print("Destroying instance...")
            make_request(base_url, f"/scraper/instances/{instance_id}", "DELETE")
        httpd.shutdown()
        httpd.server_close()

    print(f"Completed with {failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
