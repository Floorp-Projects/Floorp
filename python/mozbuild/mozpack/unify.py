# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import struct
import subprocess
from collections import OrderedDict
from tempfile import mkstemp

import buildconfig

import mozpack.path as mozpath
from mozbuild.util import hexdump
from mozpack.errors import errors
from mozpack.executables import MACHO_SIGNATURES
from mozpack.files import BaseFile, BaseFinder, ExecutableFile, GeneratedFile

# Regular expressions for unifying install.rdf
FIND_TARGET_PLATFORM = re.compile(
    r"""
    <(?P<ns>[-._0-9A-Za-z]+:)?targetPlatform>  # The targetPlatform tag, with any namespace
    (?P<platform>[^<]*)                        # The actual platform value
    </(?P=ns)?targetPlatform>                  # The closing tag
    """,
    re.X,
)
FIND_TARGET_PLATFORM_ATTR = re.compile(
    r"""
    (?P<tag><(?:[-._0-9A-Za-z]+:)?Description) # The opening part of the <Description> tag
    (?P<attrs>[^>]*?)\s+                       # The initial attributes
    (?P<ns>[-._0-9A-Za-z]+:)?targetPlatform=   # The targetPlatform attribute, with any namespace
    [\'"](?P<platform>[^\'"]+)[\'"]            # The actual platform value
    (?P<otherattrs>[^>]*?>)                    # The remaining attributes and closing angle bracket
    """,
    re.X,
)


def may_unify_binary(file):
    """
    Return whether the given BaseFile instance is an ExecutableFile that
    may be unified. Only non-fat Mach-O binaries are to be unified.
    """
    if isinstance(file, ExecutableFile):
        signature = file.open().read(4)
        if len(signature) < 4:
            return False
        signature = struct.unpack(">L", signature)[0]
        if signature in MACHO_SIGNATURES:
            return True
    return False


class UnifiedExecutableFile(BaseFile):
    """
    File class for executable and library files that to be unified with 'lipo'.
    """

    def __init__(self, executable1, executable2):
        """
        Initialize a UnifiedExecutableFile with a pair of ExecutableFiles to
        be unified. They are expected to be non-fat Mach-O executables.
        """
        assert isinstance(executable1, ExecutableFile)
        assert isinstance(executable2, ExecutableFile)
        self._executables = (executable1, executable2)

    def copy(self, dest, skip_if_older=True):
        """
        Create a fat executable from the two Mach-O executable given when
        creating the instance.
        skip_if_older is ignored.
        """
        assert isinstance(dest, str)
        tmpfiles = []
        try:
            for e in self._executables:
                fd, f = mkstemp()
                os.close(fd)
                tmpfiles.append(f)
                e.copy(f, skip_if_older=False)
            lipo = buildconfig.substs.get("LIPO") or "lipo"
            subprocess.check_call([lipo, "-create"] + tmpfiles + ["-output", dest])
        except Exception as e:
            errors.error(
                "Failed to unify %s and %s: %s"
                % (self._executables[0].path, self._executables[1].path, str(e))
            )
        finally:
            for f in tmpfiles:
                os.unlink(f)


