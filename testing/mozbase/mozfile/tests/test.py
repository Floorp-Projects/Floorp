#!/usr/bin/env python

"""
tests for mozfile
"""

import mozfile
import os
import shutil
import tarfile
import tempfile
import unittest
import zipfile

# stub file paths
files = [('foo.txt',),
         ('foo', 'bar.txt'),
         ('foo', 'bar', 'fleem.txt'),
         ('foobar', 'fleem.txt'),
         ('bar.txt')]

def create_stub():
    """create a stub directory"""

    tempdir = tempfile.mkdtemp()
    try:
        for path in files:
            fullpath = os.path.join(tempdir, *path)
            dirname = os.path.dirname(fullpath)
            if not os.path.exists(dirname):
                os.makedirs(dirname)
            contents = path[-1]
            f = file(fullpath, 'w')
            f.write(contents)
            f.close()
        return tempdir
    except Exception, e:
        try:
            shutil.rmtree(tempdir)
        except:
            pass
        raise e


class TestExtract(unittest.TestCase):
    """test extracting archives"""

    def ensure_directory_contents(self, directory):
        """ensure the directory contents match"""
        for f in files:
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
        tempdir = create_stub()
        filename = tempfile.mktemp(suffix='.tar')
        archive = tarfile.TarFile(filename, mode='w')
        try:
            for path in files:
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

        tempdir = create_stub()
        filename = tempfile.mktemp(suffix='.zip')
        archive = zipfile.ZipFile(filename, mode='w')
        try:
            for path in files:
                archive.write(os.path.join(tempdir, *path), arcname=os.path.join(*path))
        except:
            os.remove(filename)
            raise
        finally:
            shutil.rmtree(tempdir)
        archive.close()
        return filename

class TestRemoveTree(unittest.TestCase):
    """test our ability to remove a directory tree"""

    def test_remove_directory(self):
        tempdir = create_stub()
        self.assertTrue(os.path.exists(tempdir))
        self.assertTrue(os.path.isdir(tempdir))
        try:
            mozfile.rmtree(tempdir)
        except:
            shutil.rmtree(tempdir)
            raise
        self.assertFalse(os.path.exists(tempdir))

class TestNamedTemporaryFile(unittest.TestCase):
    """test our fix for NamedTemporaryFile"""

    def test_named_temporary_file(self):
        temp = mozfile.NamedTemporaryFile()
        temp.write("A simple test")
        del temp

if __name__ == '__main__':
    unittest.main()
