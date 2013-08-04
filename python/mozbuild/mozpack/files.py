# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import os
import re
import shutil
import stat
import uuid
from mozpack.executables import (
    is_executable,
    may_strip,
    strip,
    may_elfhack,
    elfhack,
)
from mozpack.chrome.manifest import ManifestEntry
from io import BytesIO
from mozpack.errors import (
    ErrorMessage,
    errors,
)
from mozpack.mozjar import JarReader
import mozpack.path
from collections import OrderedDict


class Dest(object):
    '''
    Helper interface for BaseFile.copy. The interface works as follows:
    - read() and write() can be used to sequentially read/write from the
      underlying file.
    - a call to read() after a write() will re-open the underlying file and
      read from it.
    - a call to write() after a read() will re-open the underlying file,
      emptying it, and write to it.
    '''
    def __init__(self, path):
        self.path = path
        self.mode = None

    def read(self, length=-1):
        if self.mode != 'r':
            self.file = open(self.path, 'rb')
            self.mode = 'r'
        return self.file.read(length)

    def write(self, data):
        if self.mode != 'w':
            self.file = open(self.path, 'wb')
            self.mode = 'w'
        return self.file.write(data)

    def exists(self):
        return os.path.exists(self.path)

    def close(self):
        if self.mode:
            self.mode = None
            self.file.close()


class BaseFile(object):
    '''
    Base interface and helper for file copying. Derived class may implement
    their own copy function, or rely on BaseFile.copy using the open() member
    function and/or the path property.
    '''
    def copy(self, dest, skip_if_older=True):
        '''
        Copy the BaseFile content to the destination given as a string or a
        Dest instance. Avoids replacing existing files if the BaseFile content
        matches that of the destination, or in case of plain files, if the
        destination is newer than the original file. This latter behaviour is
        disabled when skip_if_older is False.
        Returns whether a copy was actually performed (True) or not (False).
        '''
        if isinstance(dest, basestring):
            dest = Dest(dest)
        else:
            assert isinstance(dest, Dest)

        can_skip_content_check = False
        if not dest.exists():
            can_skip_content_check = True
        elif getattr(self, 'path', None) and getattr(dest, 'path', None):
            # os.path.getmtime returns a result in seconds with precision up to
            # the microsecond. But microsecond is too precise because
            # shutil.copystat only copies milliseconds, and seconds is not
            # enough precision.
            if skip_if_older and int(os.path.getmtime(self.path) * 1000) \
                    <= int(os.path.getmtime(dest.path) * 1000):
                return False
            elif os.path.getsize(self.path) != os.path.getsize(dest.path):
                can_skip_content_check = True

        if can_skip_content_check:
            if getattr(self, 'path', None) and getattr(dest, 'path', None):
                shutil.copy2(self.path, dest.path)
            else:
                # Ensure the file is always created
                if not dest.exists():
                    dest.write('')
                shutil.copyfileobj(self.open(), dest)
            return True

        src = self.open()
        copy_content = ''
        while True:
            dest_content = dest.read(32768)
            src_content = src.read(32768)
            copy_content += src_content
            if len(dest_content) == len(src_content) == 0:
                break
            # If the read content differs between origin and destination,
            # write what was read up to now, and copy the remainder.
            if dest_content != src_content:
                dest.write(copy_content)
                shutil.copyfileobj(src, dest)
                break
        if hasattr(self, 'path') and hasattr(dest, 'path'):
            shutil.copystat(self.path, dest.path)
        return True

    def open(self):
        '''
        Return a file-like object allowing to read() the content of the
        associated file. This is meant to be overloaded in subclasses to return
        a custom file-like object.
        '''
        assert self.path is not None
        return open(self.path, 'rb')


class File(BaseFile):
    '''
    File class for plain files.
    '''
    def __init__(self, path):
        self.path = path


class ExecutableFile(File):
    '''
    File class for executable and library files on OS/2, OS/X and ELF systems.
    (see mozpack.executables.is_executable documentation).
    '''
    def copy(self, dest, skip_if_older=True):
        assert isinstance(dest, basestring)
        # If File.copy didn't actually copy because dest is newer, check the
        # file sizes. If dest is smaller, it means it is already stripped and
        # elfhacked, so we can skip.
        if not File.copy(self, dest, skip_if_older) and \
                os.path.getsize(self.path) > os.path.getsize(dest):
            return False
        try:
            if may_strip(dest):
                strip(dest)
            if may_elfhack(dest):
                elfhack(dest)
        except ErrorMessage:
            os.remove(dest)
            raise
        return True