class UnifiedFinder(BaseFinder):
    """
    Helper to get unified BaseFile instances from two distinct trees on the
    file system.
    """

    def __init__(self, finder1, finder2, sorted=[], **kargs):
        """
        Initialize a UnifiedFinder. finder1 and finder2 are BaseFinder
        instances from which files are picked. UnifiedFinder.find() will act as
        FileFinder.find() but will error out when matches can only be found in
        one of the two trees and not the other. It will also error out if
        matches can be found on both ends but their contents are not identical.

        The sorted argument gives a list of mozpath.match patterns. File
        paths matching one of these patterns will have their contents compared
        with their lines sorted.
        """
        assert isinstance(finder1, BaseFinder)
        assert isinstance(finder2, BaseFinder)
        self._finder1 = finder1
        self._finder2 = finder2
        self._sorted = sorted
        BaseFinder.__init__(self, finder1.base, **kargs)

    def _find(self, path):
        """
        UnifiedFinder.find() implementation.
        """
        # There is no `OrderedSet`.  Operator `|` was added only in
        # Python 3.9, so we merge by hand.
        all_paths = OrderedDict()

        files1 = OrderedDict()
        for p, f in self._finder1.find(path):
            files1[p] = f
            all_paths[p] = True
        files2 = OrderedDict()
        for p, f in self._finder2.find(path):
            files2[p] = f
            all_paths[p] = True

        for p in all_paths:
            err = errors.count
            unified = self.unify_file(p, files1.get(p), files2.get(p))
            if unified:
                yield p, unified
            elif err == errors.count:  # No errors have already been reported.
                self._report_difference(p, files1.get(p), files2.get(p))

    def _report_difference(self, path, file1, file2):
        """
        Report differences between files in both trees.
        """
        if not file1:
            errors.error("File missing in %s: %s" % (self._finder1.base, path))
            return
        if not file2:
            errors.error("File missing in %s: %s" % (self._finder2.base, path))
            return

        errors.error(
            "Can't unify %s: file differs between %s and %s"
            % (path, self._finder1.base, self._finder2.base)
        )
        if not isinstance(file1, ExecutableFile) and not isinstance(
            file2, ExecutableFile
        ):
            from difflib import unified_diff

            try:
                lines1 = [l.decode("utf-8") for l in file1.open().readlines()]
                lines2 = [l.decode("utf-8") for l in file2.open().readlines()]
            except UnicodeDecodeError:
                lines1 = hexdump(file1.open().read())
                lines2 = hexdump(file2.open().read())

            for line in unified_diff(
                lines1,
                lines2,
                os.path.join(self._finder1.base, path),
                os.path.join(self._finder2.base, path),
            ):
                errors.out.write(line)

    def unify_file(self, path, file1, file2):
        """
        Given two BaseFiles and the path they were found at, return a
        unified version of the files. If the files match, the first BaseFile
        may be returned.
        If the files don't match or one of them is `None`, the method returns
        `None`.
        Subclasses may decide to unify by using one of the files in that case.
        """
        if not file1 or not file2:
            return None

        if may_unify_binary(file1) and may_unify_binary(file2):
            return UnifiedExecutableFile(file1, file2)

        content1 = file1.open().readlines()
        content2 = file2.open().readlines()
        if content1 == content2:
            return file1
        for pattern in self._sorted:
            if mozpath.match(path, pattern):
                if sorted(content1) == sorted(content2):
                    return file1
                break
        return None


class UnifiedBuildFinder(UnifiedFinder):
    """
    Specialized UnifiedFinder for Mozilla applications packaging. It allows
    ``*.manifest`` files to differ in their order, and unifies ``buildconfig.html``
    files by merging their content.
    """

    def __init__(self, finder1, finder2, **kargs):
        UnifiedFinder.__init__(
            self, finder1, finder2, sorted=["**/*.manifest"], **kargs
        )

    def unify_file(self, path, file1, file2):
        """
        Unify files taking Mozilla application special cases into account.
        Otherwise defer to UnifiedFinder.unify_file.
        """
        basename = mozpath.basename(path)
        if file1 and file2 and basename == "buildconfig.html":
            content1 = file1.open().readlines()
            content2 = file2.open().readlines()
            # Copy everything from the first file up to the end of its <div>,
            # insert a <hr> between the two files and copy the second file's
            # content beginning after its leading <h1>.
            return GeneratedFile(
                b"".join(
                    content1[: content1.index(b"    </div>\n")]
                    + [b"      <hr> </hr>\n"]
                    + content2[
                        content2.index(b"      <h1>Build Configuration</h1>\n") + 1 :
                    ]
                )
            )
        elif file1 and file2 and basename == "install.rdf":
            # install.rdf files often have em:targetPlatform (either as
            # attribute or as tag) that will differ between platforms. The
            # unified install.rdf should contain both em:targetPlatforms if
            # they exist, or strip them if only one file has a target platform.
            content1, content2 = (
                FIND_TARGET_PLATFORM_ATTR.sub(
                    lambda m: m.group("tag")
                    + m.group("attrs")
                    + m.group("otherattrs")
                    + "<%stargetPlatform>%s</%stargetPlatform>"
                    % (m.group("ns") or "", m.group("platform"), m.group("ns") or ""),
                    f.open().read().decode("utf-8"),
                )
                for f in (file1, file2)
            )

            platform2 = FIND_TARGET_PLATFORM.search(content2)
            return GeneratedFile(
                FIND_TARGET_PLATFORM.sub(
                    lambda m: m.group(0) + platform2.group(0) if platform2 else "",
                    content1,
                )
            )
        return UnifiedFinder.unify_file(self, path, file1, file2)
