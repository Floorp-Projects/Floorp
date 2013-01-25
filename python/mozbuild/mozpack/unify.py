# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozpack.files import (
    FileFinder,
    ExecutableFile,
    BaseFile,
    GeneratedFile,
)
from mozpack.executables import (
    MACHO_SIGNATURES,
    may_strip,
    strip,
)
from mozpack.errors import errors
from tempfile import mkstemp
import mozpack.path
import shutil
import struct
import os
import subprocess
from collections import OrderedDict


def may_unify_binary(file):
    '''
    Return whether the given BaseFile instance is an ExecutableFile that
    may be unified. Only non-fat Mach-O binaries are to be unified.
    '''
    if isinstance(file, ExecutableFile):
        signature = file.open().read(4)
        if len(signature) < 4:
            return False
        signature = struct.unpack('>L', signature)[0]
        if signature in MACHO_SIGNATURES:
            return True
    return False


class UnifiedExecutableFile(BaseFile):
    '''
    File class for executable and library files that to be unified with 'lipo'.
    '''
    def __init__(self, path1, path2):
        '''
        Initialize a UnifiedExecutableFile with the path to both non-fat Mach-O
        executables to be unified.
        '''
        self.path1 = path1
        self.path2 = path2

    def copy(self, dest):
        assert isinstance(dest, basestring)
        tmpfiles = []
        try:
            for p in [self.path1, self.path2]:
                fd, f = mkstemp()
                os.close(fd)
                tmpfiles.append(f)
                shutil.copy2(p, f)
                if may_strip(f):
                    strip(f)
            subprocess.call(['lipo', '-create'] + tmpfiles + ['-output', dest])
        finally:
            for f in tmpfiles:
                os.unlink(f)


class UnifiedFinder(FileFinder):
    '''
    Helper to get unified BaseFile instances from two distinct trees on the
    file system.
    '''
    def __init__(self, base1, base2, sorted=[], **kargs):
        '''
        Initialize a UnifiedFinder. base1 and base2 are the base directories
        for the two trees from which files are picked. UnifiedFinder.find()
        will act as FileFinder.find() but will error out when matches can only
        be found in one of the two trees and not the other. It will also error
        out if matches can be found on both ends but their contents are not
        identical.

        The sorted argument gives a list of mozpack.path.match patterns. File
        paths matching one of these patterns will have their contents compared
        with their lines sorted.
        '''
        self._base1 = FileFinder(base1, **kargs)
        self._base2 = FileFinder(base2, **kargs)
        self._sorted = sorted

    def _find(self, path):
        '''
        UnifiedFinder.find() implementation.
        '''
        files1 = OrderedDict()
        for p, f in self._base1.find(path):
            files1[p] = f
        files2 = set()
        for p, f in self._base2.find(path):
            files2.add(p)
            if p in files1:
                if may_unify_binary(files1[p]) and \
                        may_unify_binary(f):
                    yield p, UnifiedExecutableFile(files1[p].path, f.path)
                else:
                    unified = self.unify_file(p, files1[p], f)
                    if unified:
                        yield p, unified
                    else:
                        self._report_difference(p, files1[p], f)
            else:
                errors.error('File missing in %s: %s' % (self._base1.base, p))
        for p in [p for p in files1 if not p in files2]:
            errors.error('File missing in %s: %s' % (self._base2.base, p))

    def _report_difference(self, path, file1, file2):
        '''
        Report differences between files in both trees.
        '''
        errors.error("Can't unify %s: file differs between %s and %s" %
                     (path, self._base1.base, self._base2.base))
        if not isinstance(file1, ExecutableFile) and \
                not isinstance(file2, ExecutableFile):
            from difflib import unified_diff
            import sys
            for line in unified_diff(file1.open().readlines(),
                                     file2.open().readlines(),
                                     os.path.join(self._base1.base, path),
                                     os.path.join(self._base2.base, path)):
                errors.out.write(line)

    def unify_file(self, path, file1, file2):
        '''
        Given two BaseFiles and the path they were found at, check whether
        their content match and return the first BaseFile if they do.
        '''
        content1 = file1.open().readlines()
        content2 = file2.open().readlines()
        if content1 == content2:
            return file1
        for pattern in self._sorted:
            if mozpack.path.match(path, pattern):
                if sorted(content1) == sorted(content2):
                    return file1
                break
        return None


class UnifiedBuildFinder(UnifiedFinder):
    '''
    Specialized UnifiedFinder for Mozilla applications packaging. It allows
    "*.manifest" files to differ in their order, and unifies "buildconfig.html"
    files by merging their content.
    '''
    def __init__(self, base1, base2, **kargs):
        UnifiedFinder.__init__(self, base1, base2,
                               sorted=['**/*.manifest'], **kargs)

    def unify_file(self, path, file1, file2):
        '''
        Unify buildconfig.html contents, or defer to UnifiedFinder.unify_file.
        '''
        if mozpack.path.basename(path) == 'buildconfig.html':
            content1 = file1.open().readlines()
            content2 = file2.open().readlines()
            # Copy everything from the first file up to the end of its <body>,
            # insert a <hr> between the two files and copy the second file's
            # content beginning after its leading <h1>.
            return GeneratedFile(''.join(
                content1[:content1.index('</body>\n')] +
                ['<hr> </hr>\n'] +
                content2[content2.index('<h1>about:buildconfig</h1>\n') + 1:]
            ))
        return UnifiedFinder.unify_file(self, path, file1, file2)
