#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile
import unittest
from io import StringIO

import mozunit
from manifestparser import ManifestParser

here = os.path.dirname(os.path.abspath(__file__))


class TestManifestParser(unittest.TestCase):
    """
    Test the manifest parser

    You must have manifestparser installed before running these tests.
    Run ``python manifestparser.py setup develop`` with setuptools installed.
    """

    def test_sanity(self):
        """Ensure basic parser is sane"""

        parser = ManifestParser(use_toml=False)
        mozmill_example = os.path.join(here, "mozmill-example.ini")
        parser.read(mozmill_example)
        tests = parser.tests
        self.assertEqual(
            len(tests), len(open(mozmill_example).read().strip().splitlines())
        )

        # Ensure that capitalization and order aren't an issue:
        lines = ["[%s]" % test["name"] for test in tests]
        self.assertEqual(lines, open(mozmill_example).read().strip().splitlines())

        # Show how you select subsets of tests:
        mozmill_restart_example = os.path.join(here, "mozmill-restart-example.ini")
        parser.read(mozmill_restart_example)
        restart_tests = parser.get(type="restart")
        self.assertTrue(len(restart_tests) < len(parser.tests))
        self.assertEqual(
            len(restart_tests), len(parser.get(manifest=mozmill_restart_example))
        )
        self.assertFalse(
            [
                test
                for test in restart_tests
                if test["manifest"] != os.path.join(here, "mozmill-restart-example.ini")
            ]
        )
        self.assertEqual(
            parser.get("name", tags=["foo"]),
            [
                "restartTests/testExtensionInstallUninstall/test2.js",
                "restartTests/testExtensionInstallUninstall/test1.js",
            ],
        )
        self.assertEqual(
            parser.get("name", foo="bar"),
            ["restartTests/testExtensionInstallUninstall/test2.js"],
        )

    def test_sanity_toml(self):
        """Ensure basic parser is sane (TOML)"""

        parser = ManifestParser(use_toml=True)
        mozmill_example = os.path.join(here, "mozmill-example.toml")
        parser.read(mozmill_example)
        tests = parser.tests
        self.assertEqual(
            len(tests), len(open(mozmill_example).read().strip().splitlines())
        )

        # Ensure that capitalization and order aren't an issue:
        lines = ['["%s"]' % test["name"] for test in tests]
        self.assertEqual(lines, open(mozmill_example).read().strip().splitlines())

        # Show how you select subsets of tests:
        mozmill_restart_example = os.path.join(here, "mozmill-restart-example.toml")
        parser.read(mozmill_restart_example)
        restart_tests = parser.get(type="restart")
        self.assertTrue(len(restart_tests) < len(parser.tests))
        self.assertEqual(
            len(restart_tests), len(parser.get(manifest=mozmill_restart_example))
        )
        self.assertFalse(
            [
                test
                for test in restart_tests
                if test["manifest"]
                != os.path.join(here, "mozmill-restart-example.toml")
            ]
        )
        self.assertEqual(
            parser.get("name", tags=["foo"]),
            [
                "restartTests/testExtensionInstallUninstall/test2.js",
                "restartTests/testExtensionInstallUninstall/test1.js",
            ],
        )
        self.assertEqual(
            parser.get("name", foo="bar"),
            ["restartTests/testExtensionInstallUninstall/test2.js"],
        )

    def test_include(self):
        """Illustrate how include works"""

        include_example = os.path.join(here, "include-example.ini")
        parser = ManifestParser(manifests=(include_example,), use_toml=False)

        # All of the tests should be included, in order:
        self.assertEqual(parser.get("name"), ["crash-handling", "fleem", "flowers"])
        self.assertEqual(
            [
                (test["name"], os.path.basename(test["manifest"]))
                for test in parser.tests
            ],
            [
                ("crash-handling", "bar.ini"),
                ("fleem", "include-example.ini"),
                ("flowers", "foo.ini"),
            ],
        )

        # The including manifest is always reported as a part of the generated test object.
        self.assertTrue(
            all(
                [
                    t["ancestor_manifest"] == "include-example.ini"
                    for t in parser.tests
                    if t["name"] != "fleem"
                ]
            )
        )

        # The manifests should be there too:
        self.assertEqual(len(parser.manifests()), 3)

        # We already have the root directory:
        self.assertEqual(here, parser.rootdir)

        # DEFAULT values should persist across includes, unless they're
        # overwritten.  In this example, include-example.ini sets foo=bar, but
        # it's overridden to fleem in bar.ini
        self.assertEqual(parser.get("name", foo="bar"), ["fleem", "flowers"])
        self.assertEqual(parser.get("name", foo="fleem"), ["crash-handling"])

        # Passing parameters in the include section allows defining variables in
        # the submodule scope:
        self.assertEqual(parser.get("name", tags=["red"]), ["flowers"])

        # However, this should be overridable from the DEFAULT section in the
        # included file and that overridable via the key directly connected to
        # the test:
        self.assertEqual(parser.get(name="flowers")[0]["blue"], "ocean")
        self.assertEqual(parser.get(name="flowers")[0]["yellow"], "submarine")

        # You can query multiple times if you need to:
        flowers = parser.get(foo="bar")
        self.assertEqual(len(flowers), 2)

        # Using the inverse flag should invert the set of tests returned:
        self.assertEqual(
            parser.get("name", inverse=True, tags=["red"]), ["crash-handling", "fleem"]
        )

        # All of the included tests actually exist:
        self.assertEqual([i["name"] for i in parser.missing()], [])

        # Write the output to a manifest:
        buffer = StringIO()
        parser.write(fp=buffer, global_kwargs={"foo": "bar"})
        expected_output = """[DEFAULT]
foo = bar

[fleem]

[include/flowers]
blue = ocean
red = roses
yellow = submarine"""  # noqa

        self.assertEqual(buffer.getvalue().strip(), expected_output)

    def test_include_toml(self):
        """Illustrate how include works (TOML)"""

        include_example = os.path.join(here, "include-example.toml")
        parser = ManifestParser(manifests=(include_example,), use_toml=True)

        # All of the tests should be included, in order:
        self.assertEqual(parser.get("name"), ["crash-handling", "fleem", "flowers"])
        self.assertEqual(
            [
                (test["name"], os.path.basename(test["manifest"]))
                for test in parser.tests
            ],
            [
                ("crash-handling", "bar.toml"),
                ("fleem", "include-example.toml"),
                ("flowers", "foo.toml"),
            ],
        )

        # The including manifest is always reported as a part of the generated test object.
        self.assertTrue(
            all(
                [
                    t["ancestor_manifest"] == "include-example.toml"
                    for t in parser.tests
                    if t["name"] != "fleem"
                ]
            )
        )

        # The manifests should be there too:
        self.assertEqual(len(parser.manifests()), 3)

        # We already have the root directory:
        self.assertEqual(here, parser.rootdir)

        # DEFAULT values should persist across includes, unless they're
        # overwritten.  In this example, include-example.toml sets foo=bar, but
        # it's overridden to fleem in bar.toml
        self.assertEqual(parser.get("name", foo="bar"), ["fleem", "flowers"])
        self.assertEqual(parser.get("name", foo="fleem"), ["crash-handling"])

        # Passing parameters in the include section allows defining variables in
        # the submodule scope:
        self.assertEqual(parser.get("name", tags=["red"]), ["flowers"])

        # However, this should be overridable from the DEFAULT section in the
        # included file and that overridable via the key directly connected to
        # the test:
        self.assertEqual(parser.get(name="flowers")[0]["blue"], "ocean")
        self.assertEqual(parser.get(name="flowers")[0]["yellow"], "submarine")

        # You can query multiple times if you need to:
        flowers = parser.get(foo="bar")
        self.assertEqual(len(flowers), 2)

        # Using the inverse flag should invert the set of tests returned:
        self.assertEqual(
            parser.get("name", inverse=True, tags=["red"]), ["crash-handling", "fleem"]
        )

        # All of the included tests actually exist:
        self.assertEqual([i["name"] for i in parser.missing()], [])

        # Write the output to a manifest:
        buffer = StringIO()
        parser.write(fp=buffer, global_kwargs={"foo": "bar"})
        expected_output = """[DEFAULT]
foo = bar

[fleem]

[include/flowers]
blue = ocean
red = roses
yellow = submarine"""  # noqa

        self.assertEqual(buffer.getvalue().strip(), expected_output)

    def test_include_manifest_defaults(self):
        """
        Test that manifest_defaults and manifests() are correctly populated
        when includes are used.
        """

        include_example = os.path.join(here, "include-example.ini")
        noinclude_example = os.path.join(here, "just-defaults.ini")
        bar_path = os.path.join(here, "include", "bar.ini")
        foo_path = os.path.join(here, "include", "foo.ini")

        parser = ManifestParser(
            manifests=(include_example, noinclude_example), rootdir=here, use_toml=False
        )

        # Standalone manifests must be appear as-is.
        self.assertTrue(include_example in parser.manifest_defaults)
        self.assertTrue(noinclude_example in parser.manifest_defaults)

        # Included manifests must only appear together with the parent manifest
        # that included the manifest.
        self.assertFalse(bar_path in parser.manifest_defaults)
        self.assertFalse(foo_path in parser.manifest_defaults)
        ancestor_ini = os.path.relpath(include_example, parser.rootdir)
        self.assertTrue((ancestor_ini, bar_path) in parser.manifest_defaults)
        self.assertTrue((ancestor_ini, foo_path) in parser.manifest_defaults)

        # manifests() must only return file paths (strings).
        manifests = parser.manifests()
        self.assertEqual(len(manifests), 4)
        self.assertIn(foo_path, manifests)
        self.assertIn(bar_path, manifests)
        self.assertIn(include_example, manifests)
        self.assertIn(noinclude_example, manifests)

    def test_include_manifest_defaults_toml(self):
        """
        Test that manifest_defaults and manifests() are correctly populated
        when includes are used. (TOML)
        """

        include_example = os.path.join(here, "include-example.toml")
        noinclude_example = os.path.join(here, "just-defaults.toml")
        bar_path = os.path.join(here, "include", "bar.toml")
        foo_path = os.path.join(here, "include", "foo.toml")

        parser = ManifestParser(
            manifests=(include_example, noinclude_example), rootdir=here, use_toml=True
        )

        # Standalone manifests must be appear as-is.
        self.assertTrue(include_example in parser.manifest_defaults)
        self.assertTrue(noinclude_example in parser.manifest_defaults)

        # Included manifests must only appear together with the parent manifest
        # that included the manifest.
        self.assertFalse(bar_path in parser.manifest_defaults)
        self.assertFalse(foo_path in parser.manifest_defaults)
        ancestor_toml = os.path.relpath(include_example, parser.rootdir)
        self.assertTrue((ancestor_toml, bar_path) in parser.manifest_defaults)
        self.assertTrue((ancestor_toml, foo_path) in parser.manifest_defaults)

        # manifests() must only return file paths (strings).
        manifests = parser.manifests()
        self.assertEqual(len(manifests), 4)
        self.assertIn(foo_path, manifests)
        self.assertIn(bar_path, manifests)
        self.assertIn(include_example, manifests)
        self.assertIn(noinclude_example, manifests)

    def test_include_handle_defaults_False(self):
        """
        Test that manifest_defaults and manifests() are correct even when
        handle_defaults is set to False.
        """
        manifest = os.path.join(here, "include-example.ini")
        foo_path = os.path.join(here, "include", "foo.ini")

        parser = ManifestParser(
            manifests=(manifest,), handle_defaults=False, rootdir=here, use_toml=False
        )
        ancestor_ini = os.path.relpath(manifest, parser.rootdir)

        self.assertIn(manifest, parser.manifest_defaults)
        self.assertNotIn(foo_path, parser.manifest_defaults)
        self.assertIn((ancestor_ini, foo_path), parser.manifest_defaults)
        self.assertEqual(
            parser.manifest_defaults[manifest],
            {
                "foo": "bar",
                "here": here,
            },
        )
        self.assertEqual(
            parser.manifest_defaults[(ancestor_ini, foo_path)],
            {
                "here": os.path.join(here, "include"),
                "red": "roses",
                "blue": "ocean",
                "yellow": "daffodils",
            },
        )

    def test_include_handle_defaults_False_toml(self):
        """
        Test that manifest_defaults and manifests() are correct even when
        handle_defaults is set to False. (TOML)
        """
        manifest = os.path.join(here, "include-example.toml")
        foo_path = os.path.join(here, "include", "foo.toml")

        parser = ManifestParser(
            manifests=(manifest,), handle_defaults=False, rootdir=here, use_toml=True
        )
        ancestor_ini = os.path.relpath(manifest, parser.rootdir)

        self.assertIn(manifest, parser.manifest_defaults)
        self.assertNotIn(foo_path, parser.manifest_defaults)
        self.assertIn((ancestor_ini, foo_path), parser.manifest_defaults)
        self.assertEqual(
            parser.manifest_defaults[manifest],
            {
                "foo": "bar",
                "here": here,
            },
        )
        self.assertEqual(
            parser.manifest_defaults[(ancestor_ini, foo_path)],
            {
                "here": os.path.join(here, "include"),
                "red": "roses",
                "blue": "ocean",
                "yellow": "daffodils",
            },
        )

    def test_include_repeated(self):
        """
        Test that repeatedly included manifests are independent of each other.
        """
        include_example = os.path.join(here, "include-example.ini")
        included_foo = os.path.join(here, "include", "foo.ini")

        # In the expected output, blue and yellow have the values from foo.ini
        # (ocean, submarine) instead of the ones from include-example.ini
        # (violets, daffodils), because the defaults in the included file take
        # precedence over the values from the parent.
        include_output = """[include/crash-handling]
foo = fleem

[fleem]
foo = bar

[include/flowers]
blue = ocean
foo = bar
red = roses
yellow = submarine

"""
        included_output = """[include/flowers]
blue = ocean
yellow = submarine

"""

        parser = ManifestParser(
            manifests=(include_example, included_foo), rootdir=here, use_toml=False
        )
        self.assertEqual(
            parser.get("name"), ["crash-handling", "fleem", "flowers", "flowers"]
        )
        self.assertEqual(
            [
                (test["name"], os.path.basename(test["manifest"]))
                for test in parser.tests
            ],
            [
                ("crash-handling", "bar.ini"),
                ("fleem", "include-example.ini"),
                ("flowers", "foo.ini"),
                ("flowers", "foo.ini"),
            ],
        )
        self.check_included_repeat(
            parser,
            parser.tests[3],
            parser.tests[2],
            "%s%s" % (include_output, included_output),
        )

        # Same tests, but with the load order of the manifests swapped.
        parser = ManifestParser(
            manifests=(included_foo, include_example), rootdir=here, use_toml=False
        )
        self.assertEqual(
            parser.get("name"), ["flowers", "crash-handling", "fleem", "flowers"]
        )
        self.assertEqual(
            [
                (test["name"], os.path.basename(test["manifest"]))
                for test in parser.tests
            ],
            [
                ("flowers", "foo.ini"),
                ("crash-handling", "bar.ini"),
                ("fleem", "include-example.ini"),
                ("flowers", "foo.ini"),
            ],
        )
        self.check_included_repeat(
            parser,
            parser.tests[0],
            parser.tests[3],
            "%s%s" % (included_output, include_output),
        )

    def test_include_repeated_toml(self):
        """
        Test that repeatedly included manifests are independent of each other. (TOML)
        """
        include_example = os.path.join(here, "include-example.toml")
        included_foo = os.path.join(here, "include", "foo.toml")

        # In the expected output, blue and yellow have the values from foo.toml
        # (ocean, submarine) instead of the ones from include-example.toml
        # (violets, daffodils), because the defaults in the included file take
        # precedence over the values from the parent.
        include_output = """[include/crash-handling]
foo = fleem

[fleem]
foo = bar

[include/flowers]
blue = ocean
foo = bar
red = roses
yellow = submarine

"""
        included_output = """[include/flowers]
blue = ocean
yellow = submarine

"""

        parser = ManifestParser(
            manifests=(include_example, included_foo), rootdir=here, use_toml=True
        )
        self.assertEqual(
            parser.get("name"), ["crash-handling", "fleem", "flowers", "flowers"]
        )
        self.assertEqual(
            [
                (test["name"], os.path.basename(test["manifest"]))
                for test in parser.tests
            ],
            [
                ("crash-handling", "bar.toml"),
                ("fleem", "include-example.toml"),
                ("flowers", "foo.toml"),
                ("flowers", "foo.toml"),
            ],
        )
        self.check_included_repeat(
            parser,
            parser.tests[3],
            parser.tests[2],
            "%s%s" % (include_output, included_output),
            True,
        )

        # Same tests, but with the load order of the manifests swapped.
        parser = ManifestParser(
            manifests=(included_foo, include_example), rootdir=here, use_toml=True
        )
        self.assertEqual(
            parser.get("name"), ["flowers", "crash-handling", "fleem", "flowers"]
        )
        self.assertEqual(
            [
                (test["name"], os.path.basename(test["manifest"]))
                for test in parser.tests
            ],
            [
                ("flowers", "foo.toml"),
                ("crash-handling", "bar.toml"),
                ("fleem", "include-example.toml"),
                ("flowers", "foo.toml"),
            ],
        )
        self.check_included_repeat(
            parser,
            parser.tests[0],
            parser.tests[3],
            "%s%s" % (included_output, include_output),
            True,
        )

    def check_included_repeat(
        self, parser, isolated_test, included_test, expected_output, use_toml=False
    ):
        if use_toml:
            include_example_filename = "include-example.toml"
            foo_filename = "foo.toml"
        else:
            include_example_filename = "include-example.ini"
            foo_filename = "foo.ini"
        include_example = os.path.join(here, include_example_filename)
        included_foo = os.path.join(here, "include", foo_filename)
        ancestor_ini = os.path.relpath(include_example, parser.rootdir)
        manifest_default_key = (ancestor_ini, included_foo)

        self.assertFalse("ancestor_manifest" in isolated_test)
        self.assertEqual(included_test["ancestor_manifest"], include_example_filename)

        self.assertTrue(include_example in parser.manifest_defaults)
        self.assertTrue(included_foo in parser.manifest_defaults)
        self.assertTrue(manifest_default_key in parser.manifest_defaults)
        self.assertEqual(
            parser.manifest_defaults[manifest_default_key],
            {
                "foo": "bar",
                "here": os.path.join(here, "include"),
                "red": "roses",
                "blue": "ocean",
                "yellow": "daffodils",
            },
        )

        buffer = StringIO()
        parser.write(fp=buffer)
        self.assertEqual(buffer.getvalue(), expected_output)

    def test_invalid_path(self):
        """
        Test invalid path should not throw when not strict
        """
        manifest = os.path.join(here, "include-invalid.ini")
        ManifestParser(manifests=(manifest,), strict=False)

    def test_invalid_path_toml(self):
        """
        Test invalid path should not throw when not strict (TOML)
        """
        manifest = os.path.join(here, "include-invalid.ini")
        ManifestParser(manifests=(manifest,), strict=False, use_toml=True)

    def test_copy(self):
        """Test our ability to copy a set of manifests"""

        tempdir = tempfile.mkdtemp()
        include_example = os.path.join(here, "include-example.ini")
        manifest = ManifestParser(manifests=(include_example,), use_toml=False)
        manifest.copy(tempdir)
        self.assertEqual(
            sorted(os.listdir(tempdir)), ["fleem", "include", "include-example.ini"]
        )
        self.assertEqual(
            sorted(os.listdir(os.path.join(tempdir, "include"))),
            ["bar.ini", "crash-handling", "flowers", "foo.ini"],
        )
        from_manifest = ManifestParser(manifests=(include_example,))
        to_manifest = os.path.join(tempdir, "include-example.ini")
        to_manifest = ManifestParser(manifests=(to_manifest,))
        self.assertEqual(to_manifest.get("name"), from_manifest.get("name"))
        shutil.rmtree(tempdir)

    def test_copy_toml(self):
        """Test our ability to copy a set of manifests (TOML)"""

        tempdir = tempfile.mkdtemp()
        include_example = os.path.join(here, "include-example.toml")
        manifest = ManifestParser(manifests=(include_example,), use_toml=True)
        manifest.copy(tempdir)
        self.assertEqual(
            sorted(os.listdir(tempdir)), ["fleem", "include", "include-example.toml"]
        )
        self.assertEqual(
            sorted(os.listdir(os.path.join(tempdir, "include"))),
            ["bar.toml", "crash-handling", "flowers", "foo.toml"],
        )
        from_manifest = ManifestParser(manifests=(include_example,), use_toml=True)
        to_manifest = os.path.join(tempdir, "include-example.toml")
        to_manifest = ManifestParser(manifests=(to_manifest,), use_toml=True)
        self.assertEqual(to_manifest.get("name"), from_manifest.get("name"))
        shutil.rmtree(tempdir)

    def test_path_override(self):
        """You can override the path in the section too.
        This shows that you can use a relative path"""
        path_example = os.path.join(here, "path-example.ini")
        manifest = ManifestParser(manifests=(path_example,))
        self.assertEqual(manifest.tests[0]["path"], os.path.join(here, "fleem"))

    def test_path_override_toml(self):
        """You can override the path in the section too.
        This shows that you can use a relative path"""
        path_example = os.path.join(here, "path-example.toml")
        manifest = ManifestParser(manifests=(path_example,), use_toml=True)
        self.assertEqual(manifest.tests[0]["path"], os.path.join(here, "fleem"))

    def test_relative_path(self):
        """
        Relative test paths are correctly calculated.
        """
        relative_path = os.path.join(here, "relative-path.toml")
        manifest = ManifestParser(manifests=(relative_path,))
        self.assertEqual(
            manifest.tests[0]["path"], os.path.join(os.path.dirname(here), "fleem")
        )
        self.assertEqual(manifest.tests[0]["relpath"], os.path.join("..", "fleem"))
        self.assertEqual(
            manifest.tests[1]["relpath"], os.path.join("..", "testsSIBLING", "example")
        )

    def test_relative_path_toml(self):
        """
        Relative test paths are correctly calculated. (TOML)
        """
        relative_path = os.path.join(here, "relative-path.toml")
        manifest = ManifestParser(manifests=(relative_path,), use_toml=True)
        self.assertEqual(
            manifest.tests[0]["path"], os.path.join(os.path.dirname(here), "fleem")
        )
        self.assertEqual(manifest.tests[0]["relpath"], os.path.join("..", "fleem"))
        self.assertEqual(
            manifest.tests[1]["relpath"], os.path.join("..", "testsSIBLING", "example")
        )

    def test_path_from_fd(self):
        """
        Test paths are left untouched when manifest is a file-like object.
        """
        fp = StringIO("[section]\npath=fleem")
        manifest = ManifestParser(manifests=(fp,))
        self.assertEqual(manifest.tests[0]["path"], "fleem")
        self.assertEqual(manifest.tests[0]["relpath"], "fleem")
        self.assertEqual(manifest.tests[0]["manifest"], None)

    def test_comments(self):
        """
        ensure comments work, see
        https://bugzilla.mozilla.org/show_bug.cgi?id=813674
        """
        comment_example = os.path.join(here, "comment-example.ini")
        manifest = ManifestParser(manifests=(comment_example,))
        self.assertEqual(len(manifest.tests), 8)
        names = [i["name"] for i in manifest.tests]
        self.assertFalse("test_0202_app_launch_apply_update_dirlocked.js" in names)

    def test_comments_toml(self):
        """
        ensure comments work, see
        https://bugzilla.mozilla.org/show_bug.cgi?id=813674
        (TOML)
        """
        comment_example = os.path.join(here, "comment-example.toml")
        manifest = ManifestParser(manifests=(comment_example,), use_toml=True)
        self.assertEqual(len(manifest.tests), 8)
        names = [i["name"] for i in manifest.tests]
        self.assertFalse("test_0202_app_launch_apply_update_dirlocked.js" in names)

    def test_verifyDirectory(self):
        directory = os.path.join(here, "verifyDirectory")

        # correct manifest
        manifest_path = os.path.join(directory, "verifyDirectory.ini")
        manifest = ManifestParser(manifests=(manifest_path,))
        missing = manifest.verifyDirectory(directory, extensions=(".js",))
        self.assertEqual(missing, (set(), set()))

        # manifest is missing test_1.js
        test_1 = os.path.join(directory, "test_1.js")
        manifest_path = os.path.join(directory, "verifyDirectory_incomplete.ini")
        manifest = ManifestParser(manifests=(manifest_path,))
        missing = manifest.verifyDirectory(directory, extensions=(".js",))
        self.assertEqual(missing, (set(), set([test_1])))

        # filesystem is missing test_notappearinginthisfilm.js
        missing_test = os.path.join(directory, "test_notappearinginthisfilm.js")
        manifest_path = os.path.join(directory, "verifyDirectory_toocomplete.ini")
        manifest = ManifestParser(manifests=(manifest_path,))
        missing = manifest.verifyDirectory(directory, extensions=(".js",))
        self.assertEqual(missing, (set([missing_test]), set()))

    def test_verifyDirectory_toml(self):
        directory = os.path.join(here, "verifyDirectory")

        # correct manifest
        manifest_path = os.path.join(directory, "verifyDirectory.toml")
        manifest = ManifestParser(manifests=(manifest_path,), use_toml=True)
        missing = manifest.verifyDirectory(directory, extensions=(".js",))
        self.assertEqual(missing, (set(), set()))

        # manifest is missing test_1.js
        test_1 = os.path.join(directory, "test_1.js")
        manifest_path = os.path.join(directory, "verifyDirectory_incomplete.toml")
        manifest = ManifestParser(manifests=(manifest_path,), use_toml=True)
        missing = manifest.verifyDirectory(directory, extensions=(".js",))
        self.assertEqual(missing, (set(), set([test_1])))

        # filesystem is missing test_notappearinginthisfilm.js
        missing_test = os.path.join(directory, "test_notappearinginthisfilm.js")
        manifest_path = os.path.join(directory, "verifyDirectory_toocomplete.ini")
        manifest = ManifestParser(manifests=(manifest_path,), use_toml=True)
        missing = manifest.verifyDirectory(directory, extensions=(".js",))
        self.assertEqual(missing, (set([missing_test]), set()))

    def test_just_defaults(self):
        """Ensure a manifest with just a DEFAULT section exposes that data."""

        parser = ManifestParser()
        manifest = os.path.join(here, "just-defaults.ini")
        parser.read(manifest)
        self.assertEqual(len(parser.tests), 0)
        self.assertTrue(manifest in parser.manifest_defaults)
        self.assertEqual(parser.manifest_defaults[manifest]["foo"], "bar")

    def test_just_defaults_toml(self):
        """Ensure a manifest with just a DEFAULT section exposes that data. (TOML)"""

        parser = ManifestParser(use_toml=True)
        manifest = os.path.join(here, "just-defaults.toml")
        parser.read(manifest)
        self.assertEqual(len(parser.tests), 0)
        self.assertTrue(manifest in parser.manifest_defaults)
        self.assertEqual(parser.manifest_defaults[manifest]["foo"], "bar")

    def test_manifest_list(self):
        """
        Ensure a manifest with just a DEFAULT section still returns
        itself from the manifests() method.
        """

        parser = ManifestParser(use_toml=False)
        manifest = os.path.join(here, "no-tests.ini")
        parser.read(manifest)
        self.assertEqual(len(parser.tests), 0)
        self.assertTrue(len(parser.manifests()) == 1)

    def test_manifest_list_toml(self):
        """
        Ensure a manifest with just a DEFAULT section still returns
        itself from the manifests() method. (TOML)
        """

        parser = ManifestParser(use_toml=True)
        manifest = os.path.join(here, "no-tests.toml")
        parser.read(manifest)
        self.assertEqual(len(parser.tests), 0)
        self.assertTrue(len(parser.manifests()) == 1)

    def test_manifest_with_invalid_condition(self):
        """
        Ensure a skip-if or similar condition with an assignment in it
        causes errors.
        """

        parser = ManifestParser()
        manifest = os.path.join(here, "broken-skip-if.ini")
        with self.assertRaisesRegex(
            Exception, "Should not assign in skip-if condition for DEFAULT"
        ):
            parser.read(manifest)

    def test_manifest_with_invalid_condition_toml(self):
        """
        Ensure a skip-if or similar condition with an assignment in it
        causes errors. (TOML)
        """

        parser = ManifestParser(use_toml=True)
        manifest = os.path.join(here, "broken-skip-if.toml")
        with self.assertRaisesRegex(
            Exception, "Should not assign in skip-if condition for DEFAULT"
        ):
            parser.read(manifest)


if __name__ == "__main__":
    mozunit.main()
