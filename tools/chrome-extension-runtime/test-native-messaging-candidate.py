#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Gate an actual native candidate's public Native Messaging APIs on its host OS.

Uses an owned executable and real Chrome OS registration. The existing browser
runner authenticates every candidate package file and the running build before
executing the typed fixture. Raw developer builds require --allow-unqualified
and can never produce an authenticated candidate result.
"""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
ALLOWED_ID = "abcdefghijklmnopabcdefghijklmnop"
PLATFORMS = {"linux": "linux", "win32": "windows", "darwin": "mac"}


def file_record(path):
    return {"path": str(path), "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def make_owned_directories(directory, created):
    missing = []
    current = directory
    while not current.exists():
        missing.append(current)
        current = current.parent
    for item in reversed(missing):
        try:
            item.mkdir()
            created.append(item)
        except FileExistsError:
            if not item.is_dir():
                raise


class Registration:
    """Never overwrite an existing manifest or registry key, including defaults."""
    def __init__(self, platform, output, host_name, command):
        self.platform, self.output, self.host_name = platform, output, host_name
        self.command = command
        self.created_directories = []
        self.created_registry_keys = []
        self.manifest = None
        self.manifest_created = False
        self.registry = None
        self.host_registry_path = None
        self.registry_value_written = False
        self.cleanup_complete = False
        self.registration_record = {}
        self.environment = {}

    def register(self):
        if self.platform == "linux":
            config = self.output / "xdg-config"
            self.environment["XDG_CONFIG_HOME"] = str(config)
            directory = config / "google-chrome/NativeMessagingHosts"
        elif self.platform == "mac":
            directory = Path.home() / "Library/Application Support/Google/Chrome/NativeMessagingHosts"
        else:
            directory = self.output / "registration"
        make_owned_directories(directory, self.created_directories)
        self.manifest = directory / (self.host_name + ".json")
        value = {"name": self.host_name, "description": "Owned actual Floorp native candidate gate",
                 "type": "stdio", "path": str(self.command),
                 "allowed_origins": ["chrome-extension://" + ALLOWED_ID + "/"]}
        with self.manifest.open("x", encoding="utf-8", newline="\n") as stream:
            self.manifest_created = True
            json.dump(value, stream, indent=2)
            stream.write("\n")
        self.registration_record = {"manifest": file_record(self.manifest), "overwroteExistingState": False}
        if self.platform == "windows":
            import winreg
            self.registry = winreg
            view = winreg.KEY_WOW64_32KEY
            path = "Software\\Google\\Chrome\\NativeMessagingHosts\\" + self.host_name
            self.host_registry_path = path
            try:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_READ | view):
                    raise RuntimeError("Refusing to overwrite an existing native messaging registry key")
            except FileNotFoundError:
                pass
            # Record each new ancestor separately so cleanup preserves every
            # pre-existing key, value, type and subkey exactly as found.
            components = path.split("\\")
            for index in range(1, len(components) + 1):
                parent = "\\".join(components[:index])
                try:
                    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, parent, 0, winreg.KEY_READ | view):
                        pass
                except FileNotFoundError:
                    with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, parent, 0, winreg.KEY_READ | winreg.KEY_WRITE | view):
                        self.created_registry_keys.append(parent)
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_SET_VALUE | view) as key:
                winreg.SetValueEx(key, "", 0, winreg.REG_SZ, str(self.manifest))
                self.registry_value_written = True
            self.registration_record["registry"] = {"root": "HKEY_CURRENT_USER", "path": path, "view": 32,
                                                    "createdKey": True, "preservedExistingState": True}
        return self.manifest

    def cleanup(self):
        errors = []
        if self.registry:
            winreg = self.registry
            for path in reversed(self.created_registry_keys):
                try:
                    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as key:
                        children, values, _ = winreg.QueryInfoKey(key)
                    if path == self.host_registry_path and self.registry_value_written:
                        # Only this newly created unique host key has our value.
                        if children or values != 1:
                            raise RuntimeError("Owned registry key changed while gate was running")
                        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_QUERY_VALUE | winreg.KEY_WOW64_32KEY) as key:
                            value, value_type = winreg.QueryValueEx(key, "")
                        if value != str(self.manifest) or value_type != winreg.REG_SZ:
                            raise RuntimeError("Owned registry default value changed while gate was running")
                        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, path, 0, winreg.KEY_SET_VALUE | winreg.KEY_WOW64_32KEY) as key:
                            winreg.DeleteValue(key, "")
                    elif children or values:
                        # Another process may have started using a new ancestor.
                        continue
                    winreg.DeleteKeyEx(winreg.HKEY_CURRENT_USER, path, winreg.KEY_WOW64_32KEY, 0)
                except FileNotFoundError:
                    continue
                except Exception as error:
                    errors.append(str(error))
        if self.manifest_created:
            try:
                self.manifest.unlink()
            except FileNotFoundError:
                pass
            except Exception as error:
                errors.append(str(error))
        for directory in reversed(self.created_directories):
            try:
                directory.rmdir()
            except FileNotFoundError:
                pass
            except OSError as error:
                if error.errno not in (errno.ENOTEMPTY, errno.EEXIST):
                    errors.append(str(error))
        self.cleanup_complete = not errors
        if errors:
            raise RuntimeError("Owned native host registration cleanup failed: " + "; ".join(errors))


def host_launcher(output, platform):
    source = HERE / "native-messaging-candidate-host.py"
    script = output / source.name
    shutil.copyfile(source, script)
    command = output / ("native-host.bat" if platform == "windows" else "native-host")
    if platform == "windows":
        # cmd expands percent even inside quotes; fail rather than mis-execute.
        if any(any(character in value for character in '%"\r\n') for value in (sys.executable, str(script))):
            raise ValueError("Windows native host launcher paths cannot contain cmd metacharacters")
        command.write_text(f'@echo off\n"{sys.executable}" -u "{script}" %*\n', encoding="utf-8", newline="\r\n")
    else:
        command.write_text("#!/bin/sh\nexec " + shlex.quote(sys.executable) + " -u " + shlex.quote(str(script)) + ' "$@"\n',
                           encoding="utf-8", newline="\n")
        command.chmod(0o700)
    return command, [file_record(script), file_record(command)]


def verify_host_exit(traffic):
    if not traffic.is_file():
        raise RuntimeError("Actual native host did not retain any process evidence")
    events = [json.loads(line) for line in traffic.read_text(encoding="utf-8").splitlines() if line]
    starts = [event for event in events if event["event"] == "start"]
    exit_events = [event for event in events if event["event"] == "exit"]
    exits = {event["pid"] for event in exit_events if event.get("clean") is True}
    if len(exit_events) != 4 or any(event.get("clean") is not True for event in exit_events):
        raise RuntimeError("Owned native host reported an error instead of clean EOF")
    if len(starts) != 4 or any(event["pid"] not in exits for event in starts):
        raise RuntimeError("Owned native host processes did not all exit cleanly")
    if len({event["pid"] for event in starts}) != 4:
        raise RuntimeError("Expected four independently spawned actual host processes")
    return {"started": len(starts), "exited": len(exits), "traffic": file_record(traffic)}


def run_browser(arguments, environment, timeout):
    options = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP} if sys.platform == "win32" else {"start_new_session": True}
    process = subprocess.Popen(arguments, cwd=ROOT, env=environment, **options)
    try:
        return process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        if sys.platform == "win32":
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
        else:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        try:
            process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            if sys.platform != "win32":
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            process.kill()
            process.wait(timeout=8)
        raise RuntimeError("Owned native browser runner exceeded its deadline") from None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--candidate-manifest", type=Path,
                        default=Path(os.environ["FLOORP_TEST_CANDIDATE_MANIFEST"])
                        if os.environ.get("FLOORP_TEST_CANDIDATE_MANIFEST") else None)
    parser.add_argument("--allow-unqualified", action="store_true", help="Explicit developer-only raw browser run; never a candidate qualification")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    platform = PLATFORMS.get(sys.platform)
    if platform is None:
        parser.error("Native Messaging gate requires actual Linux, Windows or macOS")
    if not args.candidate_manifest and (not args.allow_unqualified or os.environ.get("CI")):
        parser.error("An authenticated native candidate manifest is required; local raw builds need --allow-unqualified")
    binary = args.browser.resolve(strict=True)
    manifest = args.candidate_manifest.resolve(strict=True) if args.candidate_manifest else None
    if args.output:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
    else:
        parent = ROOT / "_dist/chrome-runtime-tests"
        parent.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="native-messaging-candidate-", dir=parent))
    source_paths = [Path(__file__).resolve(), HERE / "native-messaging-candidate-host.py",
                    HERE / "NativeMessagingCandidate.test.mts", HERE / "native-messaging-candidate-types.ts",
                    HERE / "native-port-errors-probe.mjs", HERE / "native-port-errors-assertions.mts",
                    HERE / "native-port-errors-test-types.ts"]
    sources = [file_record(path) for path in source_paths]
    token = uuid.uuid4().hex
    host_name = "floorp_candidate_" + token
    command, deployed = host_launcher(output, platform)
    registration = Registration(platform, output, host_name, command)
    report = {"passed": False, "qualifiedNativeCandidate": False, "platform": platform,
              "token": token, "sources": sources, "deployedHost": deployed,
              "stage": "authored-actual-native-messaging-api-gate", "startedAt": time.time()}
    try:
        host_manifest = registration.register()
        config = {"hostName": host_name, "manifestPath": str(host_manifest), "hostPath": str(command),
                  "trafficPath": str(output / "host-traffic.jsonl"), "outputPath": str(output / "api-evidence.json"),
                  "token": token, "platform": platform}
        config_path = output / "configuration.json"
        config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        report["configuration"] = file_record(config_path)
        environment = {**os.environ, **registration.environment,
                       "FLOORP_NATIVE_MESSAGING_CANDIDATE_CONFIG": str(config_path)}
        environment.pop("MOZ_DISABLE_CONTENT_SANDBOX", None)
        arguments = [sys.executable, str(HERE / "run-browser-tests.py"), "--browser", str(binary),
                     "--test", str(HERE / "NativeMessagingCandidate.test.mts"),
                     "--output", str(output / "browser"), "--timeout", str(args.timeout)]
        if manifest:
            arguments.extend(["--candidate-manifest", str(manifest)])
        status = run_browser(arguments, environment, args.timeout + 120)
        report["browserExitCode"] = status
        browser_reports = list((output / "browser").glob("run-*/report.json"))
        if len(browser_reports) != 1:
            raise RuntimeError("Expected exactly one actual native browser report")
        browser_report = json.loads(browser_reports[0].read_text(encoding="utf-8"))
        report["browserReport"] = file_record(browser_reports[0])
        report["runId"] = browser_report["runId"]
        report["runtime"] = browser_report["runtime"]
        report["buildScope"] = browser_report["buildScope"]
        if manifest:
            if browser_report["buildScope"] != "authenticated-native-candidate" or not browser_report.get("candidate"):
                raise RuntimeError("Browser report lacks required authenticated candidate provenance")
            report["candidate"] = browser_report["candidate"]
        api_path = output / "api-evidence.json"
        api = json.loads(api_path.read_text(encoding="utf-8"))
        if not api.get("passed"):
            raise RuntimeError("Native public API gate failed: " + str(api.get("error")))
        if api.get("runId") != browser_report["runId"] or api.get("token") != token:
            raise RuntimeError("Native public API evidence does not match this exact browser execution")
        report["apiEvidence"] = file_record(api_path)
        report["hostProcesses"] = verify_host_exit(output / "host-traffic.jsonl")
        if sources != [file_record(path) for path in source_paths] or deployed != [file_record(Path(item["path"])) for item in deployed]:
            raise RuntimeError("Native host gate sources or executed host bytes changed")
        if status or browser_report.get("passed") is not True:
            raise RuntimeError("Actual Native Messaging browser suite failed")
        report["passed"] = True
        report["qualifiedNativeCandidate"] = manifest is not None
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["registration"] = registration.registration_record
        try:
            registration.cleanup()
        except Exception as error:
            report["cleanupError"] = str(error)
            report["passed"] = report["qualifiedNativeCandidate"] = False
        report["registrationCleanupComplete"] = registration.cleanup_complete
        report["finishedAt"] = time.time()
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "qualifiedNativeCandidate": report["qualifiedNativeCandidate"],
                      "report": str(output / "report.json"), "error": report.get("error"), "cleanupError": report.get("cleanupError")}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
