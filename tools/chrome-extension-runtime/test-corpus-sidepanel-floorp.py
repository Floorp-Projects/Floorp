#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run unchanged signed corpus panels through actual Floorp native action/UI."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
SENTINEL = "Floorp authored compatibility passage 73195: Arctic foxes cross the quiet bridge."


def page(path):
    return ("<!doctype html><title>Authored Floorp SidePanel page " + path +
            "</title><article><h1>Authored Floorp compatibility page</h1><p id=authored>" +
            SENTINEL + "</p><p>This page is controlled test content. A draft in the extension panel must survive a switch to the second page and back.</p></article>").encode()


class Fixture(BaseHTTPRequestHandler):
    def do_GET(self):
        body = page(self.path)
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True)
    parser.add_argument("--ids", default="")
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()
    output_root = ROOT / "_dist/chrome-runtime-tests"
    output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="corpus-sidepanel-floorp-", dir=output_root))
    server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    fixture = f"http://corpus.floorp.test:{server.server_address[1]}/"
    metadata = {"url": fixture, "firstPageSha256": hashlib.sha256(page("/")).hexdigest(),
                "secondPageSha256": hashlib.sha256(page("/second")).hexdigest(),
                "sentinelSha256": hashlib.sha256(SENTINEL.encode()).hexdigest()}
    corpus = json.loads((ROOT / "tools/chrome-extension-runtime/corpus-lock.json").read_text())
    metadata["pages"] = [{"id": entry["id"], "sha256": hashlib.sha256(page("/" + entry["id"] + "/")).hexdigest()} for entry in corpus["extensions"] if entry["area"] == "sidePanel"]
    try:
        result = subprocess.run([
            "python3", str(ROOT / "tools/chrome-extension-runtime/run-browser-tests.py"),
            "--browser", args.browser,
            "--test", str(ROOT / "tools/chrome-extension-runtime/test-corpus-sidepanel-floorp.mjs"),
            "--timeout", str(args.timeout),
            "--actor-module", str(ROOT / "tools/chrome-extension-runtime/CorpusSidePanelChild.sys.mjs"),
            "--actor-module", str(ROOT / "tools/chrome-extension-runtime/corpus-sidepanel-ui.sys.mjs"),
        ], env={**os.environ, "FLOORP_CORPUS_ROOT": str(ROOT), "FLOORP_CORPUS_FIXTURE": fixture,
                "FLOORP_CORPUS_FIXTURE_METADATA": json.dumps(metadata), "FLOORP_CORPUS_IDS": args.ids,
                "FLOORP_CORPUS_OUTPUT": str(output)})
        report_path = output / "report.json"
        if not report_path.is_file():
            print("FAILED: SidePanel primary report missing", flush=True)
            return 1
        report = json.loads(report_path.read_text())
        rows = report.get("extensions", [])
        failed = [row for row in rows if row.get("result") not in ("passed", "blocked-account")]
        blocked = [row for row in rows if row.get("result") == "blocked-account"]
        qualified = sum(row.get("nativeQualified") is True for row in rows)
        if result.returncode or not rows or failed or not report.get("completed"):
            print(f"FAILED: {len(failed)} primary rows failed; report {report_path}", flush=True)
            return 1
        if blocked:
            print(f"COMPLETED UNQUALIFIED: {len(blocked)} observed account gates; "
                  f"{qualified}/{len(rows)} native primary operations qualified; report {report_path}", flush=True)
        else:
            print(f"PRIMARY OPERATIONS PASSED: {len(rows)}; {qualified}/{len(rows)} native qualified; "
                  f"report {report_path}", flush=True)
        return 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
