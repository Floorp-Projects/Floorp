#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Fetch exact official framework builds for authored DevTools fixtures, without installing dependencies."""
import base64
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/chrome-extension-runtime"
VERSIONS = {"react": "19.2.6", "react-dom": "19.2.6", "scheduler": "0.27.0", "redux": "5.0.1", "vue": "3.5.44"}


def fetch(url):
    with urllib.request.urlopen(url, timeout=30) as response:
        return response.read()


def main():
    output = ROOT / "_dist/framework-reference"
    output.mkdir(parents=True, exist_ok=True)
    reference_path = TOOLS / "framework-reference.json"
    existing = json.loads(reference_path.read_text()) if reference_path.is_file() else None
    key_source = "https://registry.npmjs.org/-/npm/v1/keys"
    key_error = None
    try:
        keys = json.loads(fetch(key_source))["keys"]
    except (OSError, ValueError) as error:
        # Tarball integrity is independently checked below against vendor HTTPS
        # metadata and, where present, the existing project dependency lock.
        keys = []
        key_error = str(error)
    lock = json.loads((ROOT / "deno.lock").read_text()).get("npm", {})
    reference = {"registry": "https://registry.npmjs.org", "purpose": "Genuine authored application fixtures; no project dependency changes", "packages": {}}
    for name, version in VERSIONS.items():
        metadata_url = f"https://registry.npmjs.org/{name}/{version}"
        metadata = json.loads(fetch(metadata_url))
        dist = metadata["dist"]
        if existing and dist["integrity"] != existing["packages"][name]["integrity"]:
            raise ValueError("Frozen framework package changed: " + name)
        locked = next((entry for key, entry in lock.items() if key == f"{name}@{version}" or key.startswith(f"{name}@{version}_")), None)
        if locked and locked.get("integrity") != dist["integrity"]:
            raise ValueError("Project-locked framework integrity differs from official registry")
        archive_path = output / f"{name}-{version}.tgz"
        archive = archive_path.read_bytes() if archive_path.exists() else fetch(dist["tarball"])
        if "sha512-" + base64.b64encode(hashlib.sha512(archive).digest()).decode() != dist["integrity"]:
            raise ValueError("Official tarball integrity mismatch")
        archive_path.write_bytes(archive)
        verified = []
        for signature in dist.get("signatures", []):
            key = next((key for key in keys if key["keyid"] == signature["keyid"]), None)
            if key is None:
                continue
            key_der, key_pem, sig_path, payload = [output / (name + suffix) for suffix in ("-key.der", "-key.pem", "-signature.der", "-payload.txt")]
            key_der.write_bytes(base64.b64decode(key["key"]))
            subprocess.run(["openssl", "pkey", "-pubin", "-inform", "DER", "-in", str(key_der), "-out", str(key_pem)], check=True, capture_output=True)
            sig_path.write_bytes(base64.b64decode(signature["sig"]))
            payload.write_text(f"{name}@{version}:{dist['integrity']}")
            check = subprocess.run(["openssl", "dgst", "-sha256", "-verify", str(key_pem), "-signature", str(sig_path), str(payload)], capture_output=True)
            if check.returncode == 0:
                verified.append({"keyid": key["keyid"], "key": key["key"], "signature": signature["sig"]})
        if keys and dist.get("signatures") and not verified:
            raise ValueError("Available official registry keys did not verify " + name)
        directory = output / f"{name}-{version}"
        directory.mkdir(exist_ok=True)
        with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as package:
            package.extractall(directory, filter="data")
        reference["packages"][name] = {"version": version, "metadataSource": metadata_url, "tarball": dist["tarball"], "integrity": dist["integrity"], "sha256": hashlib.sha256(archive).hexdigest(), "registrySignatures": dist.get("signatures", []), "registryKeySource": key_source, "registryKeyFetchError": key_error, "verifiedRegistrySignatures": verified, "matchesProjectLock": bool(locked)}
    if existing:
        for name, package in reference["packages"].items():
            for field in ("version", "tarball", "integrity", "sha256"):
                if package[field] != existing["packages"][name][field]:
                    raise ValueError("Frozen framework metadata changed: " + name + " " + field)
    else:
        reference_path.write_text(json.dumps(reference, indent=2) + "\n")
    print(reference_path)


if __name__ == "__main__":
    main()