class AbsoluteSymlinkFile(File):
    '''File class that is copied by symlinking (if available).

    This class only works if the target path is absolute.
    '''

    def __init__(self, path):
        if not os.path.isabs(path):
            raise ValueError('Symlink target not absolute: %s' % path)

        File.__init__(self, path)

    def copy(self, dest, skip_if_older=True):
        assert isinstance(dest, basestring)

        # The logic in this function is complicated by the fact that symlinks
        # aren't universally supported. So, where symlinks aren't supported, we
        # fall back to file copying. Keep in mind that symlink support is
        # per-filesystem, not per-OS.

        # Handle the simple case where symlinks are definitely not supported by
        # falling back to file copy.
        if not hasattr(os, 'symlink'):
            return File.copy(self, dest, skip_if_older=skip_if_older)

        # Always verify the symlink target path exists.
        if not os.path.exists(self.path):
            raise ErrorMessage('Symlink target path does not exist: %s' % self.path)

        st = None

        try:
            st = os.lstat(dest)
        except OSError as ose:
            if ose.errno != errno.ENOENT:
                raise

        # If the dest is a symlink pointing to us, we have nothing to do.
        # If it's the wrong symlink, the filesystem must support symlinks,
        # so we replace with a proper symlink.
        if st and stat.S_ISLNK(st.st_mode):
            link = os.readlink(dest)
            if link == self.path:
                return False

            os.remove(dest)
            os.symlink(self.path, dest)
            return True

        # If the destination doesn't exist, we try to create a symlink. If that
        # fails, we fall back to copy code.
        if not st:
            try:
                os.symlink(self.path, dest)
                return True
            except OSError:
                return File.copy(self, dest, skip_if_older=skip_if_older)

        # Now the complicated part. If the destination exists, we could be
        # replacing a file with a symlink. Or, the filesystem may not support
        # symlinks. We want to minimize I/O overhead for performance reasons,
        # so we keep the existing destination file around as long as possible.
        # A lot of the system calls would be eliminated if we cached whether
        # symlinks are supported. However, even if we performed a single
        # up-front test of whether the root of the destination directory
        # supports symlinks, there's no guarantee that all operations for that
        # dest (or source) would be on the same filesystem and would support
        # symlinks.
        #
        # Our strategy is to attempt to create a new symlink with a random
        # name. If that fails, we fall back to copy mode. If that works, we
        # remove the old destination and move the newly-created symlink into
        # its place.

        temp_dest = os.path.join(os.path.dirname(dest), str(uuid.uuid4()))
        try:
            os.symlink(self.path, temp_dest)
        # TODO Figure out exactly how symlink creation fails and only trap
        # that.
        except EnvironmentError:
            return File.copy(self, dest, skip_if_older=skip_if_older)

        # If removing the original file fails, don't forget to clean up the
        # temporary symlink.
        try:
            os.remove(dest)
        except EnvironmentError:
            os.remove(temp_dest)
            raise

        os.rename(temp_dest, dest)
        return True


class RequiredExistingFile(BaseFile):
    '''
    File class that represents a file that must exist in the destination.

    The purpose of this class is to account for files that are installed
    via external means.

    When asked to copy, this class does nothing because nothing is known about
    the source file/data. However, since this file is required, we do validate
    that the destination path exists.
    '''
    def copy(self, dest, skip_if_older=True):
        if isinstance(dest, basestring):
            dest = Dest(dest)
        else:
            assert isinstance(dest, Dest)

        if not dest.exists():
            errors.fatal("Required existing file doesn't exist: %s" %
                dest.path)


class GeneratedFile(BaseFile):
    '''
    File class for content with no previous existence on the filesystem.
    '''
    def __init__(self, content):
        self.content = content

    def open(self):
        return BytesIO(self.content)


class DeflatedFile(BaseFile):
    '''
    File class for members of a jar archive. DeflatedFile.copy() effectively
    extracts the file from the jar archive.
    '''
    def __init__(self, file):
        from mozpack.mozjar import JarFileReader
        assert isinstance(file, JarFileReader)
        self.file = file

    def open(self):
        self.file.seek(0)
        return self.file


