# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains miscellaneous utility functions that don't belong anywhere
# in particular.

import errno
import os
import sys

if sys.platform == "win32":
    import ctypes

    _kernel32 = ctypes.windll.kernel32
    _FILE_ATTRIBUTE_NOT_CONTENT_INDEXED = 0x2000


def ensureParentDir(path):
    """Ensures the directory parent to the given file exists."""
    d = os.path.dirname(path)
    if d and not os.path.exists(path):
        try:
            os.makedirs(d)
        except OSError as error:
            if error.errno != errno.EEXIST:
                raise


def mkdir(path, not_indexed=False):
    """Ensure a directory exists.

    If ``not_indexed`` is True, an attribute is set that disables content
    indexing on the directory.
    """
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    if not_indexed:
        if sys.platform == "win32":
            if isinstance(path, str):
                fn = _kernel32.SetFileAttributesW
            else:
                fn = _kernel32.SetFileAttributesA

            fn(path, _FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
        elif sys.platform == "darwin":
            with open(os.path.join(path, ".metadata_never_index"), "a"):
                pass
