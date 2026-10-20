#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Authenticate, extract and exercise this run's freshly compiled Floorp package."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[3]
_selection_spec = importlib.util.spec_from_file_location(
    "candidate_package_selection", ROOT / "tools/chrome-extension-runtime/candidate-package-selection.py")
_selection = importlib.util.module_from_spec(_selection_spec)
_selection_spec.loader.exec_module(_selection)
_identity_spec = importlib.util.spec_from_file_location(
    "candidate_identity", ROOT / "tools/chrome-extension-runtime/candidate-identity.py")
_identity = importlib.util.module_from_spec(_identity_spec)
_identity_spec.loader.exec_module(_identity)
TESTS = "browser-features/modules/modules/chrome-web-store/test/"
CORE = [
    "CRXParser.test.mts", "CRXVerifier.test.mts", "CRXConverter.test.mts",
    "Constants.test.mts", "CodeValidator.test.mts", "ManifestTransformer.test.mts",
    "NativeBackgroundSelection.test.mts",
    "ChromeValidationNative.test.mts",
    "ChromeContentTimersNative.test.mts",
    "ChromeManagedStorageNative.test.mts",
    "ChromeScriptingPathsNative.test.mts",
    "ChromeDevtoolsSidebarHeightNative.test.mts",
    "ChromePublicExtensionURLNative.test.mts",
    "ChromeWorkerOwnURLImportsNative.test.mts",
    "ChromeOffscreenContextsNative.test.mts",
    "ChromeContentWorldsNative.test.mts",
    "ChromeCustomEventDetailNative.test.mts",
    "ChromeUserScriptsPermissionsNative.test.mts",
    "ChromeManagementIDsNative.test.mts",
    "DNRTransformer.test.mts", "ChromeVersionTarget.test.mts", "ChromeExtensionDownload.test.mts",
    "ChromeExtensionUpdater.test.mts", "ChromeExtensionUpdateScheduler.test.mts",
    "ChromeMessagingNative.test.mts", "ChromeIdentityNative.test.mts",
    "ChromeWebPageMessagingNative.test.mts", "ChromeExtensionURLNative.test.mts",
    "polyfills/documentId/DocumentIdPolyfill.test.mts", "polyfills/offscreen/OffscreenNative.test.mts",
    "workers/WorkerScriptCacheNative.test.mts",
    "workers/WorkerBackgroundScopeNative.test.mts",
    "workers/WorkerRootGCLifetimeNative.test.mts",
    "workers/WorkerMessagingNative.test.mts",
    "ChromeDebuggerNative.test.mts",
    "ChromeDebuggerPrincipalNative.test.mts", "ChromeAuxiliaryNative.test.mts",
    "auxiliary/ChromeTtsValidationNative.test.mts",
    "ChromeNotificationsCloseNative.test.mts",
    "ChromeDeclarativeContentNative.test.mts", "windows/WindowsNative.test.mts",
    "ChromePowerNative.test.mts", "polyfills/capture/FileSystemNative.test.mts",
    "polyfills/capture/FileSystemPrivateNative.test.mts",
    "polyfills/capture/CaptureNative.test.mts",
    "polyfills/capture/CaptureConsumerNative.test.mts", "polyfills/capture/CapturePermissionNative.test.mts",
]
MEDIA = ["polyfills/capture/" + name + ".test.mts" for name in (
    "CaptureAudioNative", "DesktopSourceNative", "DesktopCaptureNative")]
MEDIA.append("auxiliary/AuxiliaryNative.test.mts")
PERMISSIONS = [
    "ChromeUserScriptsConsentNative.test.mts",
    "workers/WorkerPermissionsNative.test.mts",
    "workers/WorkerMemberPermissionsNative.test.mts",
    "workers/WorkerNative.test.mts",
]
DESKTOP = [
    "polyfills/capture/DesktopScreenNative.test.mts",
    "windows/WindowUpdateNative.test.mts",
]


def run(args, *, cwd=ROOT):
    subprocess.run([str(value) for value in args], cwd=cwd, check=True)


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def unpack(package, destination):
    destination.mkdir(parents=True, exist_ok=False)
    if package.name.endswith(".zip"):
        with zipfile.ZipFile(package) as archive:
            for item in archive.infolist():
                path = Path(item.filename.replace("\\", "/"))
                if path.is_absolute() or ".." in path.parts or ":" in item.filename:
                    raise ValueError(f"Unsafe archive path: {item.filename}")
            archive.extractall(destination)
    elif package.name.endswith((".tar.xz", ".tar.gz")):
        with tarfile.open(package) as archive:
            archive.extractall(destination, filter="data")
    elif package.name.endswith(".dmg"):
        mount = destination / "mounted"
        mount.mkdir()
        run(["hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", mount, package])
        try:
            apps = list(mount.glob("*.app"))
            if len(apps) != 1:
                raise RuntimeError("Candidate DMG does not contain exactly one application")
            run(["ditto", apps[0], destination / apps[0].name])
        finally:
            run(["hdiutil", "detach", mount])
        mount.rmdir()
    else:
        raise ValueError(f"Unsupported native candidate archive: {package.name}")


