# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import unittest

from shutil import rmtree

from tempfile import (
    gettempdir,
    mkdtemp,
)

from mozboot.mozconfig import (
    MozconfigFindException,
    find_mozconfig,
    DEFAULT_TOPSRCDIR_PATHS,
    DEPRECATED_TOPSRCDIR_PATHS,
    DEPRECATED_HOME_PATHS,
)
from mozunit import main


class TestFindMozconfig(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop("MOZCONFIG", None)
        os.environ.pop("MOZ_OBJDIR", None)
        os.environ.pop("CC", None)
        os.environ.pop("CXX", None)
        self._temp_dirs = set()

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

        for d in self._temp_dirs:
            rmtree(d)

    def get_temp_dir(self):
        d = mkdtemp()
        self._temp_dirs.add(d)

        return d

    def test_find_legacy_env(self):
        """Ensure legacy mozconfig path definitions result in error."""

        os.environ["MOZ_MYCONFIG"] = "/foo"

        with self.assertRaises(MozconfigFindException) as e:
            find_mozconfig(self.get_temp_dir())

        self.assertTrue(str(e.exception).startswith("The MOZ_MYCONFIG"))

    def test_find_multiple_configs(self):
        """Ensure multiple relative-path MOZCONFIGs result in error."""
        relative_mozconfig = ".mconfig"
        os.environ["MOZCONFIG"] = relative_mozconfig

        srcdir = self.get_temp_dir()
        curdir = self.get_temp_dir()
        dirs = [srcdir, curdir]
        for d in dirs:
            path = os.path.join(d, relative_mozconfig)
            with open(path, "w") as f:
                f.write(path)

        orig_dir = os.getcwd()
        try:
            os.chdir(curdir)
            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(srcdir)
        finally:
            os.chdir(orig_dir)

        self.assertIn("exists in more than one of", str(e.exception))
        for d in dirs:
            self.assertIn(d, str(e.exception))

    def test_find_multiple_but_identical_configs(self):
        """Ensure multiple relative-path MOZCONFIGs pointing at the same file are OK."""
        relative_mozconfig = "../src/.mconfig"
        os.environ["MOZCONFIG"] = relative_mozconfig

        topdir = self.get_temp_dir()
        srcdir = os.path.join(topdir, "src")
        os.mkdir(srcdir)
        curdir = os.path.join(topdir, "obj")
        os.mkdir(curdir)

        path = os.path.join(srcdir, relative_mozconfig)
        with open(path, "w"):
            pass

        orig_dir = os.getcwd()
        try:
            os.chdir(curdir)
            self.assertEqual(
                os.path.realpath(find_mozconfig(srcdir)), os.path.realpath(path)
            )
        finally:
            os.chdir(orig_dir)

    def test_find_no_relative_configs(self):
        """Ensure a missing relative-path MOZCONFIG is detected."""
        relative_mozconfig = ".mconfig"
        os.environ["MOZCONFIG"] = relative_mozconfig

        srcdir = self.get_temp_dir()
        curdir = self.get_temp_dir()
        dirs = [srcdir, curdir]

        orig_dir = os.getcwd()
        try:
            os.chdir(curdir)
            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(srcdir)
        finally:
            os.chdir(orig_dir)

        self.assertIn("does not exist in any of", str(e.exception))
        for d in dirs:
            self.assertIn(d, str(e.exception))

    def test_find_relative_mozconfig(self):
        """Ensure a relative MOZCONFIG can be found in the srcdir."""
        relative_mozconfig = ".mconfig"
        os.environ["MOZCONFIG"] = relative_mozconfig

        srcdir = self.get_temp_dir()
        curdir = self.get_temp_dir()

        path = os.path.join(srcdir, relative_mozconfig)
        with open(path, "w"):
            pass

        orig_dir = os.getcwd()
        try:
            os.chdir(curdir)
            self.assertEqual(
                os.path.normpath(find_mozconfig(srcdir)), os.path.normpath(path)
            )
        finally:
            os.chdir(orig_dir)

    def test_find_abs_path_not_exist(self):
        """Ensure a missing absolute path is detected."""
        os.environ["MOZCONFIG"] = "/foo/bar/does/not/exist"

        with self.assertRaises(MozconfigFindException) as e:
            find_mozconfig(self.get_temp_dir())

        self.assertIn("path that does not exist", str(e.exception))
        self.assertTrue(str(e.exception).endswith("/foo/bar/does/not/exist"))

    def test_find_path_not_file(self):
        """Ensure non-file paths are detected."""

        os.environ["MOZCONFIG"] = gettempdir()

        with self.assertRaises(MozconfigFindException) as e:
            find_mozconfig(self.get_temp_dir())

        self.assertIn("refers to a non-file", str(e.exception))
        self.assertTrue(str(e.exception).endswith(gettempdir()))

    def test_find_default_files(self):
        """Ensure default paths are used when present."""
        for p in DEFAULT_TOPSRCDIR_PATHS:
            d = self.get_temp_dir()
            path = os.path.join(d, p)

            with open(path, "w"):
                pass

            self.assertEqual(find_mozconfig(d), path)

    def test_find_multiple_defaults(self):
        """Ensure we error when multiple default files are present."""
        self.assertGreater(len(DEFAULT_TOPSRCDIR_PATHS), 1)

        d = self.get_temp_dir()
        for p in DEFAULT_TOPSRCDIR_PATHS:
            with open(os.path.join(d, p), "w"):
                pass

        with self.assertRaises(MozconfigFindException) as e:
            find_mozconfig(d)

        self.assertIn("Multiple default mozconfig files present", str(e.exception))

    def test_find_deprecated_path_srcdir(self):
        """Ensure we error when deprecated path locations are present."""
        for p in DEPRECATED_TOPSRCDIR_PATHS:
            d = self.get_temp_dir()
            with open(os.path.join(d, p), "w"):
                pass

            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(d)

            self.assertIn("This implicit location is no longer", str(e.exception))
            self.assertIn(d, str(e.exception))

    def test_find_deprecated_home_paths(self):
        """Ensure we error when deprecated home directory paths are present."""

        for p in DEPRECATED_HOME_PATHS:
            home = self.get_temp_dir()
            os.environ["HOME"] = home
            path = os.path.join(home, p)

            with open(path, "w"):
                pass

            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(self.get_temp_dir())

            self.assertIn("This implicit location is no longer", str(e.exception))
            self.assertIn(path, str(e.exception))


if __name__ == "__main__":
    main()
