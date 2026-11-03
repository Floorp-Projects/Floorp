#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Measure authenticated browser TTS through real OS loopback audio."""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time
from types import ModuleType
from typing import Callable, cast

MAX_BYTES = 32 * 1024 * 1024
MAX_SECONDS = 300
SUITE = "browser-features/modules/modules/chrome-web-store/test/auxiliary/AuxiliaryNative.test.mts"
ROOT = Path(__file__).resolve().parents[2]


def load_module(name: str, path: Path) -> ModuleType:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Cannot load " + str(path))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


class PulseRecorder:
    def __init__(self, output: Path, env: dict[str, str], executable: str) -> None:
        self.output = output
        self.env = env
        self.executable = executable
        self.error: BaseException | None = None
        self.ready = threading.Event()
        self.process: subprocess.Popen[bytes] | None = None
        self.thread: threading.Thread | None = None
        self.log = (output / "recorder.log").open("wb")
        self.stopping = False
        self.sink = ""
        self.pactl = ""

    def start(self) -> None:
        pactl = shutil.which("pactl", path=self.env.get("PATH"))
        if not pactl:
            raise RuntimeError("Actual Pulse pactl is required")
        sink = subprocess.check_output([pactl, "get-default-sink"], env=self.env,
                                       text=True, timeout=10).strip()
        if sink not in ("floorp_ci", "floorp_test"):
            raise RuntimeError("A dedicated actual Floorp test output must already be selected")
        self.sink = sink
        self.pactl = pactl
        command = [self.executable, "--record", "--device=" + sink + ".monitor", "--raw",
                   "--format=s16le", "--rate=48000", "--channels=1",
                   "--latency-msec=10", "--process-time-msec=10"]
        self.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=self.log,
                                        env=self.env, bufsize=0)
        self.thread = threading.Thread(target=self._record, args=(sink, command), daemon=True)
        self.thread.start()
        if not self.ready.wait(10):
            raise RuntimeError("Actual Pulse monitor did not produce baseline samples")
        if self.error:
            raise self.error

    def _record(self, sink: str, command: list[str]) -> None:
        rate = 48000
        started = time.monotonic()
        capture_start = time.time() * 1000
        next_offset = 0
        clock_origin: float | None = None
        try:
            assert self.process is not None and self.process.stdout is not None
            with (self.output / "capture.pcm").open("wb") as raw, (self.output / "packets.ndjson").open("w", encoding="utf-8") as packets:
                pending = b""
                while True:
                    payload = self.process.stdout.read(4096)
                    if not payload:
                        break
                    received = time.time() * 1000
                    pending += payload
                    whole = len(pending) // 2 * 2
                    payload, pending = pending[:whole], pending[whole:]
                    if not payload:
                        continue
                    if next_offset + len(payload) > MAX_BYTES or time.monotonic() - started > MAX_SECONDS:
                        raise RuntimeError("Actual Pulse recording exceeded its bounds")
                    if abs((received - capture_start) / 1000 - (time.monotonic() - started)) > 0.1:
                        raise RuntimeError("Host wall clock changed during native speech capture")
                    if clock_origin is None:
                        clock_origin = received - len(payload) / 2 * 1000 / rate - 10
                    entry = {"byteOffset": next_offset, "frameCount": len(payload) // 2,
                             "firstSampleAtMs": clock_origin + next_offset / 2 * 1000 / rate,
                             "uncertaintyMs": 250, "silent": False,
                             "receivedAtMs": received}
                    raw.write(payload)
                    packets.write(json.dumps(entry) + "\n")
                    next_offset += len(payload)
                    if not self.ready.is_set() and next_offset >= rate * 2:
                        raw.flush()
                        packets.flush()
                        write_json(self.output / "ready.json", {
                            "backend": "real-Pulse-monitor-parec", "endpoint": sink + ".monitor",
                            "sampleRate": rate, "channels": 1, "sampleFormat": "s16le",
                            "nativeNoPacketsMeanRenderSilence": False,
                            "readyAtMs": received, "command": command,
                            "clock": "first received sample minus actual packet duration and requested10ms latency; continuous Pulse sample clock",
                        })
                        self.ready.set()
                if pending:
                    raise RuntimeError("Native Pulse PCM ended with an incomplete sample")
                if not self.stopping:
                    raise RuntimeError("Native Pulse recorder exited before its owned stop request")
        except BaseException as error:
            self.error = error
            if self.process and self.process.poll() is None:
                self.process.terminate()
        finally:
            self.ready.set()
            write_json(self.output / "done.json", {
                "passed": self.error is None, "lostPackets": self.error is not None,
                "deviceChanged": False, "captureStartedAtMs": clock_origin or capture_start,
                "captureEndedAtMs": time.time() * 1000,
                "error": str(self.error) if self.error else None,
            })

    def stop(self) -> None:
        self.stopping = True
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        if self.thread:
            self.thread.join(timeout=5)
            if self.thread.is_alive():
                raise RuntimeError("Owned native Pulse read thread did not stop")
        self.log.close()
        if self.error:
            raise self.error
        if self.sink:
            selected = subprocess.check_output([self.pactl, "get-default-sink"], env=self.env,
                                               text=True, timeout=10).strip()
            if selected != self.sink:
                done_path = self.output / "done.json"
                done = json.loads(done_path.read_text())
                done["deviceChanged"] = True
                done["passed"] = False
                write_json(done_path, done)
                raise RuntimeError("Actual default Pulse output changed during recording")


