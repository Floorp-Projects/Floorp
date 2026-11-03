#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Download, verify, and locally extract the fixed official Chrome reference.

No package installation or dependency changes are performed. The package and
binary hashes are frozen test metadata, originally checked through Google's
signed apt Release/index chain. The mutable apt index is not used to choose a
new version during a reproducibility run.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "_dist/chrome-reference")
    args = parser.parse_args()
    baseline = json.loads((Path(__file__).parent / "chrome-reference.json").read_text())
    args.output.mkdir(parents=True, exist_ok=True)
    package = baseline["package"]
    filename = args.output / package["url"].rsplit("/", 1)[1]
    if not filename.exists():
        partial = filename.with_suffix(".partial")
        try:
            with urllib.request.urlopen(package["url"], timeout=45) as response, partial.open("wb") as target:
                while chunk := response.read(1024 * 1024):
                    target.write(chunk)
            if partial.stat().st_size != package["size"] or digest(partial) != package["sha256"]:
                raise RuntimeError("Official package does not match the fixed reference hash/size")
            partial.replace(filename)
        finally:
            partial.unlink(missing_ok=True)
    if filename.stat().st_size != package["size"] or digest(filename) != package["sha256"]:
        raise RuntimeError("Existing package does not match the fixed reference hash/size")
    extracted = args.output / "extracted"
    binary = extracted / baseline["binary"]["relativePath"]
    if not binary.exists() or digest(binary) != baseline["binary"]["sha256"]:
        subprocess.run(["dpkg-deb", "--extract", str(filename), str(extracted)], check=True)
    if digest(binary) != baseline["binary"]["sha256"]:
        raise RuntimeError("Extracted Chrome binary does not match the fixed reference hash")
    version = subprocess.check_output([str(binary), "--version"], text=True).strip()
    if version != "Google Chrome " + baseline["browserVersion"]:
        raise RuntimeError("Extracted Chrome version differs from the reference")
    report = {"verified": True, "version": version, "binary": str(binary.resolve()),
              "packageSha256": package["sha256"], "binarySha256": baseline["binary"]["sha256"],
              "checkedAtUTC": datetime.datetime.now(datetime.timezone.utc).isoformat()}
    (args.output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
