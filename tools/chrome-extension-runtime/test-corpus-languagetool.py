#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Qualify unchanged LanguageTool using authored text and its actual product UI."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import zipfile

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/chrome-extension-runtime"
ID = "oldceeleldhonbafppcapldpdifcinji"
PAGE = b'''<!doctype html><html lang="en-US"><meta charset="utf-8"><title>Authored grammar correction</title><style>body{font:20px sans-serif;margin:40px}textarea{font:24px sans-serif;width:700px;height:250px}</style><h1>Authored grammar correction</h1><label for="editor">Synthetic test sentence</label><textarea id="editor" lang="en-US" autofocus></textarea><p>No user-provided text is used.</p></html>'''

SETUP_PAGE = b'''<!doctype html><html lang="en-US"><meta charset="utf-8"><meta name="lt-webextension-install-page"><title>Authored localhost product setup host</title><body></body></html>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--browser", type=Path)
    args = parser.parse_args()
    output = Path(tempfile.mkdtemp(prefix="corpus-languagetool-floorp-" if args.browser else "corpus-languagetool-reference-", dir=ROOT / "_dist/chrome-runtime-tests"))
    record = next(item for item in json.loads((TOOLS / "corpus-lock.json").read_text())["extensions"] if item["id"] == ID)
    package = ROOT / "_dist/chrome-extension-corpus/packages" / (ID + ".crx")
    if hashlib.sha256(package.read_bytes()).hexdigest() != record["packageSha256"]:
        raise ValueError("Frozen signed package hash mismatch")
    source = ROOT / "_dist/chrome-extension-corpus/chrome-unpacked" / ID
    target = output / "chrome-package"
    shutil.copytree(source, target, ignore=shutil.ignore_patterns("_metadata"))
    scripts = []
    with zipfile.ZipFile(package) as archive:
        for name in sorted(archive.namelist()):
            if Path(name).suffix.lower() not in (".js", ".mjs", ".cjs"):
                continue
            original = archive.read(name)
            if (target / name).read_bytes() != original:
                raise ValueError("Signed extension script changed: " + name)
            scripts.append(name + ":" + hashlib.sha256(original).hexdigest())
    requests = []

    class Fixture(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            requests.append({"path": self.path, "method": "GET"})
            page = SETUP_PAGE if self.path == "/product-welcome" else PAGE
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(page)))
            self.end_headers()
            self.wfile.write(page)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    fixture = {"url": f"http://127.0.0.1:{server.server_address[1]}/", "sha256": hashlib.sha256(PAGE).hexdigest(), "text": "She go to school.", "serviceData": "authored synthetic sentence; actual product service if reachable", "sourceIntegrity": {"unchangedScriptCount": len(scripts), "unchangedScriptsSha256": hashlib.sha256("\n".join(scripts).encode()).hexdigest(), "excludedStoreMetadata": "_metadata only (reserved by Chrome loadUnpacked)"}}
    fixture["onboardingHost"] = {"url": f"http://localhost:{server.server_address[1]}/product-welcome", "sha256": hashlib.sha256(SETUP_PAGE).hexdigest(), "scope": "Authored HTML-only localhost setup document explicitly supported by the unchanged product; original bundled welcome template and native consent controls; no service or browser API replacement"}
    (output / "fixture.json").write_text(json.dumps(fixture, indent=2) + "\n")
    env = {**os.environ, "FLOORP_LANGUAGETOOL_OUTPUT": str(output), "FLOORP_CORPUS_ROOT": str(ROOT)}
    command = ["node", str(TOOLS / "test-corpus-languagetool.mjs")]
    if args.browser:
        command = ["python3", str(TOOLS / "run-browser-tests.py"), "--browser", str(args.browser), "--test", str(TOOLS / "test-corpus-languagetool-floorp.mjs"), "--actor-module", str(TOOLS / "CorpusLanguageToolChild.sys.mjs"), "--timeout", "180"]
    if args.headed:
        command.append("--headed")
    try:
        result = subprocess.run(command, env=env)
        print("REPORT " + str(output / "report.json"))
        return result.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        (output / "requests.json").write_text(json.dumps(requests, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
