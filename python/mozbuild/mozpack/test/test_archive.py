# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import hashlib
import os
import shutil
import stat
import tarfile
import tempfile
import unittest

from mozpack.archive import (
    DEFAULT_MTIME,
    create_tar_from_files,
    create_tar_gz_from_files,
    create_tar_bz2_from_files,
)

from mozunit import main


MODE_STANDARD = stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH


def file_hash(path):
    h = hashlib.sha1()
    with open(path, 'rb') as fh:
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
            p = os.path.join(root, b'file%d' % i)
            with open(p, 'wb') as fh:
                fh.write(b'file%d' % i)
            # Need to set permissions or umask may influence testing.
            os.chmod(p, MODE_STANDARD)
            files[b'file%d' % i] = p

        return files

    def _verify_basic_tarfile(self, tf):
        self.assertEqual(len(tf.getmembers()), 10)

        names = ['file%d' % i for i in range(10)]
        self.assertEqual(tf.getnames(), names)

        for ti in tf.getmembers():
            self.assertEqual(ti.uid, 0)
            self.assertEqual(ti.gid, 0)
            self.assertEqual(ti.uname, '')
            self.assertEqual(ti.gname, '')
            self.assertEqual(ti.mode, MODE_STANDARD)
            self.assertEqual(ti.mtime, DEFAULT_MTIME)

    def test_dirs_refused(self):
        d = tempfile.mkdtemp()
        try:
            tp = os.path.join(d, 'test.tar')
            with open(tp, 'wb') as fh:
                with self.assertRaisesRegexp(ValueError, 'not a regular'):
                    create_tar_from_files(fh, {'test': d})
        finally:
            shutil.rmtree(d)

    def test_setuid_setgid_refused(self):
        d = tempfile.mkdtemp()
        try:
            uid = os.path.join(d, 'setuid')
            gid = os.path.join(d, 'setgid')
            with open(uid, 'a'):
                pass
            with open(gid, 'a'):
                pass

            os.chmod(uid, MODE_STANDARD | stat.S_ISUID)
            os.chmod(gid, MODE_STANDARD | stat.S_ISGID)

            tp = os.path.join(d, 'test.tar')
            with open(tp, 'wb') as fh:
                with self.assertRaisesRegexp(ValueError, 'cannot add file with setuid'):
                    create_tar_from_files(fh, {'test': uid})
                with self.assertRaisesRegexp(ValueError, 'cannot add file with setuid'):
                    create_tar_from_files(fh, {'test': gid})
        finally:
            shutil.rmtree(d)

    def test_create_tar_basic(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            tp = os.path.join(d, 'test.tar')
            with open(tp, 'wb') as fh:
                create_tar_from_files(fh, files)

            # Output should be deterministic.
            self.assertEqual(file_hash(tp), 'cd16cee6f13391abd94dfa435d2633b61ed727f1')

            with tarfile.open(tp, 'r') as tf:
                self._verify_basic_tarfile(tf)

        finally:
            shutil.rmtree(d)

    def test_executable_preserved(self):
        d = tempfile.mkdtemp()
        try:
            p = os.path.join(d, 'exec')
            with open(p, 'wb') as fh:
                fh.write('#!/bin/bash\n')
            os.chmod(p, MODE_STANDARD | stat.S_IXUSR)

            tp = os.path.join(d, 'test.tar')
            with open(tp, 'wb') as fh:
                create_tar_from_files(fh, {'exec': p})

            self.assertEqual(file_hash(tp), '357e1b81c0b6cfdfa5d2d118d420025c3c76ee93')

            with tarfile.open(tp, 'r') as tf:
                m = tf.getmember('exec')
                self.assertEqual(m.mode, MODE_STANDARD | stat.S_IXUSR)

        finally:
            shutil.rmtree(d)

    def test_create_tar_gz_basic(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            gp = os.path.join(d, 'test.tar.gz')
            with open(gp, 'wb') as fh:
                create_tar_gz_from_files(fh, files)

            self.assertEqual(file_hash(gp), 'acb602239c1aeb625da5e69336775609516d60f5')

            with tarfile.open(gp, 'r:gz') as tf:
                self._verify_basic_tarfile(tf)

        finally:
            shutil.rmtree(d)

    def test_tar_gz_name(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            gp = os.path.join(d, 'test.tar.gz')
            with open(gp, 'wb') as fh:
                create_tar_gz_from_files(fh, files, filename='foobar', compresslevel=1)

            self.assertEqual(file_hash(gp), 'fd099f96480cc1100f37baa8e89a6b820dbbcbd3')

            with tarfile.open(gp, 'r:gz') as tf:
                self._verify_basic_tarfile(tf)

        finally:
            shutil.rmtree(d)

    def test_create_tar_bz2_basic(self):
        d = tempfile.mkdtemp()
        try:
            files = self._create_files(d)

            bp = os.path.join(d, 'test.tar.bz2')
            with open(bp, 'wb') as fh:
                create_tar_bz2_from_files(fh, files)

            self.assertEqual(file_hash(bp), '1827ad00dfe7acf857b7a1c95ce100361e3f6eea')

            with tarfile.open(bp, 'r:bz2') as tf:
                self._verify_basic_tarfile(tf)
        finally:
            shutil.rmtree(d)


if __name__ == '__main__':
    main()