def binary_in(directory, platform):
    if platform == "mac":
        apps = list(directory.glob("*.app"))
        if len(apps) != 1:
            raise RuntimeError("Expected one extracted macOS application")
        with (apps[0] / "Contents/Info.plist").open("rb") as stream:
            name = plistlib.load(stream)["CFBundleExecutable"]
        return apps[0] / "Contents/MacOS" / name
    names = ("floorp.exe", "firefox.exe") if platform == "windows" else ("floorp", "firefox")
    binaries = [path for name in names for path in directory.rglob(name)
                if path.is_file() and (path.parent / "application.ini").is_file()]
    if len(binaries) != 1:
        raise RuntimeError(f"Expected one extracted browser executable, found {binaries}")
    return binaries[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=["linux", "windows", "mac"], required=True)
    parser.add_argument("--artifact", type=Path, required=True)
    parser.add_argument("--group", choices=["core", "media", "permissions", "desktop", "tts"], default="core")
    args = parser.parse_args()
    expected_host = {"linux": "linux", "windows": "win32", "mac": "darwin"}[args.platform]
    if sys.platform != expected_host:
        raise RuntimeError("Candidate execution must use the actual native operating system")
    artifact = args.artifact.resolve(strict=True)
    manifests = list(artifact.rglob("candidate-manifest.json"))
    if len(manifests) != 1:
        raise RuntimeError("Expected one immutable native candidate manifest")
    manifest = json.loads(manifests[0].read_text())
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    lock = json.loads((ROOT / "floorp-runtime.lock.json").read_text())
    if (manifest.get("schemaVersion") != 1 or manifest.get("platform") != args.platform or
            manifest.get("sourceCommit") != lock["source"]["commit"] or
            manifest.get("floorpCommit") != commit or manifest.get("productionOverlay") is not True or
            manifest.get("nativeWebExtensionBindings") is not True):
        raise RuntimeError("Candidate identity does not match this exact source and production build")
    # Share the full common/OS inventory check with installed-app authentication.
    _identity.validate_maintained_patches(manifest, ROOT)
    expected = _selection.select_native_package(manifest["packages"], args.platform)
    if Path(expected["name"]).name != expected["name"]:
        raise ValueError("Candidate package name must be a single filename")
    matches = list(artifact.rglob(expected["name"]))
    if len(matches) != 1 or matches[0].is_symlink() or matches[0].stat().st_size != expected["bytes"] or sha256(matches[0]) != expected["sha256"]:
        raise RuntimeError("Native candidate package checksum or size mismatch")
    destination = ROOT / "_dist" / f"native-candidate-{args.platform}-{args.group}"
    if destination.exists():
        raise RuntimeError(f"Refusing to reuse a previous extracted candidate: {destination}")
    unpack(matches[0], destination)
    binary = binary_in(destination, args.platform)
    output = ROOT / "_dist/chrome-runtime-tests" / f"ci-{args.platform}-{args.group}"
    output.mkdir(parents=True, exist_ok=True)
    result = {"passed": False, "platform": args.platform, "group": args.group,
              "candidate": manifest, "packageSha256": expected["sha256"], "commands": [],
              "headed": args.group in ("permissions", "desktop")}
    suites = list({"core": CORE, "media": MEDIA, "permissions": PERMISSIONS, "desktop": DESKTOP, "tts": []}[args.group])
    if args.platform == "linux" and args.group == "media":
        # Exact fragment marks belong to the Linux SpeechDispatcher backend.
        # Auxiliary speech and native loopback tests still run on every OS.
        suites.append("auxiliary/SpeechFragmentsNative.test.mts")
    command = [sys.executable, "tools/chrome-extension-runtime/run-browser-tests.py", "--browser", binary,
               "--output", output, "--timeout", "300"]
    if args.group in ("permissions", "desktop"):
        command.append("--headed")
        if sys.platform == "linux":
            command = [
                "xvfb-run", "--auto-servernum", "--server-args=-screen 0 1600x1200x24",
                sys.executable, ROOT / "tools/chrome-extension-runtime/ci/run-with-linux-window-manager.py",
                "--output", output / "window-manager", "--", *command,
            ]
    if args.group == "core":
        command += ["--test", "tools/chrome-extension-runtime/ci/ProductionOverlay.test.mts"]
        command += ["--test", "tools/chrome-extension-runtime/WorkerCanvasNative.test.mts"]
        command += ["--test", "tools/chrome-extension-runtime/NativePortErrorsCandidate.test.mts"]
        command += ["--test", "tools/chrome-extension-runtime/NativeWebRequestBatch.test.mts"]
    if args.group == "permissions":
        command += ["--actor-module", TESTS + "helpers/UserScriptsConsentUIChild.sys.mjs"]
        command += ["--test", "tools/chrome-extension-runtime/ci/ProductionWorkspaces.test.mts"]
    for suite in suites:
        command += ["--test", TESTS + suite]
    commands = [command]
    if args.group == "tts":
        waveform = output / "waveform"
        command = [sys.executable, "tools/chrome-extension-runtime/test-browser-tts-waveform.py",
                   "--browser", binary, "--candidate-manifest", manifests[0], "--output", waveform]
        commands = []
        if args.platform == "mac":
            recorder = output / "tts-macos-recorder"
            commands.append([
                "xcrun", "clang", "-fobjc-arc", "-O2",
                ROOT / "tools/chrome-extension-runtime/tts-macos-recorder.m", "-o", recorder,
                "-framework", "Foundation", "-framework", "AVFoundation",
                "-framework", "CoreMedia", "-framework", "AudioToolbox", "-framework", "CoreAudio",
            ])
            command += ["--native-recorder", recorder]
        elif args.platform == "windows":
            command += ["--native-recorder", ROOT / "tools/chrome-extension-runtime/tts-windows-recorder.ps1"]
        commands.append(command)
    if args.group == "core":
        commands.extend([
            [sys.executable, "tools/chrome-extension-runtime/test-network.py", "--browser", binary],
            [sys.executable, "tools/chrome-extension-runtime/test-response-headers.py", "--browser", binary],
            [sys.executable, "tools/chrome-extension-runtime/test-proxy.py", "--browser", binary, "--native-bypass"],
            [sys.executable, "tools/chrome-extension-runtime/test-proxy-scopes.py", "--browser", binary],
            [sys.executable, "tools/chrome-extension-runtime/ci/test-proxy-native.py", "--browser", binary],
            [sys.executable, "tools/chrome-extension-runtime/test-native-messaging-candidate.py",
             "--browser", binary, "--candidate-manifest", manifests[0],
             "--output", output / "native-messaging"],
            [sys.executable, "tools/chrome-extension-runtime/test-native-port-state-candidate.py",
             "--browser", binary, "--candidate-manifest", manifests[0],
             "--output", output / "native-port-state"],
            [sys.executable, "tools/chrome-extension-runtime/test-native-message-json-candidate.py",
             "--browser", binary, "--candidate-manifest", manifests[0],
             "--output", output / "native-message-json"],
            [sys.executable, "tools/chrome-extension-runtime/test-native-response-race-candidate.py",
             "--browser", binary, "--candidate-manifest", manifests[0],
             "--output", output / "native-response-race"],
            [sys.executable, "tools/chrome-extension-runtime/test-quiet-native-port-lifetime.py",
             "--browser", binary, "--expected-build-id", manifest["buildId"],
             "--candidate-manifest", manifests[0], "--output", output / "quiet-native-lifetime"],
            [sys.executable, "tools/chrome-extension-runtime/test-side-panel.py", "--browser", binary, "--floorp-ui"],
            [sys.executable, "tools/chrome-extension-runtime/test-diagnostics-ui.py", "--browser", binary],
        ])
    for command in commands:
        command_env = os.environ.copy()
        command_env["FLOORP_TEST_CANDIDATE_MANIFEST"] = str(manifests[0])
        if args.group in ("permissions", "desktop"):
            # The explicit headed group must create a real window even if an
            # enclosing test runner normally requests Gecko's headless widget.
            command_env.pop("MOZ_HEADLESS", None)
        code = subprocess.run([str(value) for value in command], cwd=ROOT,
                              env=command_env).returncode
        result["commands"].append({"args": [str(value) for value in command], "exitCode": code})
    reports = list(output.glob("waveform/browser/run-*/report.json" if args.group == "tts" else "run-*/report.json"))
    if len(reports) != 1:
        raise RuntimeError("Expected one actual native browser suite report")
    report = json.loads(reports[0].read_text())
    observed = report.get("runtime", {})
    expected_os = {"linux": "Linux", "windows": "WINNT", "mac": "Darwin"}[args.platform]
    result["nativeRuntimeIdentityMatches"] = (
        observed.get("buildId") == manifest["buildId"] and observed.get("os") == expected_os and
        observed.get("workerWebIDLCompiled") is True and
        (observed.get("abi", "").startswith("aarch64-") if args.platform == "mac" else
         observed.get("abi", "").startswith("x86_64-")))
    result["passed"] = result["nativeRuntimeIdentityMatches"] and all(
        item["exitCode"] == 0 for item in result["commands"])
    (output / "native-result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"{'PASS' if result['passed'] else 'FAIL'} native {args.platform}/{args.group}: {output / 'native-result.json'}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
