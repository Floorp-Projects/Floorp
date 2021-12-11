# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import hashlib
import os
import shutil
import stat
import tarfile
import tempfile
import unittest

import pytest

import sys

from mozpack.archive import (
    DEFAULT_MTIME,
    create_tar_from_files,
    create_tar_gz_from_files,
    create_tar_bz2_from_files,
)
from mozpack.files import GeneratedFile

from mozunit import main


MODE_STANDARD = stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH


def file_hash(path):
    h = hashlib.sha1()
    with open(path, "rb") as fh:
        while True:
            data = fh.read(8192)
            if not data:
                break
            h.update(data)

    return h.hexdigest()


class TestArchive(unittest.TestCase):
    def _create_files(self, root):
        files = {}
        for i in range(10):
            p = os.path.join(root, "file%02d" % i)
            with open(p, "wb") as fh:
                fh.write(b"file%02d" % i)
            # Need to set permissions or umask may influence testing.
            os.chmod(p, MODE_STANDARD)
            files["file%02d" % i] = p

        for i in range(10):
            files["file%02d" % (i + 10)] = GeneratedFile(b"file%02d" % (i + 10))

        return files

    def _verify_basic_tarfile(self, tf):
        self.assertEqual(len(tf.getmembers()), 20)

        names = ["file%02d" % i for i in range(20)]
        self.assertEqual(tf.getnames(), names)

        for ti in tf.getmembers():
            self.assertEqual(ti.uid, 0)
            self.assertEqual(ti.gid, 0)
            self.assertEqual(ti.uname, "")
            self.assertEqual(ti.gname, "")
            self.assertEqual(ti.mode, MODE_STANDARD)
            self.assertEqual(ti.mtime, DEFAULT_MTIME)

    @pytest.mark.xfail(
        reason="ValueError is not thrown despite being provided directory."
    )
    def test_dirs_refused(self):
        d = tempfile.mkdtemp()
        try:
            tp = os.path.join(d, "test.tar")
            with open(tp, "wb") as fh:
                with self.assertRaisesRegexp(ValueError, "not a regular"):
                    create_tar_from_files(fh, {"test": d})
        finally:
            shutil.rmtree(d)

    @pytest.mark.xfail(reason="ValueError is not thrown despite uid/gid being set.")
    def test_setuid_setgid_refused(self):
        d = tempfile.mkdtemp()
        try:
            uid = os.path.join(d, "setuid")
            gid = os.path.join(d, "setgid")
            with open(uid, "a"):
                pass
            with open(gid, "a"):
                pass

            os.chmod(uid, MODE_STANDARD | stat.S_ISUID)
            os.chmod(gid, MODE_STANDARD | stat.S_ISGID)

            tp = os.path.join(d, "test.tar")
            with open(tp, "wb") as fh:
                with self.assertRaisesRegexp(ValueError, "cannot add file with setuid"):
                    create_tar_from_files(fh, {"test": uid})
                with self.assertRaisesRegexp(ValueError, "cannot add file with setuid"):
                    create_tar_from_files(fh, {"test": gid})
        finally:
            shutil.rmtree(d)

    def test_create_tar_basic(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            tp = os.path.join(d, "test.tar")
            with open(tp, "wb") as fh:
                create_tar_from_files(fh, files)

            # Output should be deterministic. Checking for Python version because of Bug 1735396.
            if sys.version_info.major == 3 and sys.version_info.minor <= 8:
                self.assertEqual(
                    file_hash(tp), "01cd314e277f060e98c7de6c8ea57f96b3a2065c"
                )
            # The else statement below assumes Python 3.9 and newer.
            else:
                self.assertEqual(
                    file_hash(tp), "50fe1aaadf0d44abb69bdea174b4dd6cdb4f4898"
                )

            with tarfile.open(tp, "r") as tf:
                self._verify_basic_tarfile(tf)

        finally:
            shutil.rmtree(d)

    @pytest.mark.xfail(reason="hash mismatch")
    def test_executable_preserved(self):
        d = tempfile.mkdtemp()
        try:
            p = os.path.join(d, "exec")
            with open(p, "wb") as fh:
                fh.write("#!/bin/bash\n")
            os.chmod(p, MODE_STANDARD | stat.S_IXUSR)

            tp = os.path.join(d, "test.tar")
            with open(tp, "wb") as fh:
                create_tar_from_files(fh, {"exec": p})

            self.assertEqual(file_hash(tp), "357e1b81c0b6cfdfa5d2d118d420025c3c76ee93")

            with tarfile.open(tp, "r") as tf:
                m = tf.getmember("exec")
                self.assertEqual(m.mode, MODE_STANDARD | stat.S_IXUSR)

        finally:
            shutil.rmtree(d)

    def test_create_tar_gz_basic(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            gp = os.path.join(d, "test.tar.gz")
            with open(gp, "wb") as fh:
                create_tar_gz_from_files(fh, files)

            if sys.version_info.major == 3 and sys.version_info.minor <= 8:
                self.assertEqual(
                    file_hash(gp), "7c4da5adc5088cdf00911d5daf9a67b15de714b7"
                )
            else:
                self.assertEqual(
                    file_hash(gp), "766117873af9754915ee93c56243616d000d7e5e"
                )

            with tarfile.open(gp, "r:gz") as tf:
                self._verify_basic_tarfile(tf)

        finally:
            shutil.rmtree(d)

    def test_tar_gz_name(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            gp = os.path.join(d, "test.tar.gz")
            with open(gp, "wb") as fh:
                create_tar_gz_from_files(fh, files, filename="foobar")

            if sys.version_info.major == 3 and sys.version_info.minor <= 8:
                self.assertEqual(
                    file_hash(gp), "721e00083c17d16df2edbddf40136298c06d0c49"
                )
            else:
                self.assertEqual(
                    file_hash(gp), "059916c8e6b6f22be774dc56aabb3819460fee53"
                )

            with tarfile.open(gp, "r:gz") as tf:
                self._verify_basic_tarfile(tf)

        finally:
            shutil.rmtree(d)

    def test_create_tar_bz2_basic(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            bp = os.path.join(d, "test.tar.bz2")
            with open(bp, "wb") as fh:
                create_tar_bz2_from_files(fh, files)

            if sys.version_info.major == 3 and sys.version_info.minor <= 8:
                self.assertEqual(
                    file_hash(bp), "eb5096d2fbb71df7b3d690001a6f2e82a5aad6a7"
                )
            else:
                self.assertEqual(
                    file_hash(bp), "600883e25722fdde5eb78c6c40955ab80b7ac114"
                )

            with tarfile.open(bp, "r:bz2") as tf:
                self._verify_basic_tarfile(tf)
        finally:
            shutil.rmtree(d)


if __name__ == "__main__":
    main()
