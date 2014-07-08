#!/usr/bin/env python

import os
import stat
import shutil
import threading
import time
import unittest

import mozfile
import mozinfo

import stubs


def mark_readonly(path):
    """Removes all write permissions from given file/directory.

    :param path: path of directory/file of which modes must be changed
    """
    mode = os.stat(path)[stat.ST_MODE]
    os.chmod(path, mode & ~stat.S_IWUSR & ~stat.S_IWGRP & ~stat.S_IWOTH)


class FileOpenCloseThread(threading.Thread):
    """Helper thread for asynchronous file handling"""
    def __init__(self, path, delay, delete=False):
        threading.Thread.__init__(self)
        self.delay = delay
        self.path = path
        self.delete = delete

    def run(self):
        with open(self.path):
            time.sleep(self.delay)
        if self.delete:
            try:
                os.remove(self.path)
            except:
                pass


class MozfileRemoveTestCase(unittest.TestCase):
    """Test our ability to remove directories and files"""

    def setUp(self):
        # Generate a stub
        self.tempdir = stubs.create_stub()

    def tearDown(self):
        if os.path.isdir(self.tempdir):
            shutil.rmtree(self.tempdir)

    def test_remove_directory(self):
        """Test the removal of a directory"""
        self.assertTrue(os.path.isdir(self.tempdir))
        mozfile.remove(self.tempdir)
        self.assertFalse(os.path.exists(self.tempdir))

    def test_remove_directory_with_open_file(self):
        """Test removing a directory with an open file"""
        # Open a file in the generated stub
        filepath = os.path.join(self.tempdir, *stubs.files[1])
        f = file(filepath, 'w')
        f.write('foo-bar')

        # keep file open and then try removing the dir-tree
        if mozinfo.isWin:
            # On the Windows family WindowsError should be raised.
            self.assertRaises(OSError, mozfile.remove, self.tempdir)
            self.assertTrue(os.path.exists(self.tempdir))
        else:
            # Folder should be deleted on all other platforms
            mozfile.remove(self.tempdir)
            self.assertFalse(os.path.exists(self.tempdir))

    def test_remove_closed_file(self):
        """Test removing a closed file"""
        # Open a file in the generated stub
        filepath = os.path.join(self.tempdir, *stubs.files[1])
        with open(filepath, 'w') as f:
            f.write('foo-bar')

        # Folder should be deleted on all platforms
        mozfile.remove(self.tempdir)
        self.assertFalse(os.path.exists(self.tempdir))

    def test_removing_open_file_with_retry(self):
        """Test removing a file in use with retry"""
        filepath = os.path.join(self.tempdir, *stubs.files[1])

        thread = FileOpenCloseThread(filepath, 1)
        thread.start()

        # Wait a bit so we can be sure the file has been opened
        time.sleep(.5)
        mozfile.remove(filepath)
        thread.join()

        # Check deletion was successful
        self.assertFalse(os.path.exists(filepath))

    def test_removing_already_deleted_file_with_retry(self):
        """Test removing a meanwhile removed file with retry"""
        filepath = os.path.join(self.tempdir, *stubs.files[1])

        thread = FileOpenCloseThread(filepath, .8, True)
        thread.start()

        # Wait a bit so we can be sure the file has been opened and gets deleted
        # while remove() waits for the next retry
        time.sleep(.5)
        mozfile.remove(filepath)
        thread.join()

        # Check deletion was successful
        self.assertFalse(os.path.exists(filepath))

    def test_remove_readonly_tree(self):
        """Test removing a read-only directory"""

        dirpath = os.path.join(self.tempdir, "nested_tree")
        mark_readonly(dirpath)

        # However, mozfile should change write permissions and remove dir.
        mozfile.remove(dirpath)

        self.assertFalse(os.path.exists(dirpath))

    def test_remove_readonly_file(self):
        """Test removing read-only files"""
        filepath = os.path.join(self.tempdir, *stubs.files[1])
        mark_readonly(filepath)

        # However, mozfile should change write permission and then remove file.
        mozfile.remove(filepath)

        self.assertFalse(os.path.exists(filepath))

    @unittest.skipIf(mozinfo.isWin, "Symlinks are not supported on Windows")
    def test_remove_symlink(self):
        """Test removing a symlink"""
        file_path = os.path.join(self.tempdir, *stubs.files[1])
        symlink_path = os.path.join(self.tempdir, 'symlink')

        os.symlink(file_path, symlink_path)
        self.assertTrue(os.path.islink(symlink_path))

        # The linked folder and files should not be deleted
        mozfile.remove(symlink_path)
        self.assertFalse(os.path.exists(symlink_path))
        self.assertTrue(os.path.exists(file_path))

    @unittest.skipIf(mozinfo.isWin, "Symlinks are not supported on Windows")
    def test_remove_symlink_in_subfolder(self):
        """Test removing a folder with an contained symlink"""
        file_path = os.path.join(self.tempdir, *stubs.files[0])
        dir_path = os.path.dirname(os.path.join(self.tempdir, *stubs.files[1]))
        symlink_path = os.path.join(dir_path, 'symlink')

        os.symlink(file_path, symlink_path)
        self.assertTrue(os.path.islink(symlink_path))

        # The folder with the contained symlink will be deleted but not the
        # original linked file
        mozfile.remove(dir_path)
        self.assertFalse(os.path.exists(dir_path))
        self.assertFalse(os.path.exists(symlink_path))
        self.assertTrue(os.path.exists(file_path))

    @unittest.skipIf(mozinfo.isWin or not os.geteuid(),
                     "Symlinks are not supported on Windows and cannot run test as root")
    def test_remove_symlink_for_system_path(self):
        """Test removing a symlink which points to a system folder"""
        symlink_path = os.path.join(self.tempdir, 'symlink')

        os.symlink(os.path.dirname(self.tempdir), symlink_path)
        self.assertTrue(os.path.islink(symlink_path))

        # The folder with the contained symlink will be deleted but not the
        # original linked file
        mozfile.remove(symlink_path)
        self.assertFalse(os.path.exists(symlink_path))
