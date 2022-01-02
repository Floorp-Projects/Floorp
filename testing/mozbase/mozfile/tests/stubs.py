from __future__ import absolute_import

import os
import shutil
import tempfile


# stub file paths
files = [
    ("foo.txt",),
    (
        "foo",
        "bar.txt",
    ),
    (
        "foo",
        "bar",
        "fleem.txt",
    ),
    (
        "foobar",
        "fleem.txt",
    ),
    ("bar.txt",),
    (
        "nested_tree",
        "bar",
        "fleem.txt",
    ),
    ("readonly.txt",),
]


def create_empty_stub():
    tempdir = tempfile.mkdtemp()
    return tempdir


def create_stub(tempdir=None):
    """create a stub directory"""

    tempdir = tempdir or tempfile.mkdtemp()
    try:
        for path in files:
            fullpath = os.path.join(tempdir, *path)
            dirname = os.path.dirname(fullpath)
            if not os.path.exists(dirname):
                os.makedirs(dirname)
            contents = path[-1]
            f = open(fullpath, "w")
            f.write(contents)
            f.close()
        return tempdir
    except Exception:
        try:
            shutil.rmtree(tempdir)
        except Exception:
            pass
        raise
