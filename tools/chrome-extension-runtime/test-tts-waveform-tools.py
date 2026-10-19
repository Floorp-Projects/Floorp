#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Host-only malformed-input checks; no browser/audio qualification."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from typing import Callable, Protocol, cast
import unittest


class Analyzer(Protocol):
    MAX_BYTES: int
    MAX_METADATA_BYTES: int
    MAX_PACKET_LINE_BYTES: int
    Packet: Callable[[int, int, float, float, bool], object]

    def number(self, value: object, label: str) -> float: ...
    def read_packets(self, path: Path, raw_size: int) -> list[object]: ...
    def level(self, raw: bytes, packets: list[object], rate: int,
              start_ms: float, end_ms: float) -> object: ...
    def auxiliary_evidence(self, path: Path) -> dict[str, object]: ...


def load_analyzer() -> Analyzer:
    path = Path(__file__).with_name("tts-waveform-analyzer.py")
    spec = importlib.util.spec_from_file_location("tts_waveform_guard_tests", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Cannot load the maintained analyzer")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return cast(Analyzer, module)


analyzer = load_analyzer()


class NativeInputGuards(unittest.TestCase):
    def test_nonfinite_boolean_and_overflowing_numbers_rejected(self) -> None:
        for value in (True, float("inf"), float("nan"), "48000", 10 ** 10000):
            with self.subTest(value_type=type(value).__name__):
                with self.assertRaises(ValueError):
                    analyzer.number(value, "native metadata")

    def test_packet_payload_must_match_real_raw_byte_count(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "packets.ndjson"
            packet = {"byteOffset": 0, "frameCount": 2,
                      "firstSampleAtMs": 1000, "uncertaintyMs": 1, "silent": False}
            path.write_text(json.dumps(packet) + "\n")
            with self.assertRaisesRegex(ValueError, "exactly the recorded"):
                analyzer.read_packets(path, 2)
            for size in (True, 0, 3, analyzer.MAX_BYTES + 2):
                with self.subTest(raw_size=size):
                    with self.assertRaisesRegex(ValueError, "bounded complete sample"):
                        analyzer.read_packets(path, size)

    def test_packet_metadata_is_bounded_before_json_parsing(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "packets.ndjson"
            path.write_bytes(b"x" * (analyzer.MAX_PACKET_LINE_BYTES + 1))
            with self.assertRaisesRegex(ValueError, "line exceeds"):
                analyzer.read_packets(path, 2)
            with path.open("wb") as stream:
                stream.truncate(analyzer.MAX_METADATA_BYTES + 1)
            with self.assertRaisesRegex(ValueError, "metadata exceeds"):
                analyzer.read_packets(path, 2)

    def test_offsets_silent_flags_and_packet_timestamps_validated(self) -> None:
        cases: list[dict[str, object]] = [
            {"byteOffset": 2}, {"silent": "false"},
            {"firstSampleAtMs": float("nan")}, {"uncertaintyMs": 251},
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "packets.ndjson"
            for override in cases:
                packet = {"byteOffset": 0, "frameCount": 1,
                          "firstSampleAtMs": 1000, "uncertaintyMs": 1,
                          "silent": False, **override}
                path.write_text(json.dumps(packet) + "\n")
                with self.subTest(override=override):
                    with self.assertRaises(ValueError):
                        analyzer.read_packets(path, 2)

    def test_native_silent_flag_cannot_hide_nonzero_samples(self) -> None:
        packet = analyzer.Packet(0, 1, 1000.0, 1.0, True)
        with self.assertRaisesRegex(ValueError, "SILENT packet contains nonzero"):
            analyzer.level(b"\x01\x00", [packet], 48000, 1000.0, 1001.0)

    def test_invalid_measurement_intervals_rejected(self) -> None:
        for rate, first, last in ((0, 0.0, 1.0), (48000, 1.0, 1.0),
                                  (48000, float("nan"), 2.0)):
            with self.subTest(rate=rate, first=first, last=last):
                with self.assertRaises(ValueError):
                    analyzer.level(b"\0\0", [], rate, first, last)

    def test_uncompiled_or_duplicate_browser_reports_rejected(self) -> None:
        report = {"ok": True, "nativeFonts": False, "nativeSpeech": False}
        line = 'console.log: "[AuxiliaryNative]" ' + json.dumps(json.dumps(report))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "browser.log"
            path.write_text(line + "\n")
            with self.assertRaisesRegex(ValueError, "compiled native Auxiliary"):
                analyzer.auxiliary_evidence(path)
            path.write_text(line + "\n" + line + "\n")
            with self.assertRaisesRegex(ValueError, "exactly one"):
                analyzer.auxiliary_evidence(path)


if __name__ == "__main__":
    unittest.main()
