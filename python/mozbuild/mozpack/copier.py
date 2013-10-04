# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import stat

from mozpack.errors import errors
from mozpack.files import (
    BaseFile,
    Dest,
)
import mozpack.path
import errno
from collections import (
    OrderedDict,
)


class FileRegistry(object):
    '''
    Generic container to keep track of a set of BaseFile instances. It
    preserves the order under which the files are added, but doesn't keep
    track of empty directories (directories are not stored at all).
    The paths associated with the BaseFile instances are relative to an
    unspecified (virtual) root directory.

        registry = FileRegistry()
        registry.add('foo/bar', file_instance)
    '''

    def __init__(self):
        self._files = OrderedDict()

    def add(self, path, content):
        '''
        Add a BaseFile instance to the container, under the given path.
        '''
        assert isinstance(content, BaseFile)
        if path in self._files:
            return errors.error("%s already added" % path)
        # Check whether any parent of the given path is already stored
        partial_path = path
        while partial_path:
            partial_path = mozpack.path.dirname(partial_path)
            if partial_path in self._files:
                return errors.error("Can't add %s: %s is a file" %
                                    (path, partial_path))
        self._files[path] = content

    def match(self, pattern):
        '''
        Return the list of paths, stored in the container, matching the
        given pattern. See the mozpack.path.match documentation for a
        description of the handled patterns.
        '''
        if '*' in pattern:
            return [p for p in self.paths()
                    if mozpack.path.match(p, pattern)]
        if pattern == '':
            return self.paths()
        if pattern in self._files:
            return [pattern]
        return [p for p in self.paths()
                if mozpack.path.basedir(p, [pattern]) == pattern]

    def remove(self, pattern):
        '''
        Remove paths matching the given pattern from the container. See the
        mozpack.path.match documentation for a description of the handled
        patterns.
        '''
        items = self.match(pattern)
        if not items:
            return errors.error("Can't remove %s: %s" % (pattern,
                                "not matching anything previously added"))
        for i in items:
            del self._files[i]

    def paths(self):
        '''
        Return all paths stored in the container, in the order they were added.
        '''
        return self._files.keys()

    def __len__(self):
        '''
        Return number of paths stored in the container.
        '''
        return len(self._files)

    def __contains__(self, pattern):
        raise RuntimeError("'in' operator forbidden for %s. Use contains()." %
                           self.__class__.__name__)

    def contains(self, pattern):
        '''
        Return whether the container contains paths matching the given
        pattern. See the mozpack.path.match documentation for a description of
        the handled patterns.
        '''
        return len(self.match(pattern)) > 0

    def __getitem__(self, path):
        '''
        Return the BaseFile instance stored in the container for the given
        path.
        '''
        return self._files[path]

    def __iter__(self):
        '''
        Iterate over all (path, BaseFile instance) pairs from the container.
            for path, file in registry:
                (...)
        '''
        return self._files.iteritems()


class FileCopyResult(object):
    """Represents results of a FileCopier.copy operation."""

    def __init__(self):
        self.updated_files = set()
        self.existing_files = set()
        self.removed_files = set()
        self.removed_directories = set()

    @property
    def updated_files_count(self):
        return len(self.updated_files)

    @property
    def existing_files_count(self):
        return len(self.existing_files)

    @property
    def removed_files_count(self):
        return len(self.removed_files)

    @property
    def removed_directories_count(self):
        return len(self.removed_directories)


