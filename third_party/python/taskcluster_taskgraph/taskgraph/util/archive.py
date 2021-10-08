# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import gzip
import stat
import tarfile


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
    with tarfile.open(name="", mode="w", fileobj=fp, dereference=True) as tf:
        for archive_path, f in sorted(files.items()):
            if isinstance(f, str):
                mode = os.stat(f).st_mode
                f = open(f, "rb")
            else:
                mode = 0o0644

            ti = tarfile.TarInfo(archive_path)
            ti.mode = mode
            ti.type = tarfile.REGTYPE

            if not ti.isreg():
                raise ValueError("not a regular file: %s" % f)

            # Disallow setuid and setgid bits. This is an arbitrary restriction.
            # However, since we set uid/gid to root:root, setuid and setgid
            # would be a glaring security hole if the archive were
            # uncompressed as root.
            if ti.mode & (stat.S_ISUID | stat.S_ISGID):
                raise ValueError("cannot add file with setuid or setgid set: " "%s" % f)

            # Set uid, gid, username, and group as deterministic values.
            ti.uid = 0
            ti.gid = 0
            ti.uname = ""
            ti.gname = ""

            # Set mtime to a constant value.
            ti.mtime = DEFAULT_MTIME

            f.seek(0, 2)
            ti.size = f.tell()
            f.seek(0, 0)
            # tarfile wants to pass a size argument to read(). So just
            # wrap/buffer in a proper file object interface.
            tf.addfile(ti, f)


def create_tar_gz_from_files(fp, files, filename=None, compresslevel=9):
    """Create a tar.gz file deterministically from files.

    This is a glorified wrapper around ``create_tar_from_files`` that
    adds gzip compression.

    The passed file handle should be opened for writing in binary mode.
    When the function returns, all data has been written to the handle.
    """
    # Offset 3-7 in the gzip header contains an mtime. Pin it to a known
    # value so output is deterministic.
    gf = gzip.GzipFile(
        filename=filename or "",
        mode="wb",
        fileobj=fp,
        compresslevel=compresslevel,
        mtime=DEFAULT_MTIME,
    )
    with gf:
        create_tar_from_files(gf, files)
