#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Strict analyzer for actual-browser TTS and native loopback packets.

This never synthesizes audio. The raw file contains only actual recorder packets;
native no-render gaps are represented explicitly in the derived timeline WAV.
"""
from __future__ import annotations

import argparse
from array import array
from dataclasses import asdict, dataclass
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import cast
import wave

MAX_BYTES = 32 * 1024 * 1024
MAX_SECONDS = 300
MAX_METADATA_BYTES = 64 * 1024 * 1024
MAX_PACKET_LINE_BYTES = 64 * 1024


def object_value(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ValueError(label + " must be an object")
    return cast(dict[str, object], value)


def read_object(path: Path) -> dict[str, object]:
    return object_value(json.loads(path.read_text(encoding="utf-8-sig")), str(path))


def number(value: object, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (float, int)):
        raise ValueError(label + " must be a number")
    try:
        result = float(value)
    except OverflowError as error:
        raise ValueError(label + " exceeds the numeric bound") from error
    if not math.isfinite(result):
        raise ValueError(label + " must be finite")
    return result


def integer(value: object, label: str) -> int:
    result = number(value, label)
    if result < 0 or result != int(result):
        raise ValueError(label + " must be a nonnegative integer")
    return int(result)


@dataclass(frozen=True)
class Packet:
    byte_offset: int
    frames: int
    first_sample_ms: float
    uncertainty_ms: float
    silent: bool


@dataclass(frozen=True)
class Level:
    actual_frames: int
    actual_non_silent_frames: int
    no_packet_frames: int
    native_silent_frames: int
    rms: float
    peak: float


def auxiliary_evidence(log: Path, runtime_os: str | None = None) -> dict[str, object]:
    reports: list[dict[str, object]] = []
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        marker = '"[AuxiliaryNative]" '
        if marker not in line:
            continue
        encoded = line.split(marker, 1)[1].strip()
        try:
            payload, _ = json.JSONDecoder().raw_decode(encoded)
            if not isinstance(payload, str):
                continue
            report = object_value(json.loads(payload), "Auxiliary report")
        except (ValueError, json.JSONDecodeError):
            continue
        reports.append(report)
    if len(reports) != 1:
        raise ValueError("Require exactly one actual AuxiliaryNative JSON report")
    result = reports[0]
    if result.get("ok") is not True or result.get("nativeFonts") is not True or result.get("nativeSpeech") is not True:
        raise ValueError("Actual browser did not pass compiled native Auxiliary tests")
    evidence = object_value(result.get("speechEvidence"), "speechEvidence")
    events_value = evidence.get("events")
    if not isinstance(events_value, list):
        raise ValueError("Actual speech events are missing")
    events = [object_value(event, "speech event") for event in events_value]
    if not events or events[0].get("type") != "start" or events[-1].get("type") != "end":
        raise ValueError("Actual native start/end events are required")
    required_boundaries = ["word", "sentence"]
    if "voice" in evidence:
        voice = object_value(evidence["voice"], "selected native voice")
        capabilities = voice.get("eventTypes")
        if (not isinstance(voice.get("voiceName"), str) or not voice["voiceName"] or
                not isinstance(capabilities, list) or not capabilities or
                not all(isinstance(item, str) for item in capabilities) or
                len(set(capabilities)) != len(capabilities) or
                "word" not in capabilities):
            raise ValueError("Selected native voice capabilities are malformed or lack word events")
        if "sentence" not in capabilities:
            # The authenticated browser platform and its selected voice must
            # agree. Legacy reports and other compiled backends stay strict.
            if runtime_os != "Darwin":
                raise ValueError("Only the native Darwin backend omits sentence capability")
            if any(event.get("type") == "sentence" for event in events):
                raise ValueError("Unadvertised native sentence event")
            required_boundaries = ["word"]
    for event_type in required_boundaries:
        if not any(event.get("type") == event_type for event in events):
            raise ValueError("Missing compiled native " + event_type + " event")
    first = number(events[0].get("receivedAt"), "start.receivedAt")
    last = number(events[-1].get("receivedAt"), "end.receivedAt")
    requested = number(evidence.get("startedAt"), "speech requested time")
    completed = number(evidence.get("completedAt"), "speech completion time")
    if not requested <= first < last <= completed or last - first < 500 or completed - requested > 22000:
        raise ValueError("Native browser speech event timing is inconsistent")
    return {**evidence, "firstStartMs": first, "lastEndMs": last}


def read_packets(path: Path, raw_size: int) -> list[Packet]:
    if isinstance(raw_size, bool) or raw_size <= 0 or raw_size % 2 or raw_size > MAX_BYTES:
        raise ValueError("Raw native PCM must have a bounded complete sample payload")
    if path.stat().st_size > MAX_METADATA_BYTES:
        raise ValueError("Native packet metadata exceeds its byte bound")
    packets: list[Packet] = []
    next_offset = 0
    prior_start = -math.inf
    with path.open("rb") as stream:
        while True:
            line = stream.readline(MAX_PACKET_LINE_BYTES + 1)
            if not line:
                break
            if len(line) > MAX_PACKET_LINE_BYTES:
                raise ValueError("Native packet metadata line exceeds its byte bound")
            value = object_value(json.loads(line), "native packet")
            offset = integer(value.get("byteOffset"), "byteOffset")
            frames = integer(value.get("frameCount"), "frameCount")
            start = number(value.get("firstSampleAtMs"), "firstSampleAtMs")
            uncertainty = number(value.get("uncertaintyMs"), "uncertaintyMs")
            if offset != next_offset or not frames or start < prior_start or not 0 <= uncertainty <= 250:
                raise ValueError("Native packets are out of order, have gaps in raw bytes, or have unbounded timing")
            if not isinstance(value.get("silent"), bool):
                raise ValueError("Native silent flag must be explicit")
            packets.append(Packet(offset, frames, start, uncertainty, value["silent"] is True))
            next_offset += frames * 2
            prior_start = start
            if next_offset > MAX_BYTES or len(packets) > 100000:
                raise ValueError("Native recording exceeds its bounds")
    if not packets or next_offset != raw_size:
        raise ValueError("Native packet metadata does not cover exactly the recorded raw payload")
    return packets


def packet_samples(raw: bytes, packet: Packet, start: int, end: int) -> array[int]:
    samples: array[int] = array("h")
    samples.frombytes(raw[packet.byte_offset + start * 2:packet.byte_offset + end * 2])
    if sys.byteorder != "little":
        samples.byteswap()
    return samples


def level(raw: bytes, packets: list[Packet], rate: int, start_ms: float, end_ms: float) -> Level:
    if isinstance(rate, bool) or not 8000 <= rate <= 192000:
        raise ValueError("Native audio measurement requires a supported sample rate")
    if not math.isfinite(start_ms) or not math.isfinite(end_ms) or end_ms <= start_ms:
        raise ValueError("Native audio measurement interval is invalid")
    squares = 0
    peak = 0
    actual = 0
    native_silent = 0
    non_silent = 0
    for packet in packets:
        begin = max(0, math.ceil((start_ms - packet.first_sample_ms) * rate / 1000))
        end = min(packet.frames, math.floor((end_ms - packet.first_sample_ms) * rate / 1000))
        if begin >= end:
            continue
        samples = packet_samples(raw, packet, begin, end)
        actual += len(samples)
        if packet.silent:
            native_silent += len(samples)
            if any(samples):
                raise ValueError("Native SILENT packet contains nonzero sample data")
        else:
            non_silent += len(samples)
        squares += sum(sample * sample for sample in samples)
        peak = max(peak, max((abs(sample) for sample in samples), default=0))
    interval_frames = round((end_ms - start_ms) * rate / 1000)
    if actual > interval_frames + rate // 20:
        raise ValueError("Native audio timestamps overlap beyond bounded rounding")
    # No-packet gaps are explicit native-render silence, never signal samples.
    total = max(interval_frames, actual, 1)
    return Level(actual, non_silent, max(0, interval_frames - actual), native_silent,
                 math.sqrt(squares / total) / 32768, peak / 32768)


def timeline_wav(path: Path, raw: bytes, packets: list[Packet], rate: int,
                 capture_start_ms: float, capture_end_ms: float) -> None:
    frames = math.ceil((capture_end_ms - capture_start_ms) * rate / 1000)
    if frames <= 0 or frames * 2 > MAX_BYTES:
        raise ValueError("Derived WAV exceeds its recording bounds")
    position = 0
    with wave.open(str(path), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(rate)
        for packet in packets:
            offset = round((packet.first_sample_ms - capture_start_ms) * rate / 1000)
            if offset < position:
                # Tiny packet-clock jitter may overlap; preserve actual samples
                # in packet order and report original timestamps separately.
                if position - offset > rate // 20:
                    raise ValueError("Native packet timeline moves backwards")
                offset = position
            if offset < 0 or (offset + packet.frames) * 2 > MAX_BYTES:
                raise ValueError("Derived native packet WAV exceeds its byte bound")
            if offset > position:
                stream.writeframesraw(bytes((offset - position) * 2))
            stream.writeframesraw(raw[packet.byte_offset:packet.byte_offset + packet.frames * 2])
            position = offset + packet.frames
        if position < frames:
            stream.writeframesraw(bytes((frames - position) * 2))


def analyze(directory: Path, browser_output: Path) -> dict[str, object]:
    ready = read_object(directory / "ready.json")
    done = read_object(directory / "done.json")
    if ready.get("channels") != 1 or ready.get("sampleFormat") != "s16le":
        raise ValueError("Recorder must expose genuine mono PCM16 little endian")
    rate = integer(ready.get("sampleRate"), "sampleRate")
    if not 8000 <= rate <= 192000:
        raise ValueError("Native sample rate is outside supported recording bounds")
    if done.get("passed") is not True or done.get("lostPackets") is not False or done.get("deviceChanged") is not False:
        raise ValueError("Native recorder did not complete without packet loss/device changes")
    first = number(done.get("captureStartedAtMs"), "capture start")
    last = number(done.get("captureEndedAtMs"), "capture end")
    if not 0 < last - first <= MAX_SECONDS * 1000:
        raise ValueError("Native capture duration is outside its bound")
    raw_path = directory / "capture.pcm"
    if not 0 < raw_path.stat().st_size <= MAX_BYTES:
        raise ValueError("Actual raw packet recording is empty or too large")
    raw = raw_path.read_bytes()
    packets = read_packets(directory / "packets.ndjson", len(raw))
    for packet in packets:
        if packet.first_sample_ms < first - packet.uncertainty_ms or packet.first_sample_ms + packet.frames * 1000 / rate > last + packet.uncertainty_ms:
            raise ValueError("Native packet timestamps escape the actual capture interval")
    reports = list(browser_output.glob("run-*/report.json"))
    if len(reports) != 1:
        raise ValueError("Expected exactly one isolated browser report")
    browser = read_object(reports[0])
    modules = browser.get("modules")
    if browser.get("passed") is not True or browser.get("nativeCandidateIdentityMatches") is not True or not isinstance(modules, list) or len(modules) != 1:
        raise ValueError("Browser did not pass one authenticated candidate suite")
    suite = object_value(modules[0], "browser module")
    if suite.get("passed") is not True or not str(suite.get("path", "")).replace("\\", "/").endswith("/test/auxiliary/AuxiliaryNative.test.mts"):
        raise ValueError("The recording must contain only actual AuxiliaryNative suite output")
    runtime = object_value(browser.get("runtime"), "authenticated browser runtime")
    runtime_os = runtime.get("os")
    if not isinstance(runtime_os, str):
        raise ValueError("Authenticated browser operating system is missing")
    evidence = auxiliary_evidence(reports[0].parent / "browser.log", runtime_os)
    start = number(evidence["firstStartMs"], "native start")
    end = number(evidence["lastEndMs"], "native end")
    if first > start - 1000 or last < end:
        raise ValueError("Recording does not cover real browser speech and its baseline")
    baseline = level(raw, packets, rate, start - 1000, start - 250)
    speech = level(raw, packets, rate, start + 150, end - 50)
    rounding_tolerance = max(2, rate // 500)
    if baseline.no_packet_frames > rounding_tolerance and ready.get("nativeNoPacketsMeanRenderSilence") is not True:
        raise ValueError("Recorder baseline has unexplained missing native packets")
    bins: list[dict[str, object]] = []
    cursor = start + 150
    while cursor + 100 <= end - 50:
        measured = level(raw, packets, rate, cursor, cursor + 100)
        bins.append({"startMs": cursor, "endMs": cursor + 100, **asdict(measured)})
        cursor += 100
    voiced = sum(number(item["rms"], "bin RMS") >= 0.004 and integer(item["actual_non_silent_frames"], "bin actual frames") >= rate // 50 for item in bins)
    passed = baseline.rms < 0.01 and speech.rms >= max(0.003, baseline.rms * 5) and speech.peak >= 0.02 and voiced >= 3
    wav = directory / "browser-tts.wav"
    timeline_wav(wav, raw, packets, rate, first, last)
    result: dict[str, object] = {
        "passed": passed, "source": "authenticated-browser-chrome.tts", "recorder": ready,
        "capture": done, "speechEvidence": evidence, "browserReport": str(reports[0]),
        "baseline": asdict(baseline), "speech": asdict(speech), "speechBins": bins,
        "voicedBins": voiced, "timingUncertaintyMs": max(packet.uncertainty_ms for packet in packets),
        "intervalRoundingToleranceFrames": rounding_tolerance,
        "waveform": str(wav), "waveformSha256": hashlib.sha256(wav.read_bytes()).hexdigest(),
        "packetPcm": str(raw_path), "packetPcmSha256": hashlib.sha256(raw).hexdigest(),
        "gapRepresentation": "native no-render gaps are explicit zero spans only in derived WAV; signal statistics require real non-silent native packets",
    }
    (directory / "waveform-report.json").write_text(json.dumps(result, indent=2) + "\n")
    if not passed:
        raise ValueError("Actual browser TTS waveform is silent, too weak, too short, or contaminated by a noisy baseline")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recording", type=Path, required=True)
    parser.add_argument("--browser-output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.recording.resolve(strict=True), args.browser_output.resolve(strict=True))
    print(json.dumps({"passed": result["passed"], "waveform": result["waveform"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
