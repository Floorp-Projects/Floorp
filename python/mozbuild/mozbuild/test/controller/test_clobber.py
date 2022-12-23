# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile
import unittest

from mozunit import main

from mozbuild.base import MozbuildObject
from mozbuild.controller.building import BuildDriver
from mozbuild.controller.clobber import Clobberer
from mozbuild.test.common import prepare_tmp_topsrcdir


class TestClobberer(unittest.TestCase):
    def setUp(self):
        self._temp_dirs = []
        self._old_env = dict(os.environ)
        os.environ.pop("MOZCONFIG", None)
        os.environ.pop("MOZ_OBJDIR", None)

        return unittest.TestCase.setUp(self)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

        for d in self._temp_dirs:
            shutil.rmtree(d, ignore_errors=True)

        return unittest.TestCase.tearDown(self)

    def get_tempdir(self):
        t = tempfile.mkdtemp()
        self._temp_dirs.append(t)
        return t

    def get_topsrcdir(self):
        t = self.get_tempdir()
        prepare_tmp_topsrcdir(t)
        p = os.path.join(t, "CLOBBER")
        with open(p, "a"):
            pass

        return t

    def test_no_objdir(self):
        """If topobjdir does not exist, no clobber is needed."""

        tmp = os.path.join(self.get_tempdir(), "topobjdir")
        self.assertFalse(os.path.exists(tmp))

        c = Clobberer(self.get_topsrcdir(), tmp)
        self.assertFalse(c.clobber_needed())

        required, performed, reason = c.maybe_do_clobber(os.getcwd(), True)
        self.assertFalse(required)
        self.assertFalse(performed)
        self.assertIsNone(reason)

        self.assertFalse(os.path.isdir(tmp))
        self.assertFalse(os.path.exists(os.path.join(tmp, "CLOBBER")))

    def test_objdir_no_clobber_file(self):
        """If CLOBBER does not exist in topobjdir, treat as empty."""

        c = Clobberer(self.get_topsrcdir(), self.get_tempdir())
        self.assertFalse(c.clobber_needed())

        required, performed, reason = c.maybe_do_clobber(os.getcwd(), True)
        self.assertFalse(required)
        self.assertFalse(performed)
        self.assertIsNone(reason)

        self.assertFalse(os.path.exists(os.path.join(c.topobjdir, "CLOBBER")))

    def test_objdir_clobber_newer(self):
        """If CLOBBER in topobjdir is newer, do nothing."""

        c = Clobberer(self.get_topsrcdir(), self.get_tempdir())
        with open(c.obj_clobber, "a"):
            pass

        required, performed, reason = c.maybe_do_clobber(os.getcwd(), True)
        self.assertFalse(required)
        self.assertFalse(performed)
        self.assertIsNone(reason)

    def test_objdir_clobber_older(self):
        """If CLOBBER in topobjdir is older, we clobber."""

        c = Clobberer(self.get_topsrcdir(), self.get_tempdir())
        with open(c.obj_clobber, "a"):
            pass

        dummy_path = os.path.join(c.topobjdir, "foo")
        with open(dummy_path, "a"):
            pass

        self.assertTrue(os.path.exists(dummy_path))

        old_time = os.path.getmtime(c.src_clobber) - 60
        os.utime(c.obj_clobber, (old_time, old_time))

        self.assertTrue(c.clobber_needed())

        required, performed, reason = c.maybe_do_clobber(os.getcwd(), True)
        self.assertTrue(required)
        self.assertTrue(performed)

        self.assertFalse(os.path.exists(dummy_path))
        self.assertFalse(os.path.exists(c.obj_clobber))

    def test_objdir_is_srcdir(self):
        """If topobjdir is the topsrcdir, refuse to clobber."""

        tmp = self.get_topsrcdir()
        c = Clobberer(tmp, tmp)

        self.assertFalse(c.clobber_needed())

    def test_cwd_is_topobjdir(self):
        """If cwd is topobjdir, we can still clobber."""
        c = Clobberer(self.get_topsrcdir(), self.get_tempdir())

        with open(c.obj_clobber, "a"):
            pass

        dummy_file = os.path.join(c.topobjdir, "dummy_file")
        with open(dummy_file, "a"):
            pass

        dummy_dir = os.path.join(c.topobjdir, "dummy_dir")
        os.mkdir(dummy_dir)

        self.assertTrue(os.path.exists(dummy_file))
        self.assertTrue(os.path.isdir(dummy_dir))

        old_time = os.path.getmtime(c.src_clobber) - 60
        os.utime(c.obj_clobber, (old_time, old_time))

        self.assertTrue(c.clobber_needed())

        required, performed, reason = c.maybe_do_clobber(c.topobjdir, True)
        self.assertTrue(required)
        self.assertTrue(performed)

        self.assertFalse(os.path.exists(dummy_file))
        self.assertFalse(os.path.exists(dummy_dir))

    def test_cwd_under_topobjdir(self):
        """If cwd is under topobjdir, we can't clobber."""

        c = Clobberer(self.get_topsrcdir(), self.get_tempdir())

        with open(c.obj_clobber, "a"):
            pass

        old_time = os.path.getmtime(c.src_clobber) - 60
        os.utime(c.obj_clobber, (old_time, old_time))

        d = os.path.join(c.topobjdir, "dummy_dir")
        os.mkdir(d)

        required, performed, reason = c.maybe_do_clobber(d, True)
        self.assertTrue(required)
        self.assertFalse(performed)
        self.assertIn("Cannot clobber while the shell is inside", reason)

    def test_mozconfig_opt_in(self):
        """Auto clobber iff AUTOCLOBBER is in the environment."""

        topsrcdir = self.get_topsrcdir()
        topobjdir = self.get_tempdir()

        obj_clobber = os.path.join(topobjdir, "CLOBBER")
        with open(obj_clobber, "a"):
            pass

        dummy_file = os.path.join(topobjdir, "dummy_file")
        with open(dummy_file, "a"):
            pass

        self.assertTrue(os.path.exists(dummy_file))

        old_time = os.path.getmtime(os.path.join(topsrcdir, "CLOBBER")) - 60
        os.utime(obj_clobber, (old_time, old_time))

        # Check auto clobber is off by default
        env = dict(os.environ)
        if env.get("AUTOCLOBBER", False):
            del env["AUTOCLOBBER"]

        mbo = MozbuildObject(topsrcdir, None, None, topobjdir)
        build = mbo._spawn(BuildDriver)

        status = build._check_clobber(build.mozconfig, env)

        self.assertEqual(status, True)
        self.assertTrue(os.path.exists(dummy_file))

        # Check auto clobber opt-in works
        env["AUTOCLOBBER"] = "1"

        status = build._check_clobber(build.mozconfig, env)
        self.assertFalse(status)
        self.assertFalse(os.path.exists(dummy_file))


if __name__ == "__main__":
    main()
