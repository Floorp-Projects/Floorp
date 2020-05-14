# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals
import unittest
import six

from . import MockTOMLParser
from compare_locales.paths.matcher import Matcher
from compare_locales.paths.project import ProjectConfig, ExcludeError
from compare_locales import mozpath


class TestConfigParser(unittest.TestCase):
    def test_includes(self):
        parser = MockTOMLParser({
            "root.toml": """
basepath = "."
[env]
  o = "toolkit"
[[includes]]
  path = "{o}/other.toml"
[[includes]]
  path = "dom/more.toml"
""",
            "other.toml": """
basepath = "."
""",
            "more.toml": """
basepath = "."
"""
        })
        config = parser.parse("root.toml")
        self.assertIsInstance(config, ProjectConfig)
        configs = list(config.configs)
        self.assertEqual(configs[0], config)
        self.assertListEqual(
            [c.path for c in configs],
            [
                "root.toml",
                mozpath.abspath("toolkit/other.toml"),
                mozpath.abspath("dom/more.toml"),
            ]
        )

    def test_excludes(self):
        parser = MockTOMLParser({
            "root.toml": """
basepath = "."
[[excludes]]
  path = "exclude.toml"
[[excludes]]
  path = "other-exclude.toml"
  """,
            "exclude.toml": """
basepath = "."
""",
            "other-exclude.toml": """
basepath = "."
""",
            "grandparent.toml": """
basepath = "."
[[includes]]
  path = "root.toml"
""",
            "wrapped.toml": """
basepath = "."
[[excludes]]
  path = "root.toml"
 """
        })
        config = parser.parse("root.toml")
        self.assertIsInstance(config, ProjectConfig)
        configs = list(config.configs)
        self.assertListEqual(configs, [config])
        self.assertEqual(
            [c.path for c in config.excludes],
            [
                mozpath.abspath("exclude.toml"),
                mozpath.abspath("other-exclude.toml"),
            ]
        )
        with six.assertRaisesRegex(self, ExcludeError, 'Included configs'):
            parser.parse("grandparent.toml")
        with six.assertRaisesRegex(self, ExcludeError, 'Excluded configs'):
            parser.parse("wrapped.toml")

    def test_paths(self):
        parser = MockTOMLParser({
            "l10n.toml": """
[[paths]]
  l10n = "some/{locale}/*"
""",
            "ref.toml": """
[[paths]]
  reference = "ref/l10n/*"
  l10n = "some/{locale}/*"
""",
            "tests.toml": """
[[paths]]
  l10n = "some/{locale}/*"
  test = [
    "run_this",
  ]
""",
        })

        paths = parser.parse("l10n.toml").paths
        self.assertIn("l10n", paths[0])
        self.assertIsInstance(paths[0]["l10n"], Matcher)
        self.assertNotIn("reference", paths[0])
        self.assertNotIn("test", paths[0])
        paths = parser.parse("ref.toml").paths
        self.assertIn("l10n", paths[0])
        self.assertIsInstance(paths[0]["l10n"], Matcher)
        self.assertIn("reference", paths[0])
        self.assertIsInstance(paths[0]["reference"], Matcher)
        self.assertNotIn("test", paths[0])
        paths = parser.parse("tests.toml").paths
        self.assertIn("l10n", paths[0])
        self.assertIsInstance(paths[0]["l10n"], Matcher)
        self.assertNotIn("reference", paths[0])
        self.assertIn("test", paths[0])
        self.assertListEqual(paths[0]["test"], ["run_this"])
