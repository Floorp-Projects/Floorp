#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise installed public native messaging callers against the genuine vendor host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def media_info(path):
    value = json.loads(subprocess.check_output([
        "ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(path)
    ], text=True))
    codecs = {stream["codec_type"] for stream in value["streams"]}
    if not {"audio", "video"}.issubset(codecs) or float(value["format"]["duration"]) < 3.5:
        raise RuntimeError("Official host media qualification requires a complete authored video/audio fixture")
    return {"duration": value["format"]["duration"], "streams": [
        {"type": stream["codec_type"], "codec": stream["codec_name"]} for stream in value["streams"]
    ]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, type=Path)
    parser.add_argument("--media", required=True, type=Path)
    args = parser.parse_args()
    media = args.media.resolve(strict=True)
    expected = media_info(media)
    parent = ROOT / "_dist/chrome-runtime-tests"
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="vdhcoapp-api-floorp-", dir=parent))
    command = [sys.executable, str(ROOT / "tools/chrome-extension-runtime/run-browser-tests.py"),
               "--browser", str(args.browser.resolve(strict=True)), "--test",
               str(ROOT / "tools/chrome-extension-runtime/test-vdhcoapp-api-floorp.mjs"), "--timeout", "150"]
    result = subprocess.run(command, env={**os.environ, "FLOORP_COAPP_OUTPUT": str(output),
                                         "FLOORP_COAPP_MEDIA": str(media)}).returncode
    report_path = output / "report.json"
    if not report_path.is_file():
        raise RuntimeError("Actual installed-extension companion run produced no report")
    report = json.loads(report_path.read_text())
    report["browserRunnerExitCode"] = result
    if result:
        report["passed"] = False
    if not result and report.get("passed"):
        try:
            for key in ("savedFile", "workerSavedFile"):
                saved = Path(report[key]["path"])
                actual = media_info(saved)
                if hashlib.sha256(saved.read_bytes()).digest() != hashlib.sha256(media.read_bytes()).digest():
                    raise RuntimeError("Official host public API output differs from authored media")
                if actual != expected:
                    raise RuntimeError("Official host saved media streams differ from authored media")
                report[key].update(actual)
        except Exception as error:
            report["passed"] = False
            report["mediaValidationError"] = str(error)
            result = 1
    elif not report.get("passed"):
        result = 1
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print("REPORT " + str(report_path))
    return result


if __name__ == "__main__":
    raise SystemExit(main())
