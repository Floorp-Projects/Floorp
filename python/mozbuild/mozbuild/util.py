# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains miscellaneous utility functions that don't belong anywhere
# in particular.

from __future__ import unicode_literals

import copy
import errno
import hashlib
import os

from StringIO import StringIO


def hash_file(path):
    """Hashes a file specified by the path given and returns the hex digest."""

    # If the hashing function changes, this may invalidate lots of cached data.
    # Don't change it lightly.
    h = hashlib.sha1()

    with open(path, 'rb') as fh:
        while True:
            data = fh.read(8192)

            if not len(data):
                break

            h.update(data)

    return h.hexdigest()


class ReadOnlyDict(dict):
    """A read-only dictionary."""
    def __init__(self, d):
        dict.__init__(self, d)

    def __setitem__(self, name, value):
        raise Exception('Object does not support assignment.')


class undefined_default(object):
    """Represents an undefined argument value that isn't None."""


undefined = undefined_default()


class DefaultOnReadDict(dict):
    """A dictionary that returns default values for missing keys on read."""

    def __init__(self, d, defaults=None, global_default=undefined):
        """Create an instance from an iterable with defaults.

        The first argument is fed into the dict constructor.

        defaults is a dict mapping keys to their default values.

        global_default is the default value for *all* missing keys. If it isn't
        specified, no default value for keys not in defaults will be used and
        IndexError will be raised on access.
        """
        dict.__init__(self, d)

        self._defaults = defaults or {}
        self._global_default = global_default

    def __getitem__(self, k):
        try:
            return dict.__getitem__(self, k)
        except:
            pass

        if k in self._defaults:
            dict.__setitem__(self, k, copy.deepcopy(self._defaults[k]))
        elif self._global_default != undefined:
            dict.__setitem__(self, k, copy.deepcopy(self._global_default))

        return dict.__getitem__(self, k)


class ReadOnlyDefaultDict(DefaultOnReadDict, ReadOnlyDict):
    """A read-only dictionary that supports default values on retrieval."""
    def __init__(self, d, defaults=None, global_default=undefined):
        DefaultOnReadDict.__init__(self, d, defaults, global_default)


def ensureParentDir(path):
    """Ensures the directory parent to the given file exists."""
    d = os.path.dirname(path)
    if d and not os.path.exists(path):
        try:
            os.makedirs(d)
        except OSError, error:
            if error.errno != errno.EEXIST:
                raise


class FileAvoidWrite(StringIO):
    """File-like object that buffers output and only writes if content changed.

    We create an instance from an existing filename. New content is written to
    it. When we close the file object, if the content in the in-memory buffer
    differs from what is on disk, then we write out the new content. Otherwise,
    the original file is untouched.
    """
    def __init__(self, filename):
        StringIO.__init__(self)
        self.filename = filename

    def close(self):
        """Stop accepting writes, compare file contents, and rewrite if needed.

        Returns a tuple of bools indicating what action was performed:

            (file existed, file updated)
        """
        buf = self.getvalue()
        StringIO.close(self)
        existed = False
        try:
            existing = open(self.filename, 'rU')
            existed = True
        except IOError:
            pass
        else:
            try:
                if existing.read() == buf:
                    return True, False
            except IOError:
                pass
            finally:
                existing.close()

        ensureParentDir(self.filename)
        with open(self.filename, 'w') as file:
            file.write(buf)

        return existed, True

    def __enter__(self):
        return self
    def __exit__(self, type, value, traceback):
        self.close()

