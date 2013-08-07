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
import sys

from StringIO import StringIO

if sys.version_info[0] == 3:
    str_type = str
else:
    str_type = basestring

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


def resolve_target_to_make(topobjdir, target):
    r'''
    Resolve `target` (a target, directory, or file) to a make target.

    `topobjdir` is the object directory; all make targets will be
    rooted at or below the top-level Makefile in this directory.

    Returns a pair `(reldir, target)` where `reldir` is a directory
    relative to `topobjdir` containing a Makefile and `target` is a
    make target (possibly `None`).

    A directory resolves to the nearest directory at or above
    containing a Makefile, and target `None`.

    A regular (non-Makefile) file resolves to the nearest directory at
    or above the file containing a Makefile, and an appropriate
    target.

    A Makefile resolves to the nearest parent strictly above the
    Makefile containing a different Makefile, and an appropriate
    target.
    '''

    target = target.replace(os.sep, '/').lstrip('/')
    abs_target = os.path.join(topobjdir, target)

    # For directories, run |make -C dir|. If the directory does not
    # contain a Makefile, check parents until we find one. At worst,
    # this will terminate at the root.
    if os.path.isdir(abs_target):
        current = abs_target

        while True:
            make_path = os.path.join(current, 'Makefile')
            if os.path.exists(make_path):
                return (current[len(topobjdir) + 1:], None)

            current = os.path.dirname(current)

    # If it's not in a directory, this is probably a top-level make
    # target. Treat it as such.
    if '/' not in target:
        return (None, target)

    # We have a relative path within the tree. We look for a Makefile
    # as far into the path as possible. Then, we compute the make
    # target as relative to that directory.
    reldir = os.path.dirname(target)
    target = os.path.basename(target)

    while True:
        make_path = os.path.join(topobjdir, reldir, 'Makefile')

        # We append to target every iteration, so the check below
        # happens exactly once.
        if target != 'Makefile' and os.path.exists(make_path):
            return (reldir, target)

        target = os.path.join(os.path.basename(reldir), target)
        reldir = os.path.dirname(reldir)


class UnsortedError(Exception):
    def __init__(self, srtd, original):
        assert len(srtd) == len(original)

        self.sorted = srtd
        self.original = original

        for i, orig in enumerate(original):
            s = srtd[i]

            if orig != s:
                self.i = i
                break

    def __str__(self):
        s = StringIO()

        s.write('An attempt was made to add an unsorted sequence to a list. ')
        s.write('The incoming list is unsorted starting at element %d. ' %
            self.i)
        s.write('We expected "%s" but got "%s"' % (
            self.sorted[self.i], self.original[self.i]))

        return s.getvalue()


class StrictOrderingOnAppendList(list):
    """A list specialized for moz.build environments.

    We overload the assignment and append operations to require that incoming
    elements be ordered. This enforces cleaner style in moz.build files.
    """
    @staticmethod
    def ensure_sorted(l):
        srtd = sorted(l)

        if srtd != l:
            raise UnsortedError(srtd, l)

    def __init__(self, iterable=[]):
        StrictOrderingOnAppendList.ensure_sorted(iterable)

        list.__init__(self, iterable)

    def extend(self, l):
        if not isinstance(l, list):
            raise ValueError('List can only be extended with other list instances.')

        StrictOrderingOnAppendList.ensure_sorted(l)

        return list.extend(self, l)

    def __setslice__(self, i, j, sequence):
        if not isinstance(sequence, list):
            raise ValueError('List can only be sliced with other list instances.')

        StrictOrderingOnAppendList.ensure_sorted(sequence)

        return list.__setslice__(self, i, j, sequence)

    def __add__(self, other):
        if not isinstance(other, list):
            raise ValueError('Only lists can be appended to lists.')

        StrictOrderingOnAppendList.ensure_sorted(other)

        # list.__add__ will return a new list. We "cast" it to our type.
        return StrictOrderingOnAppendList(list.__add__(self, other))

    def __iadd__(self, other):
        if not isinstance(other, list):
            raise ValueError('Only lists can be appended to lists.')

        StrictOrderingOnAppendList.ensure_sorted(other)

        list.__iadd__(self, other)

        return self


class MozbuildDeletionError(Exception):
    pass

class HierarchicalStringList(object):
    """A hierarchy of lists of strings.

    Each instance of this object contains a list of strings, which can be set or
    appended to. A sub-level of the hierarchy is also an instance of this class,
    can be added by appending to an attribute instead.

    For example, the moz.build variable EXPORTS is an instance of this class. We
    can do:

    EXPORTS += ['foo.h']
    EXPORTS.mozilla.dom += ['bar.h']

    In this case, we have 3 instances (EXPORTS, EXPORTS.mozilla, and
    EXPORTS.mozilla.dom), and the first and last each have one element in their
    list.
    """
    __slots__ = ('_strings', '_children')

    def __init__(self):
        self._strings = StrictOrderingOnAppendList()
        self._children = {}

    def get_children(self):
        return self._children

    def get_strings(self):
        return self._strings

    def __setattr__(self, name, value):
        if name in self.__slots__:
            return object.__setattr__(self, name, value)

        # __setattr__ can be called with a list when a simple assignment is
        # used:
        #
        # EXPORTS.foo = ['file.h']
        #
        # In this case, we need to overwrite foo's current list of strings.
        #
        # However, __setattr__ is also called with a HierarchicalStringList
        # to try to actually set the attribute. We want to ignore this case,
        # since we don't actually create an attribute called 'foo', but just add
        # it to our list of children (using _get_exportvariable()).
        exports = self._get_exportvariable(name)
        if not isinstance(value, HierarchicalStringList):
            exports._check_list(value)
            exports._strings = value

    def __getattr__(self, name):
        if name.startswith('__'):
            return object.__getattr__(self, name)
        return self._get_exportvariable(name)

    def __delattr__(self, name):
        raise MozbuildDeletionError('Unable to delete attributes for this object')

    def __iadd__(self, other):
        self._check_list(other)
        self._strings += other
        return self

    def _get_exportvariable(self, name):
        return self._children.setdefault(name, HierarchicalStringList())

    def _check_list(self, value):
        if not isinstance(value, list):
            raise ValueError('Expected a list of strings, not %s' % type(value))
        for v in value:
            if not isinstance(v, str_type):
                raise ValueError(
                    'Expected a list of strings, not an element of %s' % type(v))
