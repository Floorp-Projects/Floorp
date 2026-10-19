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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dispatcher', type=Path, default=Path(shutil.which('speech-dispatcher') or '/usr/bin/speech-dispatcher'))
    parser.add_argument('--engine', type=Path)
    parser.add_argument('--client', type=Path)
    parser.add_argument('--engine-config', type=Path, default=Path('/etc/speech-dispatcher/modules/espeak-ng.conf'))
    args = parser.parse_args()
    args.launch_attempted = False
    try:
        return prepare(args)
    except BaseException:
        if args.launch_attempted:
            stop_owned_daemon(args.output.resolve())
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


def prepare(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True, mode=0o700)
    if not args.engine:
        files = subprocess.check_output(['dpkg', '-L', 'speech-dispatcher-espeak-ng'], text=True).splitlines()
        args.engine = next(Path(name) for name in files if name.endswith('/sd_espeak-ng'))
    engine = args.engine.resolve(strict=True)
    client = args.client or Path(shutil.which('spd-say') or args.dispatcher.parent / 'spd-say')
    config, modules, engines, logs = (output / name for name in ('config', 'config/modules', 'engines', 'logs'))
    for directory in (config, modules, engines, logs): directory.mkdir(parents=True, exist_ok=True)
    (engines / 'sd_espeak-ng').symlink_to(engine)
    shutil.copyfile(args.engine_config, modules / 'espeak-ng.conf')
    socket = output / 'speechd.sock'
    if any('"' in str(path) or '\n' in str(path) for path in (logs, socket)):
        raise ValueError('Speech configuration path contains a quote or newline')
    (config / 'speechd.conf').write_text(
        f'LogLevel 3\nLogDir "{logs}"\nCommunicationMethod "unix_socket"\n'
        f'SocketPath "{socket}"\nTimeout 0\nAudioOutputMethod "pulse"\n'
        'DefaultModule espeak-ng\nAddModule "espeak-ng" "sd_espeak-ng" "espeak-ng.conf"\n')
    env = {**os.environ, 'SPEECHD_ADDRESS': 'unix_socket:' + str(socket)}
    args.launch_attempted = True
    subprocess.run([str(args.dispatcher), '-d', '-t', '0', '-C', str(config), '-m', str(engines), '-P', str(output / 'pid')], check=True, env=env, timeout=20)
    for count in range(100):
        if socket.exists(): break
        time.sleep(0.1)
    else: raise RuntimeError('Genuine Speech Dispatcher socket did not become ready')
    voices = subprocess.check_output([str(client), '-o', 'espeak-ng', '-L'], env=env, text=True, timeout=20)
    if 'en' not in voices.lower(): raise RuntimeError('The genuine eSpeak NG module exposes no English synthesis voice')
    (output / 'voices.txt').write_text(voices)
    sink = subprocess.check_output(['pactl', 'get-default-sink'], text=True, env=env).strip()
    utterance = 'Floorp native speech synthesis produces real audio through the operating system.'
    raw = output / 'synthesis.raw'
    with raw.open('wb') as destination:
        recorder = subprocess.Popen(['parec', '--device=' + sink + '.monitor', '--format=s16le', '--rate=48000', '--channels=1', '--raw'], stdout=destination, stderr=subprocess.PIPE, env=env)
        try:
            time.sleep(0.3)
            subprocess.run([str(client), '-o', 'espeak-ng', '-l', 'en', '-w', utterance], env=env, check=True, timeout=30)
            time.sleep(0.5)
        finally:
            recorder.send_signal(signal.SIGTERM)
            recorder.communicate(timeout=5)
    payload = raw.read_bytes()
    samples = struct.unpack('<' + 'h' * (len(payload) // 2), payload[:len(payload) // 2 * 2])
    peak = max((abs(sample) for sample in samples), default=0)
    rms = math.sqrt(sum(sample * sample for sample in samples) / max(1, len(samples))) / 32768
    baseline_count = min(len(samples), int(0.2 * 48000))
    baseline_rms = math.sqrt(sum(sample * sample for sample in samples[:baseline_count]) / max(1, baseline_count)) / 32768
    if peak < 256 or rms < max(0.002, baseline_rms * 5): raise RuntimeError('eSpeak NG produced no measurable native Pulse audio')
    waveform = output / 'synthesis.wav'
    with wave.open(str(waveform), 'wb') as stream:
        stream.setnchannels(1); stream.setsampwidth(2); stream.setframerate(48000); stream.writeframes(payload)
    raw.unlink()
    report = {'passed': True, 'engine': str(engine), 'engineSha256': hashlib.sha256(engine.read_bytes()).hexdigest(), 'dispatcherVersion': subprocess.check_output([str(args.dispatcher), '--version'], env=env, text=True).strip(), 'speechdAddress': env['SPEECHD_ADDRESS'], 'voiceListSha256': hashlib.sha256(voices.encode()).hexdigest(), 'sink': sink, 'utterance': utterance, 'waveform': str(waveform), 'waveformSha256': hashlib.sha256(waveform.read_bytes()).hexdigest(), 'sampleRate': 48000, 'sampleCount': len(samples), 'peak': peak, 'rms': rms, 'baselineRms': baseline_rms}
    (output / 'backend.json').write_text(json.dumps(report, indent=2) + '\n')
    if os.environ.get('GITHUB_ENV'):
        with Path(os.environ['GITHUB_ENV']).open('a') as stream: stream.write('SPEECHD_ADDRESS=' + env['SPEECHD_ADDRESS'] + '\n')
    print(json.dumps(report))
    return 0


if __name__ == '__main__': raise SystemExit(main())
