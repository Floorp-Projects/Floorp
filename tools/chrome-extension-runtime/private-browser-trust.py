#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Scope normal public roots and an optional authored CA to one Linux Chrome process."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess


def prepare_chrome_trust(output, ca, browser):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True, mode=0o700)
    original = Path.home() / ".pki/nssdb"
    if not original.is_dir():
        raise ValueError("The browser NSS mount point must already exist; host trust is never created or changed")
    nss = output / "private-chrome-nssdb"
    nss.mkdir(mode=0o700)
    subprocess.run(["certutil", "-N", "-d", "sql:" + str(nss), "--empty-password"], check=True, capture_output=True)
    read_certutil = ["bwrap", "--ro-bind", "/", "/", "--dev-bind", "/dev", "/dev", "--proc", "/proc", "--", "certutil"]
    listing = subprocess.check_output(read_certutil + ["-L", "-d", "sql:" + str(original)], text=True)
    retained = []
    for line in listing.splitlines():
        match = re.fullmatch(r"(.+?)\s+([A-Za-z,]+)", line.rstrip())
        if not match or not any(flag in match[2] for flag in "CT"):
            continue
        certificate = subprocess.check_output(read_certutil + ["-L", "-d", "sql:" + str(original), "-n", match[1], "-a"])
        subprocess.run(["certutil", "-A", "-d", "sql:" + str(nss), "-n", match[1], "-t", match[2]], input=certificate, check=True, capture_output=True)
        retained.append(match[1])
    if ca is not None:
        subprocess.run(["certutil", "-A", "-d", "sql:" + str(nss), "-n", "Floorp authored corpus CA", "-t", "C,,", "-i", str(ca.resolve())], check=True, capture_output=True)
    command = ["bwrap", "--ro-bind", "/", "/", "--bind", str(output), str(output), "--bind", str(nss), str(original), "--tmpfs", "/tmp", "--ro-bind", "/tmp/.X11-unix", "/tmp/.X11-unix", "--dev-bind", "/dev", "/dev", "--proc", "/proc", "--", str(browser.resolve(strict=True))]
    wrapper = output / "chrome-local-trust"
    wrapper.write_text("#!/bin/sh\nexec " + shlex.join(command) + ' "$@"\n')
    wrapper.chmod(0o700)
    return {"wrapper": str(wrapper), "publicRootsRetained": retained, "authoredCaImported": ca is not None, "hostTrustModified": False, "certificateValidationDisabled": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ca", type=Path, help="Optional private authored CA; omit to retain only normal public roots")
    parser.add_argument("--browser", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare_chrome_trust(args.output, args.ca, args.browser)))