class FileCopier(FileRegistry):
    '''
    FileRegistry with the ability to copy the registered files to a separate
    directory.
    '''
    def copy(self, destination, skip_if_older=True, remove_unaccounted=True):
        '''
        Copy all registered files to the given destination path. The given
        destination can be an existing directory, or not exist at all. It
        can't be e.g. a file.
        The copy process acts a bit like rsync: files are not copied when they
        don't need to (see mozpack.files for details on file.copy).

        By default, files in the destination directory that aren't registered
        are removed and empty directories are deleted. To disable removing of
        unregistered files, pass remove_unaccounted=False.

        Returns a FileCopyResult that details what changed.
        '''
        assert isinstance(destination, basestring)
        assert not os.path.exists(destination) or os.path.isdir(destination)

        result = FileCopyResult()
        have_symlinks = hasattr(os, 'symlink')
        destination = os.path.normpath(destination)

        # We create the destination directory specially. We can't do this as
        # part of the loop doing mkdir() below because that loop munges
        # symlinks and permissions and parent directories of the destination
        # directory may have their own weird schema. The contract is we only
        # manage children of destination, not its parents.
        try:
            os.makedirs(destination)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

        # Because we could be handling thousands of files, code in this
        # function is optimized to minimize system calls. We prefer CPU time
        # in Python over possibly I/O bound filesystem calls to stat() and
        # friends.

        required_dirs = set()
        dest_files = set()

        for p, f in self:
            required_dirs.add(os.path.normpath(os.path.dirname(p)))
            dest_files.add(os.path.normpath(os.path.join(destination, p)))

        # Ensure all parent directories are present in required_dirs.
        extra = set()
        for d in required_dirs:
            parent = d
            while parent:
                parent = os.path.dirname(parent)
                extra.add(parent)

        required_dirs |= extra
        required_dirs = set(os.path.normpath(os.path.join(destination, d))
            for d in required_dirs)

        # Ensure destination directories are in place and proper.
        #
        # The "proper" bit is important. We need to ensure that directories
        # have appropriate permissions or we will be unable to discover
        # and write files. Furthermore, we need to verify directories aren't
        # symlinks.
        #
        # Symlinked directories (a symlink whose target is a directory) are
        # incompatible with us because our manifest talks in terms of files,
        # not directories. If we leave symlinked directories unchecked, we
        # would blindly follow symlinks and this might confuse file
        # installation. For example, if an existing directory is a symlink
        # to directory X and we attempt to install a symlink in this directory
        # to a file in directory X, we may create a recursive symlink!
        for d in sorted(required_dirs, key=len):
            try:
                os.mkdir(d)
            except OSError as error:
                if error.errno != errno.EEXIST:
                    raise

            # We allow the destination to be a symlink because the caller
            # is responsible for managing the destination and we assume
            # they know what they are doing.
            if have_symlinks and d != destination:
                st = os.lstat(d)
                if stat.S_ISLNK(st.st_mode):
                    # While we have remove_unaccounted, it doesn't apply
                    # to directory symlinks because if it did, our behavior
                    # could be very wrong.
                    os.remove(d)
                    os.mkdir(d)

            if not os.access(d, os.W_OK):
                umask = os.umask(0077)
                os.umask(umask)
                os.chmod(d, 0777 & ~umask)

        # While we have remove_unaccounted, it doesn't apply to empty
        # directories because it wouldn't make sense: an empty directory
        # is empty, so removing it should have no effect.
        existing_dirs = set()
        existing_files = set()
        for root, dirs, files in os.walk(destination):
            # We need to perform the same symlink detection as above. os.walk()
            # doesn't follow symlinks into directories by default, so we need
            # to check dirs (we can't wait for root).
            if have_symlinks:
                filtered = []
                for d in dirs:
                    full = os.path.join(root, d)
                    st = os.lstat(full)
                    if stat.S_ISLNK(st.st_mode):
                        os.remove(full)
                        # We don't need to recreate it because the code above
                        # would have created it as necessary.
                    else:
                        filtered.append(d)

                dirs[:] = filtered

            existing_dirs.add(os.path.normpath(root))

            for d in dirs:
                existing_dirs.add(os.path.normpath(os.path.join(root, d)))

            for f in files:
                existing_files.add(os.path.normpath(os.path.join(root, f)))

        # Now we reconcile the state of the world against what we want.

        # Remove files no longer accounted for.
        if remove_unaccounted:
            for f in existing_files - dest_files:
                # Windows requires write access to remove files.
                if os.name == 'nt' and not os.access(f, os.W_OK):
                    # It doesn't matter what we set permissions to since we
                    # will remove this file shortly.
                    os.chmod(f, 0600)

                os.remove(f)
                result.removed_files.add(f)

        # Install files.
        for p, f in self:
            destfile = os.path.normpath(os.path.join(destination, p))
            if f.copy(destfile, skip_if_older):
                result.updated_files.add(destfile)
            else:
                result.existing_files.add(destfile)

        # Figure out which directories can be removed. This is complicated
        # by the fact we optionally remove existing files. This would be easy
        # if we walked the directory tree after installing files. But, we're
        # trying to minimize system calls.

        # Start with the ideal set.
        remove_dirs = existing_dirs - required_dirs

        # Then don't remove directories if we didn't remove unaccounted files
        # and one of those files exists.
        if not remove_unaccounted:
            for f in existing_files:
                parent = f
                previous = ''
                parents = set()
                while True:
                    parent = os.path.dirname(parent)
                    parents.add(parent)

                    if previous == parent:
                        break

                    previous = parent

                remove_dirs -= parents

        # Remove empty directories that aren't required.
        for d in sorted(remove_dirs, key=len, reverse=True):
            # Permissions may not allow deletion. So ensure write access is
            # in place before attempting delete.
            os.chmod(d, 0700)
            os.rmdir(d)
            result.removed_directories.add(d)

        return result


