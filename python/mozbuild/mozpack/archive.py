# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import bz2
import gzip
import stat
import tarfile

from .files import (
    BaseFile, File,
)

# 2016-01-01T00:00:00+0000
DEFAULT_MTIME = 1451606400


def create_tar_from_files(fp, files):
    """Create a tar file deterministically.

    Receives a dict mapping names of files in the archive to local filesystem
    paths or ``mozpack.files.BaseFile`` instances.

    The files will be archived and written to the passed file handle opened
    for writing.

    Only regular files can be written.

    FUTURE accept a filename argument (or create APIs to write files)
    """
    # The format is explicitly set to tarfile.GNU_FORMAT, because this default format
    # has been changed in Python 3.8.
    with tarfile.open(name='', mode='w', fileobj=fp, dereference=True,
                      format=tarfile.GNU_FORMAT) as tf:
        for archive_path, f in sorted(files.items()):
            if not isinstance(f, BaseFile):
                f = File(f)

            ti = tarfile.TarInfo(archive_path)
            ti.mode = f.mode or 0o0644
            ti.type = tarfile.REGTYPE

            if not ti.isreg():
                raise ValueError('not a regular file: %s' % f)

            # Disallow setuid and setgid bits. This is an arbitrary restriction.
            # However, since we set uid/gid to root:root, setuid and setgid
            # would be a glaring security hole if the archive were
            # uncompressed as root.
            if ti.mode & (stat.S_ISUID | stat.S_ISGID):
                raise ValueError('cannot add file with setuid or setgid set: '
                                 '%s' % f)

            # Set uid, gid, username, and group as deterministic values.
            ti.uid = 0
            ti.gid = 0
            ti.uname = ''
            ti.gname = ''

            # Set mtime to a constant value.
            ti.mtime = DEFAULT_MTIME

            ti.size = f.size()
            # tarfile wants to pass a size argument to read(). So just
            # wrap/buffer in a proper file object interface.
            tf.addfile(ti, f.open())


def create_tar_gz_from_files(fp, files, filename=None, compresslevel=9):
    """Create a tar.gz file deterministically from files.

    This is a glorified wrapper around ``create_tar_from_files`` that
    adds gzip compression.

    The passed file handle should be opened for writing in binary mode.
    When the function returns, all data has been written to the handle.
    """
    # Offset 3-7 in the gzip header contains an mtime. Pin it to a known
    # value so output is deterministic.
    gf = gzip.GzipFile(filename=filename or '', mode='wb', fileobj=fp,
                       compresslevel=compresslevel, mtime=DEFAULT_MTIME)
    with gf:
        create_tar_from_files(gf, files)


class _BZ2Proxy(object):
    """File object that proxies writes to a bz2 compressor."""

    def __init__(self, fp, compresslevel=9):
        self.fp = fp
        self.compressor = bz2.BZ2Compressor(compresslevel)
        self.pos = 0

    def tell(self):
        return self.pos

    def write(self, data):
        data = self.compressor.compress(data)
        self.pos += len(data)
        self.fp.write(data)

    def close(self):
        data = self.compressor.flush()
        self.pos += len(data)
        self.fp.write(data)


def create_tar_bz2_from_files(fp, files, compresslevel=9):
    """Create a tar.bz2 file deterministically from files.

    This is a glorified wrapper around ``create_tar_from_files`` that
    adds bzip2 compression.

    This function is similar to ``create_tar_gzip_from_files()``.
    """
    proxy = _BZ2Proxy(fp, compresslevel=compresslevel)
    create_tar_from_files(proxy, files)
    proxy.close()
