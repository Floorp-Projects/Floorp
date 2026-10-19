#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Drive unchanged signed network extensions through production conversion and UI."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
IDS = ",".join((
    "pfnededegaaopdmhkdmcofjmoldfiped", "gcknhkkoolaabfmlnjonogaaifnjlfnp",
    "omghfjlpggmjjaagoclmmobgdodcjboh", "hnmpcagpplmpfojmgmnngilcnanddlhb",
    "mdnleldcmiljblolnjhpnblkcekpdkpa", "djflhoibgkdhkhhcedjiklpkjnoahfmg",
    "ddkjiahejlhfcafbddmgiahcphecmpfh",
))


class Fixture(BaseHTTPRequestHandler):
    def do_GET(self):
        record = {"role": self.server.role, "url": self.path,
                  "headers": {key.lower(): value for key, value in self.headers.items()}}
        self.server.traffic.append(record)
        script = self.server.role == "origin" and self.path.startswith("/xpopup/xpopup.js")
        page = self.server.role == "origin" and (self.path == "/" or self.path.startswith("/page?"))
        if script:
            payload = b"window.__corpusAdLoaded=true;"
            kind = "application/javascript"
        elif page:
            payload = ("<!doctype html><title>Controlled network corpus</title>"
                       "<h1>Controlled authored network operation</h1>"
                       '<p id="ready">Local HTTP evidence</p>'
                       f'<script src="http://127.0.0.1:{self.server.server_address[1]}/xpopup/xpopup.js"></script>').encode()
            kind = "text/html"
        else:
            payload = json.dumps(record).encode()
            kind = "application/json"
        self.send_response(200)
        for key, value in (("Content-Type", kind), ("Access-Control-Allow-Origin", "*"),
                           ("Cache-Control", "no-store"), ("Content-Length", str(len(payload)))):
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True)
    parser.add_argument("--ids", default=IDS)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--candidate-manifest", type=Path, help="Authenticate the exact freshly compiled native candidate before qualification")
    args = parser.parse_args()
    output_root = ROOT / "_dist/chrome-runtime-tests"
    output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="corpus-network-floorp-", dir=output_root))
    traffic = []
    servers = []
    for role in ("origin", "proxy"):
        server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
        server.role = role
        server.traffic = traffic
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        servers.append((server, thread))
    fixture = f"http://127.0.0.1:{servers[0][0].server_address[1]}/"
    try:
        result = subprocess.run([
            "python3", str(ROOT / "tools/chrome-extension-runtime/run-browser-tests.py"),
            "--browser", args.browser, "--test",
            str(ROOT / "tools/chrome-extension-runtime/test-corpus-network-floorp.mjs"),
            "--timeout", str(args.timeout), "--actor-module", str(ROOT / "tools/chrome-extension-runtime/CorpusNetworkChild.sys.mjs"),
            *(["--headed"] if args.headed else []),
            *(["--candidate-manifest", str(args.candidate_manifest.resolve(strict=True))] if args.candidate_manifest else []),
        ], env={**os.environ, "FLOORP_CORPUS_ROOT": str(ROOT),
                "FLOORP_CORPUS_FIXTURE": fixture,
                "FLOORP_CORPUS_PROXY_PORT": str(servers[1][0].server_address[1]),
                "FLOORP_CORPUS_OUTPUT": str(output), "FLOORP_CORPUS_IDS": args.ids, "FLOORP_CORPUS_BROWSER_MODE": "headed" if args.headed else "headless"})
        return result.returncode
    finally:
        for server, thread in servers:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
        (output / "traffic.json").write_text(json.dumps(traffic, indent=2))
        print("REPORT " + str(output / "report.json"))


if __name__ == "__main__":
    raise SystemExit(main())
