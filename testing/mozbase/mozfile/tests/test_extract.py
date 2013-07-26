#!/usr/bin/env python

import mozfile
import os
import shutil
import tarfile
import tempfile
import stubs
import unittest
import zipfile


class TestExtract(unittest.TestCase):
    """test extracting archives"""

    def ensure_directory_contents(self, directory):
        """ensure the directory contents match"""
        for f in stubs.files:
            path = os.path.join(directory, *f)
            exists = os.path.exists(path)
            if not exists:
                print "%s does not exist" % (os.path.join(f))
            self.assertTrue(exists)
            if exists:
                contents = file(path).read().strip()
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
        os.write(fd, 'This is not a zipfile or tarball')
        os.close(fd)
        exception = None
        try:
            dest = tempfile.mkdtemp()
            mozfile.extract(filename, dest)
        except Exception, exception:
            pass
        finally:
            os.remove(filename)
            os.rmdir(dest)
        self.assertTrue(isinstance(exception, Exception))

    ### utility functions

    def create_tarball(self):
        """create a stub tarball for testing"""
        tempdir = stubs.create_stub()
        filename = tempfile.mktemp(suffix='.tar')
        archive = tarfile.TarFile(filename, mode='w')
        try:
            for path in stubs.files:
                archive.add(os.path.join(tempdir, *path), arcname=os.path.join(*path))
        except:
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
        except:
            os.remove(filename)
            raise
        finally:
            shutil.rmtree(tempdir)
        archive.close()
        return filename
