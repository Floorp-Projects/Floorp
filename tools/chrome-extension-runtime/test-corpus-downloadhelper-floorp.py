#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Discover authored media in unchanged VDH v10, then validate its real saved file."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/chrome-extension-runtime"
PAGE = (TOOLS / "fixtures/corpus-media.html").read_bytes()


class Fixture(BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.traffic.append({"url": self.path, "range": self.headers.get("Range")})
        media = self.path == "/authored-video.mp4"
        payload = self.server.media if media else PAGE
        match = re.fullmatch(r"bytes=(\d+)-(\d*)", self.headers.get("Range", "")) if media else None
        start = int(match[1]) if match else 0
        end = min(int(match[2]), len(payload) - 1) if match and match[2] else len(payload) - 1
        if start >= len(payload) or end < start:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{len(payload)}")
            self.end_headers()
            return
        self.send_response(206 if match else 200)
        self.send_header("Content-Type", "video/mp4" if media else "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(end - start + 1))
        if media:
            self.send_header("Accept-Ranges", "bytes")
        if match:
            self.send_header("Content-Range", f"bytes {start}-{end}/{len(payload)}")
        self.end_headers()
        self.wfile.write(payload[start:end + 1])

    def log_message(self, *args):
        pass


def decoded_frames(path):
    return subprocess.check_output(["ffmpeg", "-nostdin", "-v", "error", "-i", str(path),
                                    "-map", "0:v:0", "-frames:v", "3", "-f", "hash", "-hash", "sha256", "-"], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--timeout", default=180, type=int)
    args = parser.parse_args()
    output = Path(tempfile.mkdtemp(prefix="corpus-downloadhelper-floorp-", dir=ROOT / "_dist/chrome-runtime-tests"))
    media_path = output / "authored-video.mp4"
    subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=24", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000", "-t", "4", "-c:v", "libx264", "-preset", "ultrafast", "-crf", "18", "-threads", "1", "-pix_fmt", "yuv420p", "-c:a", "aac", "-movflags", "+faststart", str(media_path)], check=True)
    server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
    server.media, server.traffic = media_path.read_bytes(), []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    fixture = {"url": f"http://127.0.0.1:{server.server_port}/", "pageSha256": hashlib.sha256(PAGE).hexdigest(), "mediaSha256": hashlib.sha256(server.media).hexdigest(), "mediaBytes": len(server.media)}
    command = ["python3", str(TOOLS / "run-browser-tests.py"), "--browser", str(args.browser), "--test", str(TOOLS / "test-corpus-downloadhelper-floorp.mjs"), "--timeout", str(args.timeout)]
    for name in ("CorpusDownloadHelperChild.sys.mjs", "corpus-sidepanel-ui.sys.mjs"):
        command.extend(["--actor-module", str(TOOLS / name)])
    result = 1
    try:
        completed = subprocess.run(command, env={**os.environ, "FLOORP_CORPUS_ROOT": str(ROOT), "FLOORP_CORPUS_OUTPUT": str(output), "FLOORP_CORPUS_FIXTURE": fixture["url"], "FLOORP_CORPUS_FIXTURE_METADATA": json.dumps(fixture)})
        result = completed.returncode
        report_path = output / "report.json"
        if result == 0 and report_path.is_file():
            report = json.loads(report_path.read_text())
            item = report["extension"]
            try:
                saved = Path(item["savedFile"]["path"])
                probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(saved)], text=True))
                assert {"video", "audio"}.issubset({stream["codec_type"] for stream in probe["streams"]}), "Saved media is missing authored video or audio"
                assert float(probe["format"]["duration"]) > 3.5, "Saved authored video duration is truncated"
                actual, expected = decoded_frames(saved), decoded_frames(media_path)
                assert actual == expected, "Saved video's decoded frames differ from the authored fixture"
                item["savedFile"].update({"streams": [{"type": stream["codec_type"], "codec": stream["codec_name"]} for stream in probe["streams"]], "duration": probe["format"]["duration"], "decodedFirstThreeFramesSha256": actual})
                item["primaryOperationResult"] = "passed"
            except Exception as error:
                item["primaryOperationResult"], item["mediaValidationError"] = "failed", str(error)
                result = 1
            report_path.write_text(json.dumps(report, indent=2))
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        (output / "traffic.json").write_text(json.dumps(server.traffic, indent=2))
        print("REPORT " + str(output / "report.json"))
    return result


if __name__ == "__main__":
    raise SystemExit(main())
