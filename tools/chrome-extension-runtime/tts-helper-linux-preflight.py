#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Actual Pulse recorder preflight only; never browser/TTS qualification."""
from __future__ import annotations

import argparse
import ast
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from types import ModuleType
from typing import Callable, Protocol, cast


class Recorder(Protocol):
    def start(self) -> None: ...
    def stop(self) -> None: ...


class MeasuredLevel(Protocol):
    rms: float
    peak: float


def load(name: str, path: Path) -> ModuleType:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Cannot import actual scratch helper")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "linux":
        raise RuntimeError("The Pulse preflight requires actual Linux")
    tools = args.tools.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    hashes: dict[str, str] = {}
    for name in ("test-browser-tts-waveform.py", "tts-waveform-analyzer.py"):
        source = tools / name
        ast.parse(source.read_text())
        hashes[name] = hashlib.sha256(source.read_bytes()).hexdigest()
    wrapper = load("tts_preflight_wrapper", tools / "test-browser-tts-waveform.py")
    analyzer = load("tts_preflight_analyzer", tools / "tts-waveform-analyzer.py")
    parec = shutil.which("parec")
    pactl = shutil.which("pactl")
    if not parec or not pactl:
        raise RuntimeError("Actual existing Pulse tools must be configured")
    recorder_type = cast(Callable[[Path, dict[str, str], str], Recorder], getattr(wrapper, "PulseRecorder"))
    recorder = recorder_type(output, dict(os.environ), parec)
    try:
        recorder.start()
        time.sleep(1)
    finally:
        recorder.stop()
    read_object = cast(Callable[[Path], dict[str, object]], getattr(analyzer, "read_object"))
    number = cast(Callable[[object, str], float], getattr(analyzer, "number"))
    ready = read_object(output / "ready.json")
    done = read_object(output / "done.json")
    if ready.get("sampleRate") != 48000:
        raise RuntimeError("Actual Pulse sample rate does not match the configured preflight")
    start_ms = number(done.get("captureStartedAtMs"), "capture start")
    end_ms = number(done.get("captureEndedAtMs"), "capture end")
    raw = (output / "capture.pcm").read_bytes()
    read_packets = cast(Callable[[Path, int], object], getattr(analyzer, "read_packets"))
    packets = read_packets(output / "packets.ndjson", len(raw))
    measure = cast(Callable[[bytes, object, int, float, float], MeasuredLevel], getattr(analyzer, "level"))
    measured = measure(raw, packets, 48000, start_ms, end_ms)
    write_wav = cast(Callable[[Path, bytes, object, int, float, float], None], getattr(analyzer, "timeline_wav"))
    wav = output / "native-loopback-preflight.wav"
    write_wav(wav, raw, packets, 48000, start_ms, end_ms)
    result = {
        "passed": done["passed"] is True and bool(raw),
        "scope": "OS recorder preflight only", "nativeTtsQualified": False,
        "browserLaunched": False, "speechOrToneGenerated": False,
        "toolsSha256": hashes, "recorder": ready, "capture": done,
        "pcmBytes": len(raw), "rms": measured.rms, "peak": measured.peak,
        "waveform": str(wav), "waveformSha256": hashlib.sha256(wav.read_bytes()).hexdigest(),
        "parecVersion": subprocess.check_output([parec, "--version"], text=True, timeout=10),
        "pulseInfo": subprocess.check_output([pactl, "info"], text=True, timeout=10),
    }
    (output / "preflight.json").write_text(json.dumps(result, indent=2) + "\n")
    if not result["passed"]:
        raise RuntimeError("Actual native Pulse recorder preflight failed")
    print(json.dumps({"passed": True, "nativeTtsQualified": False, "report": str(output / "preflight.json")}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