class NativeRecorder:
    def __init__(self, output: Path, env: dict[str, str], helper: Path, platform: str) -> None:
        self.output = output
        self.env = env
        self.helper = helper
        self.platform = platform
        self.process: subprocess.Popen[bytes] | None = None
        self.log = (output / "recorder.log").open("wb")

    def start(self) -> None:
        common = ["--raw", str(self.output / "capture.pcm"), "--packets", str(self.output / "packets.ndjson"),
                  "--ready", str(self.output / "ready.json"), "--done", str(self.output / "done.json"),
                  "--stop", str(self.output / "stop"), "--max-seconds", str(MAX_SECONDS), "--max-bytes", str(MAX_BYTES)]
        command = [str(self.helper), *common]
        if self.platform == "windows":
            shell = shutil.which("pwsh") or shutil.which("powershell")
            if not shell:
                raise RuntimeError("Actual Windows PowerShell is required for the native WASAPI helper")
            command = [shell, "-NoProfile", "-File", str(self.helper),
                       "-RawPath", str(self.output / "capture.pcm"),
                       "-PacketsPath", str(self.output / "packets.ndjson"),
                       "-ReadyPath", str(self.output / "ready.json"),
                       "-DonePath", str(self.output / "done.json"),
                       "-StopPath", str(self.output / "stop"),
                       "-MaxSeconds", str(MAX_SECONDS), "-MaxBytes", str(MAX_BYTES)]
        self.process = subprocess.Popen(command, stdout=self.log, stderr=subprocess.STDOUT, env=self.env)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError("Actual OS recorder exited before native readiness")
            if (self.output / "ready.json").is_file():
                time.sleep(0.75)
                return
            time.sleep(0.05)
        raise RuntimeError("Actual OS loopback recorder did not become ready")

    def stop(self) -> None:
        (self.output / "stop").touch()
        if self.process:
            try:
                code = self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=5)
                raise RuntimeError("Owned OS recorder did not acknowledge its stop request")
            if code:
                raise RuntimeError("Native OS recorder failed; inspect retained recorder.log")
        self.log.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", type=Path, default=ROOT)
    parser.add_argument("--browser", type=Path, required=True)
    parser.add_argument("--candidate-manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-recorder", type=Path)
    parser.add_argument("--parec", type=Path)
    args = parser.parse_args()
    root = args.repository.resolve(strict=True)
    browser = args.browser.resolve(strict=True)
    manifest = args.candidate_manifest.resolve(strict=True)
    identity = load_module("tts_candidate_identity", root / "tools/chrome-extension-runtime/candidate-identity.py")
    authenticate = cast(Callable[[Path, Path, Path], object], getattr(identity, "authenticate_candidate"))
    authenticate(manifest, browser, root)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    browser_output = output / "browser"
    browser_output.mkdir()
    env = dict(os.environ)
    env.pop("MOZ_DISABLE_CONTENT_SANDBOX", None)
    env.pop("MOZ_HEADLESS", None)
    env["FLOORP_REQUIRE_NATIVE_CHROME_AUXILIARY"] = "1"
    platform = {"linux": "linux", "darwin": "mac", "win32": "windows"}.get(sys.platform)
    if platform is None:
        raise RuntimeError("An actual supported operating system is required")
    if platform == "linux":
        parec = str(args.parec.resolve(strict=True)) if args.parec else shutil.which("parec")
        if not parec:
            raise RuntimeError("Existing real Pulse parec is required")
        recorder: PulseRecorder | NativeRecorder = PulseRecorder(output, env, parec)
    else:
        if args.native_recorder is None:
            raise RuntimeError("An actual OS-native recorder helper is required; no recording dependency is assumed")
        recorder = NativeRecorder(output, env, args.native_recorder.resolve(strict=True), platform)
    command = [sys.executable, str(root / "tools/chrome-extension-runtime/run-browser-tests.py"),
               "--browser", str(browser), "--candidate-manifest", str(manifest), "--output", str(browser_output),
               "--timeout", "240", "--test", str(root / SUITE)]
    write_json(output / "invocation.json", {"browserCommand": command, "platform": platform,
               "source": "ONLY authenticated browser AuxiliaryNative chrome.tts", "maxBytes": MAX_BYTES, "maxSeconds": MAX_SECONDS})
    try:
        try:
            recorder.start()
            subprocess.run(command, cwd=root, env=env, check=True, timeout=280)
        finally:
            recorder.stop()
        analyzer = load_module("tts_waveform_analyzer", Path(__file__).with_name("tts-waveform-analyzer.py"))
        analyze = cast(Callable[[Path, Path], dict[str, object]], getattr(analyzer, "analyze"))
        result = analyze(output, browser_output)
        print(json.dumps({"passed": result["passed"], "report": str(output / "waveform-report.json")}))
        return 0
    except BaseException as error:
        write_json(output / "failure.json", {"passed": False, "error": str(error), "partialRecording": str(output / "capture.pcm")})
        raise


if __name__ == "__main__":
    raise SystemExit(main())