class FilePurger(FileCopier):
    """A variation of FileCopier that is used to purge untracked files.

    Callers create an instance then call .add() to register files/paths that
    should exist. Once the canonical set of files that may exist is defined,
    .purge() is called against a target directory. All files and empty
    directories in the target directory that aren't in the registry will be
    deleted.
    """
    class FakeFile(BaseFile):
        def copy(self, dest, skip_if_older=True):
            return True

    def add(self, path):
        """Record that a path should exist.

        We currently do not track what kind of entity should be behind that
        path. We presumably could add type tracking later and have purging
        delete entities if there is a type mismatch.
        """
        return FileCopier.add(self, path, FilePurger.FakeFile())

    def purge(self, dest):
        """Deletes all files and empty directories not in the registry."""
        return FileCopier.copy(self, dest)

    def copy(self, *args, **kwargs):
        raise Exception('copy() disabled on FilePurger. Use purge().')


class Jarrer(FileRegistry, BaseFile):
    '''
    FileRegistry with the ability to copy and pack the registered files as a
    jar file. Also acts as a BaseFile instance, to be copied with a FileCopier.
    '''
    def __init__(self, compress=True, optimize=True):
        '''
        Create a Jarrer instance. See mozpack.mozjar.JarWriter documentation
        for details on the compress and optimize arguments.
        '''
        self.compress = compress
        self.optimize = optimize
        self._preload = []
        FileRegistry.__init__(self)

    def copy(self, dest, skip_if_older=True):
        '''
        Pack all registered files in the given destination jar. The given
        destination jar may be a path to jar file, or a Dest instance for
        a jar file.
        If the destination jar file exists, its (compressed) contents are used
        instead of the registered BaseFile instances when appropriate.
        '''
        class DeflaterDest(Dest):
            '''
            Dest-like class, reading from a file-like object initially, but
            switching to a Deflater object if written to.

                dest = DeflaterDest(original_file)
                dest.read()      # Reads original_file
                dest.write(data) # Creates a Deflater and write data there
                dest.read()      # Re-opens the Deflater and reads from it
            '''
            def __init__(self, orig=None, compress=True):
                self.mode = None
                self.deflater = orig
                self.compress = compress

            def read(self, length=-1):
                if self.mode != 'r':
                    assert self.mode is None
                    self.mode = 'r'
                return self.deflater.read(length)

            def write(self, data):
                if self.mode != 'w':
                    from mozpack.mozjar import Deflater
                    self.deflater = Deflater(self.compress)
                    self.mode = 'w'
                self.deflater.write(data)

            def exists(self):
                return self.deflater is not None

        if isinstance(dest, basestring):
            dest = Dest(dest)
        assert isinstance(dest, Dest)

        from mozpack.mozjar import JarWriter, JarReader
        try:
            old_jar = JarReader(fileobj=dest)
        except Exception:
            old_jar = []

        old_contents = dict([(f.filename, f) for f in old_jar])

        with JarWriter(fileobj=dest, compress=self.compress,
                       optimize=self.optimize) as jar:
            for path, file in self:
                if path in old_contents:
                    deflater = DeflaterDest(old_contents[path], self.compress)
                else:
                    deflater = DeflaterDest(compress=self.compress)
                file.copy(deflater, skip_if_older)
                jar.add(path, deflater.deflater)
            if self._preload:
                jar.preload(self._preload)

    def open(self):
        raise RuntimeError('unsupported')

    def preload(self, paths):
        '''
        Add the given set of paths to the list of preloaded files. See
        mozpack.mozjar.JarWriter documentation for details on jar preloading.
        '''
        self._preload.extend(paths)
