import os
import stat

from collections import namedtuple
from glob import glob

from dlmanager import fs


File = namedtuple('File', ('path', 'stat'))


class PersistLimit(object):
    """
    Keep a list of files, removing the oldest ones when the size_limit
    is reached.

    The access time of a file is used to determine the oldests, e.g. the
    last time a file was read.

    :param size_limit: the size limit in bytes. A value of 0 means no limit.
    :param file_limit: even if the size limit is reached, this force
                       to keep at least *file_limit* files.
    """
    def __init__(self, size_limit, file_limit=5):
        self.size_limit = size_limit
        self.file_limit = file_limit
        self.files = []
        self._files_size = 0

    def register_file(self, path):
        """
        register a single file.
        """
        try:
            fstat = os.stat(path)
        except OSError:
            # file do not exists probably, just skip it
            # note this happen when backgound files are canceled
            return
        if stat.S_ISREG(fstat.st_mode):
            self.files.append(File(path=path, stat=fstat))
            self._files_size += fstat.st_size

    def register_dir_content(self, directory, pattern="*"):
        """
        Register every files in a directory that match *pattern*.
        """
        for path in glob(os.path.join(directory, pattern)):
            self.register_file(path)

    def remove_old_files(self):
        """
        remove oldest registered files.
        """
        if self.size_limit <= 0 or self.file_limit <= 0:
            return
        # sort by creation time, oldest first
        files = sorted(self.files, key=lambda f: f.stat.st_atime)
        while len(files) > self.file_limit and \
                self._files_size >= self.size_limit:
            f = files.pop(0)
            fs.remove(f.path)
            self._files_size -= f.stat.st_size
        self.files = files
