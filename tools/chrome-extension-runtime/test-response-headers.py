#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run genuine DNR response-header matching against actual HTTP responses."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
from urllib.parse import parse_qs, urlsplit
ROOT = Path(__file__).resolve().parents[2]
TRAFFIC = []

class Fixture(BaseHTTPRequestHandler):
    def do_GET(self):
        TRAFFIC.append({"path": self.path, "headers": dict(self.headers)})
        headers = {"content-type": ["application/json"], **json.loads(parse_qs(urlsplit(self.path).query).get("h", ["{}"])[0])}
        parsed = urlsplit(self.path)
        if parsed.path == "/frame":
            params = parse_qs(parsed.query)
            headers["content-type"] = ["text/html"]
            if params.get("allow") == ["1"]:
                headers["x-allow"] = ["yes"]
            key = json.dumps(params.get("key", [""])[0])
            body = ("<script>fetch('/child').then(response=>parent.postMessage({key:" + key + ",allowed:response.ok},'*'),()=>parent.postMessage({key:" + key + ",allowed:false},'*'))</script>").encode()
        else:
            body = json.dumps({"path": self.path}).encode()
        self.send_response(200)
        for name, values in headers.items():
            for value in values:
                self.send_header(name, value)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True)
    args = parser.parse_args()
    output = Path(tempfile.mkdtemp(prefix="response-headers-native-", dir=ROOT / "_dist/chrome-runtime-tests"))
    server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        result = subprocess.run(["python3", str(ROOT / "tools/chrome-extension-runtime/run-browser-tests.py"), "--browser", args.browser, "--test", str(ROOT / "browser-features/modules/modules/chrome-web-store/test/DNRTransformer.test.mts"), "--test", str(ROOT / "tools/chrome-extension-runtime/ResponseHeadersNative.test.mjs"), "--timeout", "120"], env={**os.environ, "FLOORP_HEADERS_ROOT": str(ROOT), "FLOORP_HEADERS_OUTPUT": str(output), "FLOORP_HEADERS_ORIGIN": f"http://127.0.0.1:{server.server_port}/"})
        report_file = output / "report.json"
        if result.returncode == 0 and report_file.exists():
            report = json.loads(report_file.read_text())
            missing = [info["serverMustSee"] for info in report.get("observations", []) if info.get("serverMustSee") and not any(request["path"] == info["serverMustSee"] for request in TRAFFIC)]
            report["responseActionsReachedOrigin"] = not missing
            if missing:
                report["passed"] = False
                report["error"] = "Response-stage action ran before actual server response: " + str(missing)
            report_file.write_text(json.dumps(report, indent=2))
            return 1 if missing else 0
        return result.returncode
    finally:
        server.shutdown(); server.server_close(); thread.join(timeout=5)
        (output / "traffic.json").write_text(json.dumps(TRAFFIC, indent=2))
        print("REPORT " + str(output / "report.json"))

if __name__ == "__main__":
    raise SystemExit(main())
