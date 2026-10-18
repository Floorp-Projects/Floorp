#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Create a separate macOS app and fresh profiles for live two-client acceptance.

Only copies the supplied app. It never reads an existing browser profile or
account, and refuses an existing destination. Production loader outputs and the
packed Runtime registration patch must already be available.
"""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import socket
import subprocess
import tempfile
import zipfile


def generated_entries(directory, archive_prefix):
    entries = {}
    for line in (directory / "jar.mn").read_text().splitlines():
        match = re.fullmatch(r"\s+(\S+) \(([^)]+)\)", line)
        if not match:
            continue
        destination, source = match.groups()
        source_path = directory / source
        if not source_path.is_file() or ".." in Path(source).parts:
            raise ValueError(f"Invalid generated entry: {source}")
        entries[archive_prefix + destination] = source_path.read_bytes()
    if not entries:
        raise ValueError(f"Missing production entries in {directory}")
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-app", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    repository = Path(__file__).resolve().parents[2]
    output = args.output_root.resolve()
    if output.exists():
        parser.error("Output must be a new directory; existing profiles are never reused")
    build = args.build_root.resolve()
    chrome = generated_entries(build / "bridge/loader-features/_dist", "chrome/noraneko/")
    modules = generated_entries(build / "bridge/loader-modules/_dist", "chrome/noraneko/")
    replacements = {**chrome, **modules}
    engine = "chrome/noraneko/nora-resource/modules/workspaces/FloorpWorkspacesSync.sys.mjs"
    if engine not in replacements:
        parser.error("Production build lacks the workspace sync engine")
    output.mkdir(parents=True, mode=0o700)
    app = output / "Floorp Workspace Sync QA.app"
    subprocess.run(["ditto", str(args.source_app.resolve()), str(app)], check=True)
    resources = app / "Contents/Resources"
    archive = resources / "omni.ja"
    service_path = "modules/services-sync/service.sys.mjs"
    with zipfile.ZipFile(archive) as original:
        with tempfile.TemporaryDirectory(prefix="registration-", dir=output) as temporary:
            root = Path(temporary)
            target = root / service_path
            target.parent.mkdir(parents=True)
            target.write_bytes(original.read(service_path))
            patch = repository / "tools/patches/modules-services-sync-service.sys.patch"
            reverse = subprocess.run(["git", "apply", "--reverse", "--check", str(patch)],
                                     cwd=root, capture_output=True)
            if reverse.returncode:
                subprocess.run(["git", "apply", "--check", str(patch)], cwd=root, check=True)
                subprocess.run(["git", "apply", str(patch)], cwd=root, check=True)
            subprocess.run(["git", "apply", "--reverse", "--check", str(patch)], cwd=root, check=True)
            replacements[service_path] = target.read_bytes()
        temporary_archive = resources / "omni.qa.tmp"
        with zipfile.ZipFile(temporary_archive, "w", compression=zipfile.ZIP_DEFLATED) as destination:
            for entry in original.infolist():
                if entry.filename.startswith(("chrome/noraneko/nora-content/", "chrome/noraneko/nora-resource/")):
                    continue
                destination.writestr(entry, replacements.pop(entry.filename, original.read(entry.filename)))
            for name, contents in replacements.items():
                destination.writestr(name, contents)
    temporary_archive.replace(archive)
    with zipfile.ZipFile(archive) as verified:
        assert verified.read(engine) == modules[engine]
        assert b"result.FloorpWorkspaces" in verified.read(service_path)
    plist_path = app / "Contents/Info.plist"
    info = plistlib.loads(plist_path.read_bytes())
    info["CFBundleName"] = "Floorp Workspace Sync QA"
    info["CFBundleDisplayName"] = "Floorp Workspace Sync QA"
    info["CFBundleIdentifier"] = "org.floorp.workspace-sync.qa"
    plist_path.write_bytes(plistlib.dumps(info))
    policies = resources / "distribution/policies.json"
    policies.parent.mkdir(exist_ok=True)
    policies.write_text(json.dumps({"policies": {"DisableAppUpdate": True}}, indent=2))
    # Developer-ID-only entitlements cannot be retained by a local ad-hoc QA
    # signature. Preserve the runtime capabilities needed by Gecko/JIT instead.
    entitlements = output / "qa-entitlements.plist"
    entitlements.write_bytes(plistlib.dumps({
        "com.apple.security.cs.allow-jit": True,
        "com.apple.security.cs.allow-dyld-environment-variables": True,
        "com.apple.security.cs.disable-library-validation": True,
        "com.apple.security.device.audio-input": True,
        "com.apple.security.device.camera": True,
        "com.apple.security.device.usb": True,
        "com.apple.security.personal-information.location": True,
    }))
    subprocess.run(["codesign", "--force", "--deep", "--sign", "-",
                    "--options", "runtime", "--entitlements", str(entitlements),
                    str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    clients = []
    for name in ["A", "B"]:
        profile = output / f"profile-{name.lower()}"
        profile.mkdir(mode=0o700)
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        prefs = {
            "floorp.workspaces.sync.acceptance-profile": name,
            "marionette.port": port,
            # Live FxA must use production networking. WebDriver defaults replace
            # the auth endpoint with a non-production dummy URI otherwise.
            "remote.prefs.recommended": False,
            "services.sync.registerEngines": "FloorpWorkspaces",
            "services.sync.engine.floorp-workspaces": True,
            "services.sync.engineStatusChanged.floorp-workspaces": True,
            "services.sync.client.name": f"Floorp Workspace Sync QA {name}",
            "browser.shell.checkDefaultBrowser": False,
            "floorp.browser.welcome.page.shown": True,
            "browser.startup.homepage_override.mstone": "ignore",
            "app.update.auto": False,
            "signon.rememberSignons": False,
        }
        (profile / "user.js").write_text("\n".join(
            f"user_pref({json.dumps(key)}, {json.dumps(value)});" for key, value in prefs.items()
        ))
        client_app = output / f"Floorp Sync QA {name}.app"
        subprocess.run(["ditto", str(app), str(client_app)], check=True)
        client_plist = client_app / "Contents/Info.plist"
        client_info = plistlib.loads(client_plist.read_bytes())
        client_info["CFBundleName"] = f"Floorp Sync QA {name}"
        client_info["CFBundleDisplayName"] = client_info["CFBundleName"]
        client_info["CFBundleIdentifier"] = f"org.floorp.workspace-sync.qa.{name.lower()}"
        client_plist.write_bytes(plistlib.dumps(client_info))
        subprocess.run(["codesign", "--force", "--sign", "-", "--options", "runtime",
                        "--entitlements", str(entitlements), str(client_app)], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(client_app)], check=True)
        clients.append({"name": name, "profile": str(profile), "marionettePort": port,
                        "app": str(client_app), "binary": str(client_app / "Contents/MacOS/floorp")})
    record = {
        "app": str(app), "binary": str(app / "Contents/MacOS/floorp"),
        "sourceApp": str(args.source_app.resolve()),
        "buildCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=build, text=True).strip(),
        "buildTree": subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=build, text=True).strip(),
        "engineSha256": hashlib.sha256(modules[engine]).hexdigest(),
        "archiveSha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
        "clients": clients,
        "scope": "Only FloorpWorkspaces and mandatory native clients engines are registered. Real Mozilla sign-in is required separately.",
    }
    (output / "setup.json").write_text(json.dumps(record, indent=2))
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
