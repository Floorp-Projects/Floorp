#!/usr/bin/env python3
"""Issue #2816: exercise the real generator with isolated Lepton fixtures.

    python3 tools/test_gen_chrome_extras.py

The default main generator fails these tests until PR #2819 is applied.
To check an existing candidate without changing main:
    python3 tools/test_gen_chrome_extras.py --generator /path/to/candidate.py
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

GENERATOR = Path(__file__).with_name("gen-chrome-extras.py").resolve()


def generate(condition: str, rules: str) -> str:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        source = root / "browser-features/skin/lepton/css"
        source.mkdir(parents=True)
        (source / "leptonChrome.css").write_text(
            f"@media {condition} {{\n"
            + "\n".join("  " + line for line in rules.splitlines())
            + "\n}\n",
            encoding="utf-8",
        )
        subprocess.run(
            [sys.executable, str(GENERATOR)],
            cwd=source,
            check=True,
            capture_output=True,
            text=True,
        )
        css = (root / "browser-features/chrome/common/designs/chrome-extras-css/icon-menu.css").read_text(encoding="utf-8")
        # Provenance comments are not executable media queries.
        import re

        return re.sub(r"/\*[\s\S]*?\*/", "", css).strip()


class PlatformConditions(unittest.TestCase):
    gate = '(not (-moz-bool-pref: "userChrome.icon.disabled")) and (-moz-bool-pref: "userChrome.icon.menu")'

    def test_macos_padding_stays_conditional(self) -> None:
        css = generate(
            self.gate + ' and (-moz-bool-pref: "layout.css.osx-font-smoothing.enabled")',
            "menupopup > menuitem {\n  padding-inline-start: 0 !important;\n}",
        )
        self.assertTrue(css.startswith('@media (-moz-pref("layout.css.osx-font-smoothing.enabled")) {'), css)
        self.assertIn("padding-inline-start: 0 !important;", css)
        self.assertNotIn("userChrome.", css)
        self.assertNotIn("-moz-bool-pref", css)

    def test_linux_icon_gutter_stays_conditional(self) -> None:
        css = generate(
            self.gate + " and (-moz-gtk-csd-available)",
            ".menu-icon {\n  display: unset !important;\n}",
        )
        self.assertTrue(css.startswith("@media (-moz-gtk-csd-available) {"), css)
        self.assertIn("display: unset !important;", css)

    def test_windows_bookmark_spacing_stays_conditional(self) -> None:
        css = generate(
            self.gate + " and (-moz-platform: windows)",
            "#BMB_bookmarksPopup menuitem {\n  padding-inline-start: 24px !important;\n}",
        )
        self.assertTrue(css.startswith("@media (-moz-platform: windows) {"), css)
        self.assertIn("padding-inline-start: 24px !important;", css)

    def test_platform_query_union_is_preserved(self) -> None:
        css = generate(
            self.gate + " and (-moz-platform: windows), " + self.gate + " and (-moz-gtk-csd-available)",
            "menuitem {\n  background-size: 16px;\n}",
        )
        self.assertTrue(css.startswith("@media (-moz-platform: windows), (-moz-gtk-csd-available) {"), css)
        self.assertIn("background-size: 16px;", css)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--generator", type=Path)
    options, remaining = parser.parse_known_args()
    if options.generator:
        GENERATOR = options.generator.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
