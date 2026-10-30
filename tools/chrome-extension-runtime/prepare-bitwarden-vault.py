#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Start a genuine pinned local Vaultwarden server for password-manager corpus tests."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import ssl
import subprocess
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
IMAGE = "vaultwarden/server@sha256:efb3cde962015fcc036b2ea625242248611943b27212ebf4392841fb30fad055"
BITWARDEN_ID = "nngceckbapebfimnlniiiahkandclblb"


def run(command):
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def prepare_chrome(output, ca):
    """Trust only in the child mount namespace; never copy client private keys."""
    chrome = ROOT / "_dist/chrome-reference/extracted/opt/google/chrome/chrome"
    baseline = json.loads((ROOT / "tools/chrome-extension-runtime/chrome-reference.json").read_text())
    if hashlib.sha256(chrome.read_bytes()).hexdigest() != baseline["binary"]["sha256"]:
        raise ValueError("Frozen Chrome binary mismatch")
    spec = importlib.util.spec_from_file_location("private_browser_trust", Path(__file__).with_name("private-browser-trust.py"))
    trust = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(trust)
    trust.prepare_chrome_trust(output, ca, chrome)
    corpus = json.loads((ROOT / "tools/chrome-extension-runtime/corpus-lock.json").read_text())
    record = next(entry for entry in corpus["extensions"] if entry["id"] == BITWARDEN_ID)
    package = ROOT / f"_dist/chrome-extension-corpus/packages/{BITWARDEN_ID}.crx"
    if hashlib.sha256(package.read_bytes()).hexdigest() != record["packageSha256"]:
        raise ValueError("Frozen Bitwarden package mismatch")
    source = ROOT / f"_dist/chrome-extension-corpus/chrome-unpacked/{BITWARDEN_ID}"
    target = output / "chrome-bitwarden-original"
    shutil.copytree(source, target, ignore=shutil.ignore_patterns("_metadata"))
    scripts = []
    for path in sorted(source.rglob("*")):
        if path.is_file() and path.suffix.lower() in (".js", ".mjs", ".cjs"):
            name = path.relative_to(source).as_posix()
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if hashlib.sha256((target / name).read_bytes()).hexdigest() != digest:
                raise ValueError("Original signed script changed: " + name)
            scripts.append(name + ":" + digest)
    integrity = {"packageSha256": record["packageSha256"], "unchangedScriptCount": len(scripts), "unchangedScriptsSha256": hashlib.sha256("\n".join(scripts).encode()).hexdigest(), "excludedStoreMetadata": "_metadata (Chrome loadUnpacked rejects reserved store-generated folder)", "scripts": scripts}
    (output / "source-integrity.json").write_text(json.dumps(integrity, indent=2) + "\n")
    return integrity


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--stop", type=Path, help="Stop only the server recorded by the given server.json")
    args = parser.parse_args()
    if args.stop:
        report = json.loads(args.stop.read_text())
        if not report["containerName"].startswith("floorp-corpus-vaultwarden-"):
            parser.error("This is not a corpus test container")
        run(["docker", "rm", "-f", report["containerName"]])
        return 0
    output = args.output or Path(tempfile.mkdtemp(prefix="vaultwarden-", dir=ROOT / "_dist/chrome-runtime-tests"))
    output = output.resolve()
    if (output / "server.json").exists():
        parser.error("This output already owns a test vault; choose a new output directory")
    output.mkdir(parents=True, exist_ok=True, mode=0o700)
    tls = output / "data/tls"
    tls.mkdir(parents=True, mode=0o700)
    run(["docker", "pull", IMAGE])
    run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(tls / "ca-key.pem"), "-out", str(tls / "ca.pem"), "-days", "3", "-subj", "/CN=Floorp ephemeral corpus localhost CA", "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign"])
    for name, host in [("localhost", "localhost"), ("authored", "corpus.floorp.test")]:
        run(["openssl", "req", "-newkey", "rsa:2048", "-nodes", "-keyout", str(tls / (name + "-key.pem")), "-out", str(tls / (name + ".csr")), "-subj", "/CN=" + host])
        extension = tls / (name + ".ext")
        extension.write_text("basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:" + host + "\n")
        run(["openssl", "x509", "-req", "-in", str(tls / (name + ".csr")), "-CA", str(tls / "ca.pem"), "-CAkey", str(tls / "ca-key.pem"), "-CAcreateserial", "-out", str(tls / (name + ".pem")), "-days", "3", "-extfile", str(extension)])
    for key in tls.glob("*key.pem"):
        key.chmod(0o600)
    integrity = prepare_chrome(output, tls / "ca.pem")
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
    name = "floorp-corpus-vaultwarden-" + secrets.token_hex(4)
    url = f"https://localhost:{port}"
    command = ["docker", "run", "-d", "--name", name, "--user", f"{os.getuid()}:{os.getgid()}", "--memory", "256m", "--cpus", "1", "--cap-drop", "ALL", "--security-opt", "no-new-privileges", "--read-only", "--tmpfs", "/tmp:rw,noexec,nosuid,nodev", "--publish", f"127.0.0.1:{port}:8443", "--mount", f"type=bind,src={output / 'data'},dst=/data"]
    for value in ["ROCKET_ADDRESS=0.0.0.0", "ROCKET_PORT=8443", 'ROCKET_TLS={certs="/data/tls/localhost.pem",key="/data/tls/localhost-key.pem"}', "DOMAIN=" + url, "SIGNUPS_ALLOWED=true", "SIGNUPS_VERIFY=false", "INVITATIONS_ALLOWED=false", "LOG_LEVEL=warn"]:
        command += ["--env", value]
    container = subprocess.check_output(command + [IMAGE], text=True).strip()
    credentials = {"email": "floorp-" + secrets.token_hex(5) + "@localhost.test", "masterPassword": "FloorpTest-" + secrets.token_urlsafe(24), "username": "floorp-authored-user", "password": "Floorp-authored-login-" + secrets.token_urlsafe(12)}
    secret = output / "private-credentials.json"
    secret.write_text(json.dumps(credentials))
    secret.chmod(0o600)
    report = {"image": IMAGE, "version": "1.37.4", "webVaultVersion": "2026.7.0", "sourceRevision": "7c188967e7a467b19b2757bc3326f9cf3ccae327", "containerName": name, "containerId": container, "url": url, "caSHA256": hashlib.sha256((tls / "ca.pem").read_bytes()).hexdigest(), "primaryOperation": "not-run", "output": str(output), "sourceIntegrity": integrity}
    context = ssl.create_default_context(cafile=str(tls / "ca.pem"))
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), urllib.request.HTTPSHandler(context=context))
    try:
        for attempt in range(100):
            try:
                with opener.open(url + "/api/config", timeout=2) as response:
                    config = json.load(response)
                break
            except OSError:
                if attempt == 99:
                    raise
                time.sleep(0.1)
        report["httpsVerifiedWithCA"] = True
        report["serverConfigVersion"] = config.get("version")
    except Exception:
        run(["docker", "rm", "-f", name])
        raise
    (output / "server.json").write_text(json.dumps(report, indent=2) + "\n")
    print(output / "server.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
