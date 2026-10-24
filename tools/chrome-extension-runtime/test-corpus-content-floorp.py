#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run unchanged signed content extensions through the production Floorp converter."""

import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/chrome-extension-runtime"
PAGE = (TOOLS / "fixtures/corpus-content.html").read_bytes()
USER_SCRIPT_IDS = {"jinjaccalgkegednnccohejagnlnfdag", "dhdgffkkebhmkfjojejmpbldmpobfkfo"}
CONSENT_ACTOR = ROOT / "browser-features/modules/modules/chrome-web-store/test/helpers/UserScriptsConsentUIChild.sys.mjs"


class Fixture(BaseHTTPRequestHandler):
    page = PAGE

    def do_GET(self):
        page = PAGE if self.path == "/frame" else self.page
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(page)))
        self.end_headers()
        self.wfile.write(page)

    def log_message(self, *args):
        pass


def browser_command(browser, output, timeout, *, platform=None):
    """Use native headed UI; Linux always gets a fresh owned X server and WM."""
    command = [sys.executable, str(TOOLS / "run-browser-tests.py"),
               "--browser", str(browser), "--headed",
               "--test", str(TOOLS / "test-corpus-content-floorp.mjs"),
               "--timeout", str(timeout)]
    for path in (TOOLS / "CorpusContentChild.sys.mjs", TOOLS / "CorpusContentUIChild.sys.mjs", CONSENT_ACTOR):
        command.extend(["--actor-module", str(path)])
    if (sys.platform if platform is None else platform) == "linux":
        command = [
            "xvfb-run", "--auto-servernum", "--server-args=-screen 0 1600x1200x24",
            sys.executable, str(TOOLS / "ci/run-with-linux-window-manager.py"),
            "--output", str(Path(output) / "window-manager"), "--", *command,
        ]
    return command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True)
    parser.add_argument("--ids", default="eimadpbcbfnmbkopoojfekhnkhdbieeh")
    parser.add_argument("--timeout", type=int, default=240)
    args = parser.parse_args()
    if USER_SCRIPT_IDS.intersection(args.ids.split(",")):
        Fixture.page = PAGE + b'<iframe src="/frame" title="Authored matching frame"></iframe>'
    output_root = ROOT / "_dist/chrome-runtime-tests"
    output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(
        prefix="corpus-content-floorp-", dir=output_root))
    server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    fixture = f"http://127.0.0.1:{server.server_address[1]}/"
    metadata = {"url": fixture, "sha256": hashlib.sha256(Fixture.page).hexdigest()}
    command = browser_command(args.browser, output, args.timeout)
    environment = {
        **os.environ,
        "FLOORP_CORPUS_ROOT": str(ROOT),
        "FLOORP_CORPUS_FIXTURE": fixture,
        "FLOORP_CORPUS_FIXTURE_METADATA": json.dumps(metadata),
        "FLOORP_CORPUS_OUTPUT": str(output),
        "FLOORP_CORPUS_IDS": args.ids,
    }
    environment.pop("MOZ_HEADLESS", None)
    environment.pop("MOZ_DISABLE_CONTENT_SANDBOX", None)
    try:
        result = subprocess.run(command, env=environment)
        print("REPORT " + str(output / "report.json"))
        return result.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
