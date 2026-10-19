#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run selected genuine frozen automation packages in headed production Floorp."""
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
PAGE = (TOOLS / "fixtures/corpus-automation.html").read_bytes()
AUTOMATION_IDS = (
    "gcbalfbdmfieckjlnblleoemohcganoc",
    "infppggnoaenmfagbfknfkancpbljcca",
    "hfjnppljknigdnnpocjjgdcfmnodoafe",
    "jnhgnonknehpejjnehehllkliplmbmhn",
)


class Fixture(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)

    def log_message(self, *args):
        pass


def selected_ids(value):
    ids = value.split(",")
    if not all(ids) or len(ids) != len(set(ids)):
        raise argparse.ArgumentTypeError("Select a nonempty list of distinct automation extension IDs")
    if any(extension_id not in AUTOMATION_IDS for extension_id in ids):
        raise argparse.ArgumentTypeError("Select only original frozen automation IDs: " + ",".join(AUTOMATION_IDS))
    return ids


def browser_command(browser, output, timeout, *, platform=None):
    """Use a headed desktop; Linux creates its own display and window manager."""
    command = [sys.executable, str(TOOLS / "run-browser-tests.py"),
               "--browser", str(browser), "--headed",
               "--test", str(TOOLS / "test-corpus-automation-floorp.mjs"),
               "--timeout", str(timeout), "--output", str(Path(output) / "browser"),
               "--actor-module", str(TOOLS / "CorpusAutomationChild.sys.mjs")]
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
    parser.add_argument("--ids", type=selected_ids, default=list(AUTOMATION_IDS),
                        help="Comma-separated original automation extension IDs; defaults to all four")
    parser.add_argument("--output", type=Path, default=ROOT / "_dist/chrome-runtime-tests",
                        help="Parent for a fresh corpus-automation-floorp-* evidence directory")
    parser.add_argument("--timeout", type=int, default=240)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="corpus-automation-floorp-", dir=args.output)).resolve()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    fixture = f"http://127.0.0.1:{server.server_address[1]}/automation"
    metadata = {"url": fixture, "sha256": hashlib.sha256(PAGE).hexdigest()}
    environment = {
        **os.environ,
        "FLOORP_CORPUS_ROOT": str(ROOT),
        "FLOORP_CORPUS_FIXTURE": fixture,
        "FLOORP_CORPUS_FIXTURE_METADATA": json.dumps(metadata),
        "FLOORP_CORPUS_OUTPUT": str(output),
        "FLOORP_CORPUS_IDS": ",".join(args.ids),
    }
    environment.pop("MOZ_HEADLESS", None)
    environment.pop("MOZ_DISABLE_CONTENT_SANDBOX", None)
    try:
        result = subprocess.run(browser_command(args.browser, output, args.timeout), env=environment)
        print("REPORT " + str(output / "report.json"))
        return result.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
