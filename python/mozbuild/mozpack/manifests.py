# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from contextlib import contextmanager

from .copier import FilePurger
import mozpack.path as mozpath


# This probably belongs in a more generic module. Where?
@contextmanager
def _auto_fileobj(path, fileobj, mode='r'):
    if path and fileobj:
        raise AssertionError('Only 1 of path or fileobj may be defined.')

    if not path and not fileobj:
        raise AssertionError('Must specified 1 of path or fileobj.')

    if path:
        fileobj = open(path, mode)

    try:
        yield fileobj
    finally:
        if path:
            fileobj.close()


class UnreadablePurgeManifest(Exception):
    """Error for failure when reading content of a serialized PurgeManifest."""


class PurgeManifest(object):
    """Describes actions to be used with a copier.FilePurger instance.

    This class facilitates serialization and deserialization of data used
    to construct a copier.FilePurger and to perform a purge operation.

    The manifest contains a set of entries (paths that are accounted for and
    shouldn't be purged) and a relative path. The relative path is optional and
    can be used to e.g. have several manifest files in a directory be
    dynamically applied to subdirectories under a common base directory.

    Don't be confused by the name of this class: entries are files that are
    *not* purged.
    """
    def __init__(self, relpath='', path=None, fileobj=None):
        self.relpath = relpath
        self.entries = set()

        if not path and not fileobj:
            return

        with _auto_fileobj(path, fileobj, mode='rt') as fh:
            self._read_from_fh(fh)

    def __eq__(self, other):
        if not isinstance(other, PurgeManifest):
            return False

        return other.relpath == self.relpath and other.entries == self.entries

    def _read_from_fh(self, fileobj):
        version = fileobj.readline().rstrip()
        if version != '1':
            raise UnreadablePurgeManifest('Unknown manifest version: %s' %
                version)

        self.relpath = fileobj.readline().rstrip()

        for entry in fileobj:
            self.entries.add(entry.rstrip())

    def add(self, path):
        return self.entries.add(path)

    def write(self, path=None, fileobj=None):
        with _auto_fileobj(path, fileobj, 'wt') as fh:
            fh.write('1\n')
            fh.write('%s\n' % self.relpath)

            # We write sorted so written output is consistent.
            for entry in sorted(self.entries):
                fh.write('%s\n' % entry)

    def get_purger(self, prepend_relpath=False):
        """Obtain a FilePurger instance from this manifest.

        If :prepend_relpath is truish, the relative path in the manifest will
        be prepended to paths added to the FilePurger. Otherwise, the raw paths
        will be used.
        """
        p = FilePurger()
        for entry in self.entries:
            if prepend_relpath:
                entry = mozpath.join(self.relpath, entry)

            p.add(entry)

        return p
