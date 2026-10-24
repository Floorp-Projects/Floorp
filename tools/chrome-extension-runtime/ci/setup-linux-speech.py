#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Start genuine eSpeak NG synthesis and prove output through the actual Pulse sink."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import signal
import struct
import subprocess
import time
import wave

SAMPLE_RATE = 48000
BASELINE_SAMPLES = SAMPLE_RATE // 4


def write_report(output, report):
    (output / "backend.json").write_text(json.dumps(report, indent=2) + "\n")


def run_logged(command, output, name, env, timeout):
    with (output / (name + ".stdout.log")).open("wb") as stdout, \
            (output / (name + ".stderr.log")).open("wb") as stderr:
        subprocess.run(command, stdout=stdout, stderr=stderr, env=env,
                       check=True, timeout=timeout)
    return (output / (name + ".stdout.log")).read_text()


def wait_recorded_samples(recorder, raw, target, timeout=15):
    """Wait for actual captured frames, including Pulse's initial buffer latency."""
    deadline = time.monotonic() + timeout
    while raw.stat().st_size // 2 < target:
        if recorder.poll() is not None:
            raise RuntimeError(f"Pulse recorder exited before {target} samples: {recorder.returncode}")
        if time.monotonic() >= deadline:
            raise RuntimeError(f"Pulse recorder did not capture {target} samples before timeout")
        time.sleep(0.025)


