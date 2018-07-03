#!/usr/bin/env python

from __future__ import absolute_import, print_function

import os
import shutil
import tarfile
import tempfile
import unittest
import zipfile

import mozunit

import mozfile

import stubs


class TestExtract(unittest.TestCase):
    """test extracting archives"""

    def ensure_directory_contents(self, directory):
        """ensure the directory contents match"""
        for f in stubs.files:
            path = os.path.join(directory, *f)
            exists = os.path.exists(path)
            if not exists:
                print("%s does not exist" % (os.path.join(f)))
            self.assertTrue(exists)
            if exists:
                contents = open(path).read().strip()
                self.assertTrue(contents == f[-1])

    def test_extract_zipfile(self):
        """test extracting a zipfile"""
        _zipfile = self.create_zip()
        self.assertTrue(os.path.exists(_zipfile))
        try:
            dest = tempfile.mkdtemp()
            try:
                mozfile.extract_zip(_zipfile, dest)
                self.ensure_directory_contents(dest)
            finally:
                shutil.rmtree(dest)
        finally:
            os.remove(_zipfile)

    def test_extract_zipfile_missing_file_attributes(self):
        """if files do not have attributes set the default permissions have to be inherited."""
        _zipfile = os.path.join(os.path.dirname(__file__), 'files', 'missing_file_attributes.zip')
        self.assertTrue(os.path.exists(_zipfile))
        dest = tempfile.mkdtemp()
        try:
            # Get the default file permissions for the user
            fname = os.path.join(dest, 'foo')
            with open(fname, 'w'):
                pass
            default_stmode = os.stat(fname).st_mode

            files = mozfile.extract_zip(_zipfile, dest)
            for filename in files:
                self.assertEqual(os.stat(os.path.join(dest, filename)).st_mode,
                                 default_stmode)
        finally:
            shutil.rmtree(dest)

    def test_extract_tarball(self):
        """test extracting a tarball"""
        tarball = self.create_tarball()
        self.assertTrue(os.path.exists(tarball))
        try:
            dest = tempfile.mkdtemp()
            try:
                mozfile.extract_tarball(tarball, dest)
                self.ensure_directory_contents(dest)
            finally:
                shutil.rmtree(dest)
        finally:
            os.remove(tarball)

    def test_extract(self):
        """test the generalized extract function"""

        # test extracting a tarball
        tarball = self.create_tarball()
        self.assertTrue(os.path.exists(tarball))
        try:
            dest = tempfile.mkdtemp()
            try:
                mozfile.extract(tarball, dest)
                self.ensure_directory_contents(dest)
            finally:
                shutil.rmtree(dest)
        finally:
            os.remove(tarball)

        # test extracting a zipfile
        _zipfile = self.create_zip()
        self.assertTrue(os.path.exists(_zipfile))
        try:
            dest = tempfile.mkdtemp()
            try:
                mozfile.extract_zip(_zipfile, dest)
                self.ensure_directory_contents(dest)
            finally:
                shutil.rmtree(dest)
        finally:
            os.remove(_zipfile)

        # test extracting some non-archive; this should fail
        fd, filename = tempfile.mkstemp()
        os.write(fd, b'This is not a zipfile or tarball')
        os.close(fd)
        exception = None

        try:
            dest = tempfile.mkdtemp()
            mozfile.extract(filename, dest)
        except Exception as exc:
            exception = exc
        finally:
            os.remove(filename)
            os.rmdir(dest)

        self.assertTrue(isinstance(exception, Exception))

    # utility functions

    def create_tarball(self):
        """create a stub tarball for testing"""
        tempdir = stubs.create_stub()
        filename = tempfile.mktemp(suffix='.tar')
        archive = tarfile.TarFile(filename, mode='w')
        try:
            for path in stubs.files:
                archive.add(os.path.join(tempdir, *path), arcname=os.path.join(*path))
        except BaseException:
            os.remove(archive)
            raise
        finally:
            shutil.rmtree(tempdir)
        archive.close()
        return filename

    def create_zip(self):
        """create a stub zipfile for testing"""

        tempdir = stubs.create_stub()
        filename = tempfile.mktemp(suffix='.zip')
        archive = zipfile.ZipFile(filename, mode='w')
        try:
            for path in stubs.files:
                archive.write(os.path.join(tempdir, *path), arcname=os.path.join(*path))
        except BaseException:
            os.remove(filename)
            raise
        finally:
            shutil.rmtree(tempdir)
        archive.close()
        return filename


if __name__ == '__main__':
    mozunit.main()
