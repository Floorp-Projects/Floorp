#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Host-only command preparation checks; never launch a browser or change consent."""
import importlib.util
from pathlib import Path
import sys
import unittest


PATH = Path(__file__).with_name("test-corpus-content-floorp.py")
SPEC = importlib.util.spec_from_file_location("content_corpus_wrapper", PATH)
WRAPPER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WRAPPER)


class ContentCorpusCommandTest(unittest.TestCase):
    def test_linux_owns_fresh_display_and_window_manager(self):
        command = WRAPPER.browser_command("/native/floorp", "/proof", 240, platform="linux")
        self.assertEqual(command[:3], ["xvfb-run", "--auto-servernum", "--server-args=-screen 0 1600x1200x24"])
        separator = command.index("--")
        self.assertEqual(command[3:separator], [sys.executable, str(WRAPPER.TOOLS / "ci/run-with-linux-window-manager.py"), "--output", "/proof/window-manager"])
        self.assertEqual(command[separator + 1:separator + 3], [sys.executable, str(WRAPPER.TOOLS / "run-browser-tests.py")])
        self.assertIn("--headed", command)
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
                self.assertEqual(command[command.index("--timeout") + 1], "123")

    def test_all_private_actors_are_copied_from_exact_maintained_sources(self):
        command = WRAPPER.browser_command("floorp", "proof", 240, platform="linux")
        copied = [Path(command[index + 1]) for index, value in enumerate(command) if value == "--actor-module"]
        self.assertEqual(copied, [WRAPPER.TOOLS / "CorpusContentChild.sys.mjs", WRAPPER.TOOLS / "CorpusContentUIChild.sys.mjs", WRAPPER.CONSENT_ACTOR])
        self.assertTrue(all(path.is_file() for path in copied))


if __name__ == "__main__":
    unittest.main()
