#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Host-only automation selection/desktop checks; no browser or account access."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


PATH = Path(__file__).with_name("test-corpus-automation-floorp.py")
SPEC = importlib.util.spec_from_file_location("automation_corpus_wrapper", PATH)
WRAPPER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRAPPER)


class AutomationCorpusCommandTest(unittest.TestCase):
    def test_selection_is_exact_and_does_not_add_account_case(self):
        selected = [WRAPPER.AUTOMATION_IDS[index] for index in (0, 1, 3)]
        self.assertEqual(WRAPPER.selected_ids(",".join(selected)), selected)
        self.assertNotIn(WRAPPER.AUTOMATION_IDS[2], selected)
        corpus = json.loads((WRAPPER.TOOLS / "corpus-lock.json").read_text())
        self.assertEqual(len(corpus["extensions"]), 50)
        for extension_id in WRAPPER.AUTOMATION_IDS:
            records = [record for record in corpus["extensions"] if record["id"] == extension_id]
            self.assertEqual(len(records), 1)
            self.assertEqual(records[0]["status"], "frozen")

    def test_invalid_and_duplicate_selection_fail_before_browser(self):
        for value in ("", ",", WRAPPER.AUTOMATION_IDS[0] + ",", "a" * 32,
                      ",".join([WRAPPER.AUTOMATION_IDS[0]] * 2)):
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                WRAPPER.selected_ids(value)

    def test_linux_owns_fresh_display_and_window_manager(self):
        command = WRAPPER.browser_command("/native/floorp", "/proof", 360, platform="linux")
        self.assertEqual(command[:3], ["xvfb-run", "--auto-servernum", "--server-args=-screen 0 1600x1200x24"])
        separator = command.index("--")
        self.assertEqual(command[3:separator], [sys.executable, str(WRAPPER.TOOLS / "ci/run-with-linux-window-manager.py"), "--output", str(Path("/proof") / "window-manager")])
        self.assertIn("--headed", command[separator + 1:])
        self.assertNotIn("--headless", command)
        self.assertNotIn("--replace", command)

    def test_native_desktop_platforms_keep_headed_browser_without_xvfb(self):
        for platform in ("win32", "darwin"):
            with self.subTest(platform=platform):
                command = WRAPPER.browser_command("native browser", Path("proof"), 123, platform=platform)
                self.assertEqual(command[:2], [sys.executable, str(WRAPPER.TOOLS / "run-browser-tests.py")])
                self.assertIn("--headed", command)
                self.assertNotIn("xvfb-run", command)
                self.assertEqual(command[command.index("--browser") + 1], "native browser")
                self.assertEqual(command[command.index("--actor-module") + 1], str(WRAPPER.TOOLS / "CorpusAutomationChild.sys.mjs"))
                self.assertEqual(command[command.index("--output") + 1], str(Path("proof/browser")))

    def test_main_passes_exact_selection_and_keeps_all_evidence_under_output(self):
        selected = [WRAPPER.AUTOMATION_IDS[index] for index in (0, 1, 3)]
        with tempfile.TemporaryDirectory() as temporary:
            parent = Path(temporary) / "new-output"
            arguments = [str(PATH), "--browser", "native browser", "--ids", ",".join(selected), "--output", str(parent)]
            with patch.object(sys, "argv", arguments), patch.dict(WRAPPER.os.environ, {
                "MOZ_HEADLESS": "1", "MOZ_DISABLE_CONTENT_SANDBOX": "1",
            }), patch.object(WRAPPER.subprocess, "run") as run:
                run.return_value.returncode = 0
                self.assertEqual(WRAPPER.main(), 0)
            run.assert_called_once()
            environment = run.call_args.kwargs["env"]
            self.assertEqual(environment["FLOORP_CORPUS_IDS"], ",".join(selected))
            self.assertNotIn("MOZ_HEADLESS", environment)
            self.assertNotIn("MOZ_DISABLE_CONTENT_SANDBOX", environment)
            output = Path(environment["FLOORP_CORPUS_OUTPUT"])
            self.assertEqual(output.parent, parent.resolve())
            self.assertTrue(output.is_dir())
            command = run.call_args.args[0]
            self.assertIn(str(output / "browser"), command)
            self.assertIn(str(output / "window-manager"), command) if sys.platform == "linux" else None


if __name__ == "__main__":
    unittest.main()