class XPTFile(GeneratedFile):
    '''
    File class for a linked XPT file. It takes several XPT files as input
    (using the add() and remove() member functions), and links them at copy()
    time.
    '''
    def __init__(self):
        self._files = set()

    def add(self, xpt):
        '''
        Add the given XPT file (as a BaseFile instance) to the list of XPTs
        to link.
        '''
        assert isinstance(xpt, BaseFile)
        self._files.add(xpt)

    def remove(self, xpt):
        '''
        Remove the given XPT file (as a BaseFile instance) from the list of
        XPTs to link.
        '''
        assert isinstance(xpt, BaseFile)
        self._files.remove(xpt)

    def copy(self, dest, skip_if_older=True):
        '''
        Link the registered XPTs and place the resulting linked XPT at the
        destination given as a string or a Dest instance. Avoids an expensive
        XPT linking if the interfaces in an existing destination match those of
        the individual XPTs to link.
        skip_if_older is ignored.
        '''
        if isinstance(dest, basestring):
            dest = Dest(dest)
        assert isinstance(dest, Dest)

        from xpt import xpt_link, Typelib, Interface
        all_typelibs = [Typelib.read(f.open()) for f in self._files]
        if dest.exists():
            # Typelib.read() needs to seek(), so use a BytesIO for dest
            # content.
            dest_interfaces = \
                dict((i.name, i)
                     for i in Typelib.read(BytesIO(dest.read())).interfaces
                     if i.iid != Interface.UNRESOLVED_IID)
            identical = True
            for f in self._files:
                typelib = Typelib.read(f.open())
                for i in typelib.interfaces:
                    if i.iid != Interface.UNRESOLVED_IID and \
                            not (i.name in dest_interfaces and
                                 i == dest_interfaces[i.name]):
                        identical = False
                        break
            if identical:
                return False
        s = BytesIO()
        xpt_link(all_typelibs).write(s)
        dest.write(s.getvalue())
        return True

    def open(self):
        raise RuntimeError("Unsupported")

    def isempty(self):
        '''
        Return whether there are XPT files to link.
        '''
        return len(self._files) == 0


class ManifestFile(BaseFile):
    '''
    File class for a manifest file. It takes individual manifest entries (using
    the add() and remove() member functions), and adjusts them to be relative
    to the base path for the manifest, given at creation.
    Example:
        There is a manifest entry "content webapprt webapprt/content/" relative
        to "webapprt/chrome". When packaging, the entry will be stored in
        jar:webapprt/omni.ja!/chrome/chrome.manifest, which means the entry
        will have to be relative to "chrome" instead of "webapprt/chrome". This
        doesn't really matter when serializing the entry, since this base path
        is not written out, but it matters when moving the entry at the same
        time, e.g. to jar:webapprt/omni.ja!/chrome.manifest, which we don't do
        currently but could in the future.
    '''
    def __init__(self, base, entries=None):
        self._entries = entries if entries else []
        self._base = base

    def add(self, entry):
        '''
        Add the given entry to the manifest. Entries are rebased at open() time
        instead of add() time so that they can be more easily remove()d.
        '''
        assert isinstance(entry, ManifestEntry)
        self._entries.append(entry)

    def remove(self, entry):
        '''
        Remove the given entry from the manifest.
        '''
        assert isinstance(entry, ManifestEntry)
        self._entries.remove(entry)

    def open(self):
        '''
        Return a file-like object allowing to read() the serialized content of
        the manifest.
        '''
        return BytesIO(''.join('%s\n' % e.rebase(self._base)
                               for e in self._entries))

    def __iter__(self):
        '''
        Iterate over entries in the manifest file.
        '''
        return iter(self._entries)

    def isempty(self):
        '''
        Return whether there are manifest entries to write
        '''
        return len(self._entries) == 0


class MinifiedProperties(BaseFile):
    '''
    File class for minified properties. This wraps around a BaseFile instance,
    and removes lines starting with a # from its content.
    '''
    def __init__(self, file):
        assert isinstance(file, BaseFile)
        self._file = file

    def open(self):
        '''
        Return a file-like object allowing to read() the minified content of
        the properties file.
        '''
        return BytesIO(''.join(l for l in self._file.open().readlines()
                               if not l.startswith('#')))


class BaseFinder(object):
    def __init__(self, base, minify=False):
        '''
        Initializes the instance with a reference base directory. The
        optional minify argument specifies whether file types supporting
        minification (currently only "*.properties") should be minified.
        '''
        self.base = base
        self._minify = minify

    def find(self, pattern):
        '''
        Yield path, BaseFile_instance pairs for all files under the base
        directory and its subdirectories that match the given pattern. See the
        mozpack.path.match documentation for a description of the handled
        patterns.
        '''
        while pattern.startswith('/'):
            pattern = pattern[1:]
        for p, f in self._find(pattern):
            yield p, self._minify_file(p, f)

    def __iter__(self):
        '''
        Iterates over all files under the base directory (excluding files
        starting with a '.' and files at any level under a directory starting
        with a '.').
            for path, file in finder:
                ...
        '''
        return self.find('')

    def __contains__(self, pattern):
        raise RuntimeError("'in' operator forbidden for %s. Use contains()." %
                           self.__class__.__name__)

    def contains(self, pattern):
        '''
        Return whether some files under the base directory match the given
        pattern. See the mozpack.path.match documentation for a description of
        the handled patterns.
        '''
        return any(self.find(pattern))

    def _minify_file(self, path, file):
        '''
        Return an appropriate MinifiedSomething wrapper for the given BaseFile
        instance (file), according to the file type (determined by the given
        path), if the FileFinder was created with minification enabled.
        Otherwise, just return the given BaseFile instance.
        Currently, only "*.properties" files are handled.
        '''
        if self._minify and not isinstance(file, ExecutableFile):
            if path.endswith('.properties'):
                return MinifiedProperties(file)
        return file


