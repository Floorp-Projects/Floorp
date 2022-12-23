# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import unittest
from pathlib import Path
from shutil import rmtree
from tempfile import gettempdir, mkdtemp

import pytest
from mozunit import main

from mozboot.mozconfig import (
    DEFAULT_TOPSRCDIR_PATHS,
    DEPRECATED_HOME_PATHS,
    DEPRECATED_TOPSRCDIR_PATHS,
    MozconfigFindException,
    find_mozconfig,
)


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

        for temp_dir in self._temp_dirs:
            rmtree(str(temp_dir))

    def get_temp_dir(self):
        new_temp_dir = Path(mkdtemp())
        self._temp_dirs.add(new_temp_dir)

        return new_temp_dir

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

        src_dir = self.get_temp_dir()
        cur_dir = self.get_temp_dir()
        dirs = [src_dir, cur_dir]
        for iter_dir in dirs:
            path = iter_dir / relative_mozconfig
            with open(path, "w") as file:
                file.write(str(path))

        orig_dir = Path.cwd()
        try:
            os.chdir(cur_dir)
            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(src_dir)
        finally:
            os.chdir(orig_dir)

        self.assertIn("exists in more than one of", str(e.exception))
        for iter_dir in dirs:
            self.assertIn(str(iter_dir.resolve()), str(e.exception))

    def test_find_multiple_but_identical_configs(self):
        """Ensure multiple relative-path MOZCONFIGs pointing at the same file are OK."""
        relative_mozconfig = "../src/.mconfig"
        os.environ["MOZCONFIG"] = relative_mozconfig

        top_dir = self.get_temp_dir()
        src_dir = top_dir / "src"
        src_dir.mkdir()
        cur_dir = top_dir / "obj"
        cur_dir.mkdir()

        path = src_dir / relative_mozconfig
        with open(path, "w"):
            pass

        orig_dir = Path.cwd()
        try:
            os.chdir(cur_dir)
            self.assertEqual(Path(find_mozconfig(src_dir)).resolve(), path.resolve())
        finally:
            os.chdir(orig_dir)

    def test_find_no_relative_configs(self):
        """Ensure a missing relative-path MOZCONFIG is detected."""
        relative_mozconfig = ".mconfig"
        os.environ["MOZCONFIG"] = relative_mozconfig

        src_dir = self.get_temp_dir()
        cur_dir = self.get_temp_dir()
        dirs = [src_dir, cur_dir]

        orig_dir = Path.cwd()
        try:
            os.chdir(cur_dir)
            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(src_dir)
        finally:
            os.chdir(orig_dir)

        self.assertIn("does not exist in any of", str(e.exception))
        for iter_dir in dirs:
            self.assertIn(str(iter_dir.resolve()), str(e.exception))

    def test_find_relative_mozconfig(self):
        """Ensure a relative MOZCONFIG can be found in the srcdir."""
        relative_mozconfig = ".mconfig"
        os.environ["MOZCONFIG"] = relative_mozconfig

        src_dir = Path(self.get_temp_dir())
        cur_dir = Path(self.get_temp_dir())

        path = src_dir / relative_mozconfig
        with open(path, "w"):
            pass

        orig_dir = Path.cwd()
        try:
            os.chdir(cur_dir)
            self.assertEqual(
                str(Path(find_mozconfig(src_dir)).resolve()), str(path.resolve())
            )
        finally:
            os.chdir(orig_dir)

    @pytest.mark.skipif(
        sys.platform.startswith("win"),
        reason="This test uses unix-style absolute paths, since we now use Pathlib, and "
        "`is_absolute()` always returns `False` on Windows if there isn't a drive"
        " letter, this test is invalid for Windows.",
    )
    def test_find_abs_path_not_exist(self):
        """Ensure a missing absolute path is detected."""
        os.environ["MOZCONFIG"] = "/foo/bar/does/not/exist"

        with self.assertRaises(MozconfigFindException) as e:
            find_mozconfig(self.get_temp_dir())

        self.assertIn("path that does not exist", str(e.exception))
        self.assertIn("/foo/bar/does/not/exist", str(e.exception))

    def test_find_path_not_file(self):
        """Ensure non-file paths are detected."""

        os.environ["MOZCONFIG"] = gettempdir()

        with self.assertRaises(MozconfigFindException) as e:
            find_mozconfig(self.get_temp_dir())

        self.assertIn("refers to a non-file", str(e.exception))
        self.assertTrue(str(e.exception).endswith(gettempdir()))

    def test_find_default_files(self):
        """Ensure default paths are used when present."""
        for default_dir in DEFAULT_TOPSRCDIR_PATHS:
            temp_dir = self.get_temp_dir()
            path = temp_dir / default_dir

            with open(path, "w"):
                pass

            self.assertEqual(Path(find_mozconfig(temp_dir)), path)

    def test_find_multiple_defaults(self):
        """Ensure we error when multiple default files are present."""
        self.assertGreater(len(DEFAULT_TOPSRCDIR_PATHS), 1)

        temp_dir = self.get_temp_dir()
        for default_dir in DEFAULT_TOPSRCDIR_PATHS:
            with open(temp_dir / default_dir, "w"):
                pass

        with self.assertRaises(MozconfigFindException) as e:
            find_mozconfig(temp_dir)

        self.assertIn("Multiple default mozconfig files present", str(e.exception))

    def test_find_deprecated_path_srcdir(self):
        """Ensure we error when deprecated path locations are present."""
        for deprecated_dir in DEPRECATED_TOPSRCDIR_PATHS:
            temp_dir = self.get_temp_dir()
            with open(temp_dir / deprecated_dir, "w"):
                pass

            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(temp_dir)

            self.assertIn("This implicit location is no longer", str(e.exception))
            self.assertIn(str(temp_dir), str(e.exception))

    def test_find_deprecated_home_paths(self):
        """Ensure we error when deprecated home directory paths are present."""

        for deprecated_path in DEPRECATED_HOME_PATHS:
            home = self.get_temp_dir()
            os.environ["HOME"] = str(home)
            path = home / deprecated_path

            with open(path, "w"):
                pass

            with self.assertRaises(MozconfigFindException) as e:
                find_mozconfig(self.get_temp_dir())

            self.assertIn("This implicit location is no longer", str(e.exception))
            self.assertIn(str(path), str(e.exception))


if __name__ == "__main__":
    main()
