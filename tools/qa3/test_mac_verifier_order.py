# SPDX-License-Identifier: MPL-2.0

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from mac_verifier_order import validate_stage_order


PRODUCER_STAGES = [
    "configure",
    "effective-config",
    "fresh45-exports",
    "mozilla-upstream-helpers-build",
    "mozilla-upstream-helpers-install",
    "mozilla-upstream-inventory",
    "mozilla-shell-build",
    "mozilla-http3-build",
    "mozilla-browser-build",
    "mozilla-updater-build",
    "mozilla-channel-build",
    "mozilla-dmg-build",
    "mozilla-resources-build",
    "mozilla-upstream-child-build",
    "mozilla-upstream-child-libs",
    "mozilla-upstream-resources-build",
    "mozilla-upstream-xpcom",
    "mozilla-xpcshell",
]


def summary(stages):
    return {"stages": [{"stage": stage} for stage in stages]}


class MacVerifierOrderTests(unittest.TestCase):
    def test_accepts_producer_order_with_interleaved_diagnostics(self):
        stages = PRODUCER_STAGES.copy()
        stages.insert(stages.index("mozilla-browser-build"), "nss-runtime-load")
        validate_stage_order(summary(stages))

    def test_rejects_inventory_after_target_builds(self):
        stages = PRODUCER_STAGES.copy()
        stages.remove("mozilla-upstream-inventory")
        stages.insert(
            stages.index("mozilla-upstream-child-build"), "mozilla-upstream-inventory"
        )
        with self.assertRaisesRegex(ValueError, "inventory after helper installation"):
            validate_stage_order(summary(stages))

    def test_rejects_reordered_helper_or_target_stages(self):
        for left, right in (
            ("mozilla-upstream-helpers-build", "mozilla-upstream-helpers-install"),
            ("mozilla-shell-build", "mozilla-http3-build"),
        ):
            stages = PRODUCER_STAGES.copy()
            first, second = stages.index(left), stages.index(right)
            stages[first], stages[second] = stages[second], stages[first]
            with self.subTest(stage=left), self.assertRaises(ValueError):
                validate_stage_order(summary(stages))

    def test_rejects_missing_or_duplicate_required_stages(self):
        for stage in ("mozilla-upstream-inventory", "mozilla-dmg-build"):
            missing = [name for name in PRODUCER_STAGES if name != stage]
            duplicate = [*PRODUCER_STAGES, stage]
            for stages in (missing, duplicate):
                with self.subTest(stage=stage, count=stages.count(stage)):
                    with self.assertRaises(ValueError):
                        validate_stage_order(summary(stages))

    def test_rejects_full_build_and_checkpoint_stages(self):
        for stage in ("full-build", "checkpoint-restore"):
            with self.subTest(stage=stage), self.assertRaisesRegex(
                ValueError, "outside the Firefox45 verifier scope"
            ):
                validate_stage_order(summary([*PRODUCER_STAGES, stage]))

    def test_rejects_malformed_or_oversized_stage_lists(self):
        for value in (
            [],
            {},
            {"stages": "configure"},
            {"stages": [{}]},
            {"stages": ["configure"]},
            summary([None]),
            summary(["diagnostic"] * 129),
        ):
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_stage_order(value)

    def test_cli_accepts_valid_summary_and_rejects_invalid_input(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            command = [
                sys.executable,
                "-B",
                str(Path(__file__).with_name("mac_verifier_order.py")),
                str(path),
            ]
            path.write_text(json.dumps(summary(PRODUCER_STAGES)), encoding="utf-8")
            result = subprocess.run(command, capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout, "Mac Firefox45 stage order verified\n")

            for payload in ('{"stages":[],"stages":[]}', '{"stages": ['):
                path.write_text(payload, encoding="utf-8")
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(result.returncode, 1)
                self.assertEqual(result.stdout, "")
                self.assertIn("Mac verifier order check failed:", result.stderr)
