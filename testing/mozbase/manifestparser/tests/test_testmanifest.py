#!/usr/bin/env python

import os
import shutil
import tempfile
import unittest

import mozunit
from manifestparser import ParseError, TestManifest
from manifestparser.filters import subsuite

here = os.path.dirname(os.path.abspath(__file__))


class TestTestManifest(unittest.TestCase):
    """Test the Test Manifest"""

    def test_testmanifest(self):
        # Test filtering based on platform:
        filter_example = os.path.join(here, "filter-example.ini")
        manifest = TestManifest(manifests=(filter_example,), strict=False)
        self.assertEqual(
            [
                i["name"]
                for i in manifest.active_tests(os="win", disabled=False, exists=False)
            ],
            ["windowstest", "fleem"],
        )
        self.assertEqual(
            [
                i["name"]
                for i in manifest.active_tests(os="linux", disabled=False, exists=False)
            ],
            ["fleem", "linuxtest"],
        )

        # Look for existing tests.  There is only one:
        self.assertEqual([i["name"] for i in manifest.active_tests()], ["fleem"])

        # You should be able to expect failures:
        last = manifest.active_tests(exists=False, toolkit="gtk")[-1]
        self.assertEqual(last["name"], "linuxtest")
        self.assertEqual(last["expected"], "pass")
        last = manifest.active_tests(exists=False, toolkit="cocoa")[-1]
        self.assertEqual(last["expected"], "fail")

    def test_testmanifest_toml(self):
        # Test filtering based on platform:
        filter_example = os.path.join(here, "filter-example.toml")
        manifest = TestManifest(
            manifests=(filter_example,), strict=False, use_toml=True
        )
        self.assertEqual(
            [
                i["name"]
                for i in manifest.active_tests(os="win", disabled=False, exists=False)
            ],
            ["windowstest", "fleem"],
        )
        self.assertEqual(
            [
                i["name"]
                for i in manifest.active_tests(os="linux", disabled=False, exists=False)
            ],
            ["fleem", "linuxtest"],
        )

        # Look for existing tests.  There is only one:
        self.assertEqual([i["name"] for i in manifest.active_tests()], ["fleem"])

        # You should be able to expect failures:
        last = manifest.active_tests(exists=False, toolkit="gtk")[-1]
        self.assertEqual(last["name"], "linuxtest")
        self.assertEqual(last["expected"], "pass")
        last = manifest.active_tests(exists=False, toolkit="cocoa")[-1]
        self.assertEqual(last["expected"], "fail")

    def test_missing_paths(self):
        """
        Test paths that don't exist raise an exception in strict mode.
        """
        tempdir = tempfile.mkdtemp()

        missing_path = os.path.join(here, "missing-path.ini")
        manifest = TestManifest(manifests=(missing_path,), strict=True)
        self.assertRaises(IOError, manifest.active_tests)
        self.assertRaises(IOError, manifest.copy, tempdir)
        self.assertRaises(IOError, manifest.update, tempdir)

        shutil.rmtree(tempdir)

    def test_missing_paths_toml(self):
        """
        Test paths that don't exist raise an exception in strict mode. (TOML)
        """
        tempdir = tempfile.mkdtemp()

        missing_path = os.path.join(here, "missing-path.toml")
        manifest = TestManifest(manifests=(missing_path,), strict=True, use_toml=True)
        self.assertRaises(IOError, manifest.active_tests)
        self.assertRaises(IOError, manifest.copy, tempdir)
        self.assertRaises(IOError, manifest.update, tempdir)

        shutil.rmtree(tempdir)

    def test_comments(self):
        """
        ensure comments work, see
        https://bugzilla.mozilla.org/show_bug.cgi?id=813674
        """
        comment_example = os.path.join(here, "comment-example.toml")
        manifest = TestManifest(manifests=(comment_example,))
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
        manifest = TestManifest(manifests=(comment_example,), use_toml=True)
        self.assertEqual(len(manifest.tests), 8)
        names = [i["name"] for i in manifest.tests]
        self.assertFalse("test_0202_app_launch_apply_update_dirlocked.js" in names)

    def test_manifest_subsuites(self):
        """
        test subsuites and conditional subsuites
        """
        relative_path = os.path.join(here, "subsuite.ini")
        manifest = TestManifest(manifests=(relative_path,))
        info = {"foo": "bar"}

        # 6 tests total
        tests = manifest.active_tests(exists=False, **info)
        self.assertEqual(len(tests), 6)

        # only 3 tests for subsuite bar when foo==bar
        tests = manifest.active_tests(exists=False, filters=[subsuite("bar")], **info)
        self.assertEqual(len(tests), 3)

        # only 1 test for subsuite baz, regardless of conditions
        other = {"something": "else"}
        tests = manifest.active_tests(exists=False, filters=[subsuite("baz")], **info)
        self.assertEqual(len(tests), 1)
        tests = manifest.active_tests(exists=False, filters=[subsuite("baz")], **other)
        self.assertEqual(len(tests), 1)

        # 4 tests match when the condition doesn't match (all tests except
        # the unconditional subsuite)
        info = {"foo": "blah"}
        tests = manifest.active_tests(exists=False, filters=[subsuite()], **info)
        self.assertEqual(len(tests), 5)

        # test for illegal subsuite value
        manifest.tests[0]["subsuite"] = 'subsuite=bar,foo=="bar",type="nothing"'
        with self.assertRaises(ParseError):
            manifest.active_tests(exists=False, filters=[subsuite("foo")], **info)

    def test_manifest_subsuites_toml(self):
        """
        test subsuites and conditional subsuites (TOML)
        """
        relative_path = os.path.join(here, "subsuite.toml")
        manifest = TestManifest(manifests=(relative_path,), use_toml=True)
        info = {"foo": "bar"}

        # 6 tests total
        tests = manifest.active_tests(exists=False, **info)
        self.assertEqual(len(tests), 6)

        # only 3 tests for subsuite bar when foo==bar
        tests = manifest.active_tests(exists=False, filters=[subsuite("bar")], **info)
        self.assertEqual(len(tests), 3)

        # only 1 test for subsuite baz, regardless of conditions
        other = {"something": "else"}
        tests = manifest.active_tests(exists=False, filters=[subsuite("baz")], **info)
        self.assertEqual(len(tests), 1)
        tests = manifest.active_tests(exists=False, filters=[subsuite("baz")], **other)
        self.assertEqual(len(tests), 1)

        # 4 tests match when the condition doesn't match (all tests except
        # the unconditional subsuite)
        info = {"foo": "blah"}
        tests = manifest.active_tests(exists=False, filters=[subsuite()], **info)
        self.assertEqual(len(tests), 5)

        # test for illegal subsuite value
        manifest.tests[0]["subsuite"] = 'subsuite=bar,foo=="bar",type="nothing"'
        with self.assertRaises(ParseError):
            manifest.active_tests(exists=False, filters=[subsuite("foo")], **info)

    def test_none_and_empty_manifest(self):
        """
        Test TestManifest for None and empty manifest, see
        https://bugzilla.mozilla.org/show_bug.cgi?id=1087682
        """
        none_manifest = TestManifest(manifests=None, strict=False)
        self.assertEqual(len(none_manifest.test_paths()), 0)
        self.assertEqual(len(none_manifest.active_tests()), 0)

        empty_manifest = TestManifest(manifests=[], strict=False)
        self.assertEqual(len(empty_manifest.test_paths()), 0)
        self.assertEqual(len(empty_manifest.active_tests()), 0)

    def test_none_and_empty_manifest_toml(self):
        """
        Test TestManifest for None and empty manifest, see
        https://bugzilla.mozilla.org/show_bug.cgi?id=1087682
        (TOML)
        """
        none_manifest = TestManifest(manifests=None, strict=False, use_toml=True)
        self.assertEqual(len(none_manifest.test_paths()), 0)
        self.assertEqual(len(none_manifest.active_tests()), 0)

        empty_manifest = TestManifest(manifests=[], strict=False)
        self.assertEqual(len(empty_manifest.test_paths()), 0)
        self.assertEqual(len(empty_manifest.active_tests()), 0)


if __name__ == "__main__":
    mozunit.main()
