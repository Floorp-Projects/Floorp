# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from contextlib import contextmanager

from .copier import FilePurger
from .files import (
    AbsoluteSymlinkFile,
    ExistingFile,
    File,
)
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


class UnreadableInstallManifest(Exception):
    """Raised when an invalid install manifest is parsed."""


class InstallManifest(object):
    """Describes actions to be used with a copier.FileCopier instance.

    This class facilitates serialization and deserialization of data used to
    construct a copier.FileCopier and to perform copy operations.

    The manifest defines source paths, destination paths, and a mechanism by
    which the destination file should come into existence.

    Entries in the manifest correspond to the following types:

      copy -- The file specified as the source path will be copied to the
          destination path.

      symlink -- The destination path will be a symlink to the source path.
          If symlinks are not supported, a copy will be performed.

      exists -- The destination path is accounted for and won't be deleted by
          the FileCopier. If the destination path doesn't exist, an error is
          raised.

      optional -- The destination path is accounted for and won't be deleted by
          the FileCopier. No error is raised if the destination path does not
          exist.

    Versions 1 and 2 of the manifest format are similar. Version 2 added
    optional path support.
    """
    FIELD_SEPARATOR = '\x1f'

    SYMLINK = 1
    COPY = 2
    REQUIRED_EXISTS = 3
    OPTIONAL_EXISTS = 4

    def __init__(self, path=None, fileobj=None):
        """Create a new InstallManifest entry.

        If path is defined, the manifest will be populated with data from the
        file path.

        If fh is defined, the manifest will be populated with data read
        from the specified file object.

        Both path and fileobj cannot be defined.
        """
        self._dests = {}

        if not path and not fileobj:
            return

        with _auto_fileobj(path, fileobj, 'rb') as fh:
            self._load_from_fileobj(fh)

    def _load_from_fileobj(self, fileobj):
        version = fileobj.readline().rstrip()
        if version not in ('1', '2'):
            raise UnreadableInstallManifest('Unknown manifest version: ' %
                version)

        for line in fileobj:
            line = line.rstrip()

            fields = line.split(self.FIELD_SEPARATOR)

            record_type = int(fields[0])

            if record_type == self.SYMLINK:
                dest, source= fields[1:]
                self.add_symlink(source, dest)
                continue

            if record_type == self.COPY:
                dest, source = fields[1:]
                self.add_copy(source, dest)
                continue

            if record_type == self.REQUIRED_EXISTS:
                _, path = fields
                self.add_required_exists(path)
                continue

            if record_type == self.OPTIONAL_EXISTS:
                _, path = fields
                self.add_optional_exists(path)
                continue

            raise UnreadableInstallManifest('Unknown record type: %d' %
                record_type)

    def __len__(self):
        return len(self._dests)

    def __contains__(self, item):
        return item in self._dests

    def __eq__(self, other):
        return isinstance(other, InstallManifest) and self._dests == other._dests

    def __neq__(self, other):
        return not self.__eq__(other)

    def __ior__(self, other):
        if not isinstance(other, InstallManifest):
            raise ValueError('Can only | with another instance of InstallManifest.')

        for dest in sorted(other._dests):
            self._add_entry(dest, other._dests[dest])

        return self

    def write(self, path=None, fileobj=None):
        """Serialize this manifest to a file or file object.

        If path is specified, that file will be written to. If fileobj is specified,
        the serialized content will be written to that file object.

        It is an error if both are specified.
        """
        with _auto_fileobj(path, fileobj, 'wb') as fh:
            fh.write('2\n')

            for dest in sorted(self._dests):
                entry = self._dests[dest]

                parts = ['%d' % entry[0], dest]
                parts.extend(entry[1:])
                fh.write('%s\n' % self.FIELD_SEPARATOR.join(
                    p.encode('utf-8') for p in parts))

    def add_symlink(self, source, dest):
        """Add a symlink to this manifest.

        dest will be a symlink to source.
        """
        self._add_entry(dest, (self.SYMLINK, source))

    def add_copy(self, source, dest):
        """Add a copy to this manifest.

        source will be copied to dest.
        """
        self._add_entry(dest, (self.COPY, source))

    def add_required_exists(self, dest):
        """Record that a destination file must exist.

        This effectively prevents the listed file from being deleted.
        """
        self._add_entry(dest, (self.REQUIRED_EXISTS,))

    def add_optional_exists(self, dest):
        """Record that a destination file may exist.

        This effectively prevents the listed file from being deleted. Unlike a
        "required exists" file, files of this type do not raise errors if the
        destination file does not exist.
        """
        self._add_entry(dest, (self.OPTIONAL_EXISTS,))

    def _add_entry(self, dest, entry):
        if dest in self._dests:
            raise ValueError('Item already in manifest: %s' % dest)

        self._dests[dest] = entry

    def populate_registry(self, registry):
        """Populate a mozpack.copier.FileRegistry instance with data from us.

        The caller supplied a FileRegistry instance (or at least something that
        conforms to its interface) and that instance is populated with data
        from this manifest.
        """
        for dest in sorted(self._dests):
            entry = self._dests[dest]
            install_type = entry[0]

            if install_type == self.SYMLINK:
                registry.add(dest, AbsoluteSymlinkFile(entry[1]))
                continue

            if install_type == self.COPY:
                registry.add(dest, File(entry[1]))
                continue

            if install_type == self.REQUIRED_EXISTS:
                registry.add(dest, ExistingFile(required=True))
                continue

            if install_type == self.OPTIONAL_EXISTS:
                registry.add(dest, ExistingFile(required=False))
                continue

            raise Exception('Unknown install type defined in manifest: %d' %
                install_type)