class FileFinder(BaseFinder):
    '''
    Helper to get appropriate BaseFile instances from the file system.
    '''
    def __init__(self, base, find_executables=True, **kargs):
        '''
        Create a FileFinder for files under the given base directory.
        The find_executables argument determines whether the finder needs to
        try to guess whether files are executables. Disabling this guessing
        when not necessary can speed up the finder significantly.
        '''
        BaseFinder.__init__(self, base, **kargs)
        self.find_executables = find_executables

    def _find(self, pattern):
        '''
        Actual implementation of FileFinder.find(), dispatching to specialized
        member functions depending on what kind of pattern was given.
        Note all files with a name starting with a '.' are ignored when
        scanning directories, but are not ignored when explicitely requested.
        '''
        if '*' in pattern:
            return self._find_glob('', mozpack.path.split(pattern))
        elif os.path.isdir(os.path.join(self.base, pattern)):
            return self._find_dir(pattern)
        else:
            return self._find_file(pattern)

    def _find_dir(self, path):
        '''
        Actual implementation of FileFinder.find() when the given pattern
        corresponds to an existing directory under the base directory.
        Ignores file names starting with a '.' under the given path. If the
        path itself has leafs starting with a '.', they are not ignored.
        '''
        for p in os.listdir(os.path.join(self.base, path)):
            if p.startswith('.'):
                continue
            for p_, f in self._find(mozpack.path.join(path, p)):
                yield p_, f

    def _find_file(self, path):
        '''
        Actual implementation of FileFinder.find() when the given pattern
        corresponds to an existing file under the base directory.
        '''
        srcpath = os.path.join(self.base, path)
        if not os.path.exists(srcpath):
            return

        if self.find_executables and is_executable(srcpath):
            yield path, ExecutableFile(srcpath)
        else:
            yield path, File(srcpath)

    def _find_glob(self, base, pattern):
        '''
        Actual implementation of FileFinder.find() when the given pattern
        contains globbing patterns ('*' or '**'). This is meant to be an
        equivalent of:
            for p, f in self:
                if mozpack.path.match(p, pattern):
                    yield p, f
        but avoids scanning the entire tree.
        '''
        if not pattern:
            for p, f in self._find(base):
                yield p, f
        elif pattern[0] == '**':
            for p, f in self._find(base):
                if mozpack.path.match(p, mozpack.path.join(*pattern)):
                    yield p, f
        elif '*' in pattern[0]:
            if not os.path.exists(os.path.join(self.base, base)):
                return
            for p in os.listdir(os.path.join(self.base, base)):
                if p.startswith('.') and not pattern[0].startswith('.'):
                    continue
                if mozpack.path.match(p, pattern[0]):
                    for p_, f in self._find_glob(mozpack.path.join(base, p),
                                                 pattern[1:]):
                        yield p_, f
        else:
            for p, f in self._find_glob(mozpack.path.join(base, pattern[0]),
                                        pattern[1:]):
                yield p, f


class JarFinder(BaseFinder):
    '''
    Helper to get appropriate DeflatedFile instances from a JarReader.
    '''
    def __init__(self, base, reader, **kargs):
        '''
        Create a JarFinder for files in the given JarReader. The base argument
        is used as an indication of the Jar file location.
        '''
        assert isinstance(reader, JarReader)
        BaseFinder.__init__(self, base, **kargs)
        self._files = OrderedDict((f.filename, f) for f in reader)

    def _find(self, pattern):
        '''
        Actual implementation of JarFinder.find(), dispatching to specialized
        member functions depending on what kind of pattern was given.
        '''
        if '*' in pattern:
            for p in self._files:
                if mozpack.path.match(p, pattern):
                    yield p, DeflatedFile(self._files[p])
        elif pattern == '':
            for p in self._files:
                yield p, DeflatedFile(self._files[p])
        elif pattern in self._files:
            yield pattern, DeflatedFile(self._files[pattern])
        else:
            for p in self._files:
                if mozpack.path.basedir(p, [pattern]) == pattern:
                    yield p, DeflatedFile(self._files[p])
