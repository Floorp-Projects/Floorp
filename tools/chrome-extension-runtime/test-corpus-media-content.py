#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run frozen content packages against authored native media and TLS transport."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
import os
import re
from pathlib import Path
import ssl
import shutil
import subprocess
import tempfile
import threading
import zipfile

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/chrome-extension-runtime"
HOSTS = ["www.youtube.com", "www.youtube-nocookie.com", "returnyoutubedislikeapi.com", "sponsor.ajay.app"]


def run(command):
    subprocess.run(command, check=True, capture_output=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ids", default="ponfpcnoihfmfllpaingbgckeeldkhle")
    parser.add_argument("--headed", action="store_true")
    parser.add_argument("--browser", type=Path, help="Run the production Floorp converter instead of the reference")
    args = parser.parse_args()
    output_root = ROOT / "_dist/chrome-runtime-tests"
    output_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="corpus-media-floorp-" if args.browser else "corpus-media-reference-", dir=output_root))
    tls = output / "tls"
    tls.mkdir(mode=0o700)
    run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(tls / "ca-key.pem"), "-out", str(tls / "ca.pem"), "-days", "3", "-subj", "/CN=Floorp authored media fixture CA", "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign"])
    run(["openssl", "req", "-newkey", "rsa:2048", "-nodes", "-keyout", str(tls / "server-key.pem"), "-out", str(tls / "server.csr"), "-subj", "/CN=" + HOSTS[0]])
    extension = tls / "server.ext"
    extension.write_text("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=" + ",".join("DNS:" + host for host in HOSTS) + "\n")
    run(["openssl", "x509", "-req", "-in", str(tls / "server.csr"), "-CA", str(tls / "ca.pem"), "-CAkey", str(tls / "ca-key.pem"), "-CAcreateserial", "-out", str(tls / "server.pem"), "-days", "3", "-extfile", str(extension)])
    for key in tls.glob("*key.pem"):
        key.chmod(0o600)
    video = output / "native.webm"
    run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc=size=160x90:rate=10:duration=12", "-threads", "1", "-c:v", "libvpx", "-deadline", "realtime", "-cpu-used", "8", "-b:v", "60k", "-pix_fmt", "yuv420p", str(video)])
    reference = json.loads((TOOLS / "chrome-reference.json").read_text())
    chrome = ROOT / "_dist/chrome-reference/extracted/opt/google/chrome/chrome"
    if hashlib.sha256(chrome.read_bytes()).hexdigest() != reference["binary"]["sha256"]:
        raise ValueError("Frozen reference binary mismatch")
    spec = importlib.util.spec_from_file_location("private_browser_trust", TOOLS / "private-browser-trust.py")
    trust = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(trust)
    trust_record = {"privateProfile": True, "certificateValidationDisabled":False} if args.browser else trust.prepare_chrome_trust(output, tls / "ca.pem", chrome)
    page = (TOOLS / "fixtures/corpus-player.html").read_bytes()
    requests = []

    class Fixture(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            host = self.headers.get("Host", "").split(":")[0]
            request = {"host": host, "path": self.path, "method": "GET"}
            requests.append(request)
            status, extra_headers = 200, {}
            if self.path.startswith("/native.webm"):
                payload, content_type = video.read_bytes(), "video/webm"
                total = len(payload)
                extra_headers["Accept-Ranges"] = "bytes"
                requested = self.headers.get("Range")
                if requested:
                    request["range"] = requested
                    match = re.fullmatch(r"bytes=(\d*)-(\d*)", requested.strip())
                    start, end = 0, total - 1
                    if match and any(match.groups()):
                        if match[1]:
                            start = int(match[1])
                            end = min(int(match[2]), end) if match[2] else end
                        else:
                            suffix = int(match[2])
                            start = max(0, total - suffix) if suffix else total
                    else:
                        start = total
                    if start >= total or end < start:
                        status, payload = 416, b""
                        extra_headers["Content-Range"] = f"bytes */{total}"
                    else:
                        status, payload = 206, payload[start:end + 1]
                        extra_headers["Content-Range"] = f"bytes {start}-{end}/{total}"
            elif host == "returnyoutubedislikeapi.com":
                value = {"id": "FloorpTest1", "likes": 321, "dislikes": 123, "viewCount": 4567, "rating": 3.0, "deleted": False} if self.path.startswith("/votes") else {}
                payload, content_type = json.dumps(value).encode(), "application/json"
            elif host == "sponsor.ajay.app":
                # SHA-256("FloorpTest1")[:5] == "99296". Authored service
                # data travels through the unchanged product's real TLS request.
                value = [{"videoID": "FloorpTest1", "segments": [{
                    "segment": [2, 7], "UUID": "a" * 64,
                    "category": "sponsor", "actionType": "skip",
                    "videoDuration": 12, "locked": 0, "votes": 1,
                }]}] if self.path.startswith("/api/skipSegments/99296?") else []
                payload, content_type = json.dumps(value).encode(), "application/json"
            else:
                payload, content_type = page, "text/html; charset=utf-8"
            request["status"] = status
            self.send_response(status)
            for name, value in extra_headers.items():
                self.send_header(name, value)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)

    server = ThreadingHTTPServer(("127.0.0.1", 443), Fixture)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(tls / "server.pem", tls / "server-key.pem")
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    integrity = {}
    corpus = json.loads((TOOLS / "corpus-lock.json").read_text())
    for id in args.ids.split(","):
        record = next(item for item in corpus["extensions"] if item["id"] == id)
        package = ROOT / "_dist/chrome-extension-corpus/packages" / (id + ".crx")
        if hashlib.sha256(package.read_bytes()).hexdigest() != record["packageSha256"]:
            raise ValueError("Frozen signed package mismatch")
        source = ROOT / "_dist/chrome-extension-corpus/chrome-unpacked" / id
        target = output / "chrome-packages" / id
        shutil.copytree(source, target, ignore=shutil.ignore_patterns("_metadata"))
        scripts = []
        with zipfile.ZipFile(package) as archive:
            for name in sorted(archive.namelist()):
                if Path(name).suffix.lower() not in (".js", ".mjs", ".cjs"):
                    continue
                original = archive.read(name)
                unpacked = (target / name).read_bytes()
                if original != unpacked:
                    raise ValueError("Signed extension script changed: " + name)
                scripts.append(name + ":" + hashlib.sha256(original).hexdigest())
        integrity[id] = {"excludedStoreMetadata": "_metadata (Chrome loadUnpacked reserves this generated store directory)", "unchangedScriptCount": len(scripts), "unchangedScriptsSha256": hashlib.sha256("\n".join(scripts).encode()).hexdigest()}
    metadata = {"hosts": HOSTS, "fixtureSha256": hashlib.sha256(page).hexdigest(), "nativeVideoSha256": hashlib.sha256(video.read_bytes()).hexdigest(), "caSha256": hashlib.sha256((tls / "ca.pem").read_bytes()).hexdigest(), "trust": trust_record, "serviceData": "authored local fixture, not upstream service", "sourceIntegrity": integrity}
    (output / "fixture.json").write_text(json.dumps(metadata, indent=2) + "\n")
    try:
        command = ["node", str(TOOLS / "test-corpus-media-content.mjs"), "--ids", args.ids]
        if args.browser:
            command = ["python3", str(TOOLS / "run-browser-tests.py"), "--browser", str(args.browser), "--test", str(TOOLS / "test-corpus-media-content-floorp.mjs"), "--actor-module", str(TOOLS / "CorpusMediaChild.sys.mjs"), "--timeout", "180"]
        if args.headed:
            command.append("--headed")
        result = subprocess.run(command, env={**os.environ, "FLOORP_MEDIA_CORPUS_OUTPUT": str(output), "FLOORP_MEDIA_CHROME": trust_record.get("wrapper", ""), "FLOORP_CORPUS_ROOT":str(ROOT), "FLOORP_CORPUS_IDS": args.ids})
        report_path = output / "report.json"
        failed_transport = False
        if report_path.is_file():
            report = json.loads(report_path.read_text())
            report["nativeTransportRequests"] = requests
            for item in report.get("extensions", []):
                if item["name"] == "Return YouTube Dislike" and item["primaryOperationResult"] == "passed":
                    observed = any(request["host"] == "returnyoutubedislikeapi.com" and request["path"].startswith("/votes?videoId=FloorpTest1") for request in requests)
                    if not observed:
                        item["primaryOperationResult"] = "failed"
                        item["error"] = "Required native TLS metadata request was not observed"
                        item["nativeQualified"] = False
                        failed_transport = True
                    else:
                        item["checks"].append("Actual fixture server observed the native TLS metadata request")
            for item in report.get("extensions", []):
                if item["name"] == "SponsorBlock" and item["primaryOperationResult"] == "passed":
                    observed = any(request["host"] == "sponsor.ajay.app" and request["path"].startswith("/api/skipSegments/99296") for request in requests)
                    if not observed:
                        item["primaryOperationResult"] = "failed"
                        item["error"] = "Required native TLS sponsor segment request was not observed"
                        item["nativeQualified"] = False
                        failed_transport = True
                    else:
                        item["checks"].append("Actual fixture server observed the native TLS sponsor segment request")
            if args.browser:
                report["nativeQualified"] = bool(report.get("extensions")) and all(item.get("nativeQualified") is True for item in report["extensions"])
            report_path.write_text(json.dumps(report, indent=2) + "\n")
        print("REPORT " + str(report_path))
        return 1 if failed_transport else result.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        (output / "requests.json").write_text(json.dumps(requests, indent=2) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
