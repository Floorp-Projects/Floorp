#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import unittest

import mozunit
from manifestparser import ManifestParser, combine_fields

here = os.path.dirname(os.path.abspath(__file__))


def deepstrip(txt):
    "Collapses all repeated blanks to one blank, and strips"
    return re.sub(r"  +", " ", txt).strip()


class TestDefaultSkipif(unittest.TestCase):
    """Tests applying a skip-if condition in [DEFAULT] and || with the value for the test"""

    def test_defaults_toml(self):
        default = os.path.join(here, "default-skipif.toml")
        parser = ManifestParser(manifests=(default,), use_toml=True)
        for test in parser.tests:
            if test["name"] == "test1":
                self.assertEqual(
                    deepstrip(test["skip-if"]), "os == 'win' && debug\ndebug"
                )
            elif test["name"] == "test2":
                self.assertEqual(
                    deepstrip(test["skip-if"]), "os == 'win' && debug\nos == 'linux'"
                )
            elif test["name"] == "test3":
                self.assertEqual(
                    deepstrip(test["skip-if"]), "os == 'win' && debug\nos == 'win'"
                )
            elif test["name"] == "test4":
                self.assertEqual(
                    deepstrip(test["skip-if"]),
                    "os == 'win' && debug\nos == 'win' && debug",
                )
            elif test["name"] == "test5":
                self.assertEqual(deepstrip(test["skip-if"]), "os == 'win' && debug")
            elif test["name"] == "test6":
                self.assertEqual(
                    deepstrip(test["skip-if"]), "os == 'win' && debug\ndebug"
                )


class TestDefaultSupportFiles(unittest.TestCase):
    """Tests combining support-files field in [DEFAULT] with the value for a test"""

    def test_defaults_toml(self):
        default = os.path.join(here, "default-suppfiles.toml")
        parser = ManifestParser(manifests=(default,), use_toml=True)
        expected_supp_files = {
            "test7": "foo.js",
            "test8": "foo.js bar.js",
            "test9": "foo.js",
        }
        for test in parser.tests:
            expected = expected_supp_files[test["name"]]
            self.assertEqual(test["support-files"], expected)


class TestOmitDefaults(unittest.TestCase):
    """Tests passing omit-defaults prevents defaults from propagating to definitions."""

    def test_defaults_toml(self):
        manifests = (
            os.path.join(here, "default-suppfiles.toml"),
            os.path.join(here, "default-skipif.toml"),
        )
        parser = ManifestParser(
            manifests=manifests, handle_defaults=False, use_toml=True
        )
        expected_supp_files = {
            "test8": "bar.js",
        }
        expected_skip_ifs = {
            "test1": "debug",
            "test2": "os == 'linux'",
            "test3": "os == 'win'",
            "test4": "os == 'win' && debug",
            "test6": "debug",
        }
        for test in parser.tests:
            for field, expectations in (
                ("support-files", expected_supp_files),
                ("skip-if", expected_skip_ifs),
            ):
                expected = expectations.get(test["name"])
                if not expected:
                    self.assertNotIn(field, test)
                else:
                    self.assertEqual(test[field].strip(), expected)

        expected_defaults = {
            os.path.join(here, "default-suppfiles.toml"): {
                "support-files": "foo.js",
            },
            os.path.join(here, "default-skipif.toml"): {
                "skip-if": "os == 'win' && debug",
            },
        }
        for path, defaults in expected_defaults.items():
            self.assertIn(path, parser.manifest_defaults)
            actual_defaults = parser.manifest_defaults[path]
            for key, value in defaults.items():
                self.assertIn(key, actual_defaults)
                self.assertEqual(value, actual_defaults[key].strip())


class TestSubsuiteDefaults(unittest.TestCase):
    """Test that subsuites are handled correctly when managing defaults
    outside of the manifest parser."""

    def test_subsuite_defaults_toml(self):
        manifest = os.path.join(here, "default-subsuite.toml")
        parser = ManifestParser(
            manifests=(manifest,), handle_defaults=False, use_toml=True
        )
        expected_subsuites = {
            "test1": "baz",
            "test2": "foo",
        }
        defaults = parser.manifest_defaults[manifest]
        for test in parser.tests:
            value = combine_fields(defaults, test)
            self.assertEqual(expected_subsuites[value["name"]], value["subsuite"])


if __name__ == "__main__":
    mozunit.main()