def measure_waveform(output, report):
    raw = output / "synthesis.raw"
    if not raw.exists():
        return
    payload = raw.read_bytes()
    payload = payload[:len(payload) // 2 * 2]
    samples = struct.unpack("<" + "h" * (len(payload) // 2), payload)
    baseline = samples[:BASELINE_SAMPLES]
    peak = max((abs(sample) for sample in samples), default=0)
    rms = math.sqrt(sum(sample * sample for sample in samples) / max(1, len(samples))) / 32768
    baseline_rms = math.sqrt(sum(sample * sample for sample in baseline) / max(1, len(baseline))) / 32768
    waveform = output / "synthesis.wav"
    with wave.open(str(waveform), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(SAMPLE_RATE)
        stream.writeframes(payload)
    report.update({
        "waveform": str(waveform),
        "waveformSha256": hashlib.sha256(waveform.read_bytes()).hexdigest(),
        "rawSha256": hashlib.sha256(raw.read_bytes()).hexdigest(),
        "sampleRate": SAMPLE_RATE,
        "sampleCount": len(samples),
        "baselineSampleCount": len(baseline),
        "peak": peak,
        "rms": rms,
        "baselineRms": baseline_rms,
        "minimumPeak": 256,
        "minimumRms": max(0.002, baseline_rms * 5),
    })


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dispatcher", type=Path, default=Path(shutil.which("speech-dispatcher") or "/usr/bin/speech-dispatcher"))
    parser.add_argument("--engine", type=Path)
    parser.add_argument("--client", type=Path)
    parser.add_argument("--engine-config", type=Path, default=Path("/etc/speech-dispatcher/modules/espeak-ng.conf"))
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True, mode=0o700)
    args.launch_attempted = False
    report = {"passed": False, "stage": "configuration"}
    try:
        return prepare(args, report)
    except BaseException as error:
        report["error"] = str(error)
        measure_waveform(args.output, report)
        write_report(args.output, report)
        if args.launch_attempted:
            stop_owned_daemon(args.output)
        raise


def stop_owned_daemon(output):
    try:
        pid = int((output / "pid").read_text().strip())
        process = Path("/proc") / str(pid)
        command = (process / "cmdline").read_bytes().decode().split("\0")
        expected = str(output / "config")
        if process.stat().st_uid == os.getuid() and command[command.index("-C") + 1] == expected and command[command.index("-P") + 1] == str(output / "pid"):
            os.kill(pid, signal.SIGTERM)
    except (OSError, ValueError, IndexError):
        pass


def prepare(args, report):
    output = args.output
    if not args.engine:
        files = subprocess.check_output(["dpkg", "-L", "speech-dispatcher-espeak-ng"], text=True).splitlines()
        args.engine = next(Path(name) for name in files if name.endswith("/sd_espeak-ng"))
    engine = args.engine.resolve(strict=True)
    client = args.client or Path(shutil.which("spd-say") or args.dispatcher.parent / "spd-say")
    config, modules, engines, logs = (output / name for name in ("config", "config/modules", "engines", "logs"))
    for directory in (config, modules, engines, logs):
        directory.mkdir(parents=True, exist_ok=True)
    (engines / "sd_espeak-ng").symlink_to(engine)
    shutil.copyfile(args.engine_config, modules / "espeak-ng.conf")
    socket = output / "speechd.sock"
    if any('"' in str(path) or "\n" in str(path) for path in (logs, socket)):
        raise ValueError("Speech configuration path contains a quote or newline")
    (config / "speechd.conf").write_text(
        f'LogLevel 3\nLogDir "{logs}"\nCommunicationMethod "unix_socket"\n'
        f'SocketPath "{socket}"\nTimeout 0\nAudioOutputMethod "pulse"\n'
        'DefaultModule espeak-ng\nAddModule "espeak-ng" "sd_espeak-ng" "espeak-ng.conf"\n')
    env = {**os.environ, "SPEECHD_ADDRESS": "unix_socket:" + str(socket)}
    report.update({"engine": str(engine), "engineSha256": hashlib.sha256(engine.read_bytes()).hexdigest(),
                   "speechdAddress": env["SPEECHD_ADDRESS"], "stage": "dispatcher startup"})
    write_report(output, report)
    args.launch_attempted = True
    run_logged([str(args.dispatcher), "-d", "-t", "0", "-C", str(config), "-m", str(engines), "-P", str(output / "pid")], output, "dispatcher", env, 20)
    for count in range(100):
        if socket.exists():
            break
        time.sleep(0.1)
    else:
        raise RuntimeError("Genuine Speech Dispatcher socket did not become ready")
    report["stage"] = "voice discovery"
    voices = run_logged([str(client), "-o", "espeak-ng", "-L"], output, "voices", env, 20)
    (output / "voices.txt").write_text(voices)
    if "en" not in voices.lower():
        raise RuntimeError("The genuine eSpeak NG module exposes no English synthesis voice")
    sink = run_logged(["pactl", "get-default-sink"], output, "default-sink", env, 10).strip()
    report.update({"sink": sink, "voiceListSha256": hashlib.sha256(voices.encode()).hexdigest()})
    # Preserve routing and module state even when recording/synthesis fails.
    run_logged(["pactl", "list", "short", "sources"], output, "pulse-sources", env, 10)
    run_logged(["pactl", "list", "short", "modules"], output, "pulse-modules", env, 10)
    utterance = "Floorp native speech synthesis produces real audio through the operating system."
    report["utterance"] = utterance
    raw = output / "synthesis.raw"
    with raw.open("wb") as destination, (output / "recorder.stderr.log").open("wb") as stderr:
        recorder = subprocess.Popen(["parec", "--device=" + sink + ".monitor", "--format=s16le", "--rate=" + str(SAMPLE_RATE), "--channels=1", "--raw"], stdout=destination, stderr=stderr, env=env)
        try:
            report["stage"] = "recording baseline"
            # A wall-clock delay does not prove that parec has started delivering
            # samples. Pulse can buffer longer than that delay on a fresh runner.
            wait_recorded_samples(recorder, raw, BASELINE_SAMPLES)
            report["samplesBeforeSynthesis"] = raw.stat().st_size // 2
            report["stage"] = "synthesis"
            run_logged([str(client), "-o", "espeak-ng", "-l", "en", "-w", utterance], output, "synthesis", env, 30)
            report["stage"] = "recording tail"
            wait_recorded_samples(recorder, raw, raw.stat().st_size // 2 + BASELINE_SAMPLES)
        finally:
            if recorder.poll() is None:
                recorder.send_signal(signal.SIGTERM)
            try:
                recorder.wait(timeout=5)
            except subprocess.TimeoutExpired:
                recorder.kill()
                recorder.wait(timeout=5)
            report["recorderReturnCode"] = recorder.returncode
    measure_waveform(output, report)
    report["stage"] = "waveform validation"
    write_report(output, report)
    if report["peak"] < report["minimumPeak"] or report["rms"] < report["minimumRms"]:
        raise RuntimeError("eSpeak NG produced no measurable native Pulse audio")
    report["dispatcherVersion"] = run_logged([str(args.dispatcher), "--version"], output, "dispatcher-version", env, 10).strip()
    report.update({"passed": True, "stage": "complete"})
    write_report(output, report)
    if os.environ.get("GITHUB_ENV"):
        with Path(os.environ["GITHUB_ENV"]).open("a") as stream:
            stream.write("SPEECHD_ADDRESS=" + env["SPEECHD_ADDRESS"] + "\n")
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
