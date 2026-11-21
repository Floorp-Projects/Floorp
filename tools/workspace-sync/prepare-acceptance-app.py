#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Copy a complete macOS/Linux app and create fresh live Sync QA profiles.

Existing destinations and browser profiles are never reused. This tool only
installs built resources and public CA certificates; it never signs in.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import socket
import subprocess
import sys

from acceptance_app_support import (
    app_layout, certificate_files, copy_app, generated_entries,
    install_profile_certificates, overlay_app, profile_preferences,
    proxy_preferences, validate_source_app, write_preferences,
)


def mac_identity(app, name, identifier):
    plist_path = app / "Contents/Info.plist"
    info = plistlib.loads(plist_path.read_bytes())
    info.update(CFBundleName=name, CFBundleDisplayName=name, CFBundleIdentifier=identifier)
    plist_path.write_bytes(plistlib.dumps(info))


def sign_mac(app, entitlements, deep=False):
    command = ["codesign", "--force"] + (["--deep"] if deep else [])
    subprocess.run(command + ["--sign", "-", "--options", "runtime", "--entitlements",
                              str(entitlements), str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-app", type=Path, required=True,
                        help="Complete .app bundle on macOS, extracted floorp directory on Linux")
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--source-ref", help="Exact source app release commit, to replace previously shipped native patches")
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--platform", choices=["macos", "linux"],
                        default="macos" if sys.platform == "darwin" else "linux")
    parser.add_argument("--proxy-url", default=os.environ.get("HTTPS_PROXY") or os.environ.get("HTTP_PROXY"),
                        help="HTTP proxy without credentials; defaults to inherited HTTPS/HTTP proxy")
    parser.add_argument("--ca-certificate", type=Path, action="append", default=[],
                        help="Public PEM CA to trust in fresh profiles; defaults to NODE_EXTRA_CA_CERTS")
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    source_ref = subprocess.check_output(
        ["git", "rev-parse", "--verify", "--end-of-options", f"{args.source_ref}^{{commit}}"],
        cwd=repository, text=True).strip() if args.source_ref else None
    output = args.output_root.resolve()
    if output.exists():
        parser.error("Output must be a new directory; existing profiles are never reused")
    source = args.source_app.resolve(strict=True)
    validate_source_app(source, args.platform)
    proxy = proxy_preferences(args.proxy_url)
    certificates = certificate_files(args.ca_certificate)
    build = args.build_root.resolve(strict=True)
    chrome = generated_entries(build / "bridge/loader-features/_dist", "chrome/noraneko/")
    modules = generated_entries(build / "bridge/loader-modules/_dist", "chrome/noraneko/")
    generated = {**chrome, **modules}
    # The workspace dialog is a separate page bundle. Include every page built
    # by the normal before-mach build so a current controller never loads stale
    # release UI (for example its pending-container disabled dropdown).
    for manifest in sorted((build / "browser-features").glob("pages-*/_dist/jar.mn")):
        pages = generated_entries(manifest.parent, "chrome/noraneko/")
        if generated.keys() & pages.keys():
            parser.error("Production manifests contain duplicate archive entries")
        generated.update(pages)
    if "chrome/noraneko/nora-content-modal-child/index.html" not in generated:
        parser.error("Production build lacks the workspace dialog page")
    engine = "chrome/noraneko/nora-resource/modules/workspaces/FloorpWorkspacesSync.sys.mjs"
    constants = "chrome/noraneko/nora-resource/modules/NoranekoConstants.sys.mjs"
    if engine not in generated:
        parser.error("Production build lacks the workspace sync engine")
    if constants not in modules or b"import.meta.env" in modules[constants]:
        parser.error("Production build lacks resolved metadata; run the normal before-mach build")
    # A failed preparation remains private. Never reuse a possible auth profile.
    os.umask(0o077)
    output.mkdir(parents=True, mode=0o700)
    app = output / ("Floorp Workspace Sync QA.app" if args.platform == "macos" else "floorp-qa-base")
    copy_app(source, app, args.platform)
    resources, binary = app_layout(app, args.platform)
    hashes = overlay_app(resources, repository, generated, output, source_ref)
    policies = resources / "distribution/policies.json"
    policies.parent.mkdir(exist_ok=True)
    policies.write_text(json.dumps({"policies": {"DisableAppUpdate": True}}, indent=2))
    entitlements = output / "qa-entitlements.plist"
    if args.platform == "macos":
        mac_identity(app, "Floorp Workspace Sync QA", "org.floorp.workspace-sync.qa")
        entitlements.write_bytes(plistlib.dumps({
            "com.apple.security.cs.allow-jit": True,
            "com.apple.security.cs.allow-dyld-environment-variables": True,
            "com.apple.security.cs.disable-library-validation": True,
            "com.apple.security.device.audio-input": True,
            "com.apple.security.device.camera": True,
            "com.apple.security.device.usb": True,
            "com.apple.security.personal-information.location": True,
        }))
        sign_mac(app, entitlements, deep=True)
    clients = []
    # Hold both sockets during allocation to guarantee distinct chosen ports.
    reservations = []
    try:
        for name in ["A", "B"]:
            profile = output / f"profile-{name.lower()}"
            profile.mkdir(mode=0o700)
            listener = socket.socket()
            listener.bind(("127.0.0.1", 0))
            reservations.append(listener)
            port = listener.getsockname()[1]
            write_preferences(profile, profile_preferences(name, port, proxy))
            ca_hashes = install_profile_certificates(profile, certificates)
            client_app = output / (f"Floorp Sync QA {name}.app" if args.platform == "macos"
                                   else f"floorp-qa-{name.lower()}")
            copy_app(app, client_app, args.platform)
            client_resources, client_binary = app_layout(client_app, args.platform)
            if args.platform == "macos":
                mac_identity(client_app, f"Floorp Sync QA {name}",
                             f"org.floorp.workspace-sync.qa.{name.lower()}")
                sign_mac(client_app, entitlements)
            else:
                (client_app / "workspace-sync-qa.json").write_text(json.dumps({
                    "schemaVersion": 1, "client": name,
                    "identity": f"org.floorp.workspace-sync.qa.{name.lower()}",
                }))
            clients.append({"name": name, "profile": str(profile), "marionettePort": port,
                            "app": str(client_app), "binary": str(client_binary),
                            "archiveSha256": hashlib.sha256((client_resources / "omni.ja").read_bytes()).hexdigest(),
                            "browserArchiveSha256": hashlib.sha256((client_resources / "browser/omni.ja").read_bytes()).hexdigest(),
                            "caCertificateSha256": ca_hashes})
    finally:
        for listener in reservations:
            listener.close()
    record = {
        "schemaVersion": 2, "platform": args.platform,
        "app": str(app), "binary": str(binary), "sourceApp": str(source),
        "sourceAppCommit": source_ref,
        "buildCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=build, text=True).strip(),
        "buildTree": subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=build, text=True).strip(),
        "buildWorkingTreeDirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=build)),
        "engineSha256": hashlib.sha256(modules[engine]).hexdigest(),
        "archiveSha256": hashlib.sha256((resources / "omni.ja").read_bytes()).hexdigest(),
        "browserArchiveSha256": hashlib.sha256((resources / "browser/omni.ja").read_bytes()).hexdigest(),
        "resourceSha256": hashes, "clients": clients,
        "proxyConfigured": bool(proxy),
        "scope": "Only FloorpWorkspaces and mandatory native clients engines are registered. Sign in separately.",
    }
    (output / "setup.json").write_text(json.dumps(record, indent=2))
    print(json.dumps({"setup": str(output / "setup.json"), "platform": args.platform,
                      "resourcesVerified": len(hashes), "proxyConfigured": bool(proxy),
                      "clients": [{"name": c["name"], "port": c["marionettePort"],
                                   "trustedCertificates": len(c["caCertificateSha256"])} for c in clients]}, indent=2))


if __name__ == "__main__":
    main()
