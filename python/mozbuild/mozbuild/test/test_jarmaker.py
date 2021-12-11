# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function
import unittest

import os
import sys
import os.path
from filecmp import dircmp
from tempfile import mkdtemp
from shutil import rmtree, copy2
import six
from six import StringIO
from zipfile import ZipFile
import mozunit

from mozbuild.jar import JarMaker


if sys.platform == "win32":
    import ctypes
    from ctypes import POINTER, WinError

    DWORD = ctypes.c_ulong
    LPDWORD = POINTER(DWORD)
    HANDLE = ctypes.c_void_p
    GENERIC_READ = 0x80000000
    FILE_SHARE_READ = 0x00000001
    OPEN_EXISTING = 3
    MAX_PATH = 260

    class FILETIME(ctypes.Structure):
        _fields_ = [("dwLowDateTime", DWORD), ("dwHighDateTime", DWORD)]

    class BY_HANDLE_FILE_INFORMATION(ctypes.Structure):
        _fields_ = [
            ("dwFileAttributes", DWORD),
            ("ftCreationTime", FILETIME),
            ("ftLastAccessTime", FILETIME),
            ("ftLastWriteTime", FILETIME),
            ("dwVolumeSerialNumber", DWORD),
            ("nFileSizeHigh", DWORD),
            ("nFileSizeLow", DWORD),
            ("nNumberOfLinks", DWORD),
            ("nFileIndexHigh", DWORD),
            ("nFileIndexLow", DWORD),
        ]

    # http://msdn.microsoft.com/en-us/library/aa363858
    CreateFile = ctypes.windll.kernel32.CreateFileA
    CreateFile.argtypes = [
        ctypes.c_char_p,
        DWORD,
        DWORD,
        ctypes.c_void_p,
        DWORD,
        DWORD,
        HANDLE,
    ]
    CreateFile.restype = HANDLE

    # http://msdn.microsoft.com/en-us/library/aa364952
    GetFileInformationByHandle = ctypes.windll.kernel32.GetFileInformationByHandle
    GetFileInformationByHandle.argtypes = [HANDLE, POINTER(BY_HANDLE_FILE_INFORMATION)]
    GetFileInformationByHandle.restype = ctypes.c_int

    # http://msdn.microsoft.com/en-us/library/aa364996
    GetVolumePathName = ctypes.windll.kernel32.GetVolumePathNameA
    GetVolumePathName.argtypes = [ctypes.c_char_p, ctypes.c_char_p, DWORD]
    GetVolumePathName.restype = ctypes.c_int

    # http://msdn.microsoft.com/en-us/library/aa364993
    GetVolumeInformation = ctypes.windll.kernel32.GetVolumeInformationA
    GetVolumeInformation.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        DWORD,
        LPDWORD,
        LPDWORD,
        LPDWORD,
        ctypes.c_char_p,
        DWORD,
    ]
    GetVolumeInformation.restype = ctypes.c_int


def symlinks_supported(path):
    if sys.platform == "win32":
        # Add 1 for a trailing backslash if necessary, and 1 for the terminating
        # null character.
        volpath = ctypes.create_string_buffer(len(path) + 2)
        rv = GetVolumePathName(six.ensure_binary(path), volpath, len(volpath))
        if rv == 0:
            raise WinError()

        fsname = ctypes.create_string_buffer(MAX_PATH + 1)
        rv = GetVolumeInformation(
            volpath, None, 0, None, None, None, fsname, len(fsname)
        )
        if rv == 0:
            raise WinError()

        # Return true only if the fsname is NTFS
        return fsname.value == "NTFS"
    else:
        return True


def _getfileinfo(path):
    """Return information for the given file. This only works on Windows."""
    fh = CreateFile(
        six.ensure_binary(path),
        GENERIC_READ,
        FILE_SHARE_READ,
        None,
        OPEN_EXISTING,
        0,
        None,
    )
    if fh is None:
        raise WinError()
    info = BY_HANDLE_FILE_INFORMATION()
    rv = GetFileInformationByHandle(fh, info)
    if rv == 0:
        raise WinError()
    return info


def is_symlink_to(dest, src):
    if sys.platform == "win32":
        # Check if both are on the same volume and have the same file ID
        destinfo = _getfileinfo(dest)
        srcinfo = _getfileinfo(src)
        return (
            destinfo.dwVolumeSerialNumber == srcinfo.dwVolumeSerialNumber
            and destinfo.nFileIndexHigh == srcinfo.nFileIndexHigh
            and destinfo.nFileIndexLow == srcinfo.nFileIndexLow
        )
    else:
        # Read the link and check if it is correct
        if not os.path.islink(dest):
            return False
        target = os.path.abspath(os.readlink(dest))
        abssrc = os.path.abspath(src)
        return target == abssrc


class _TreeDiff(dircmp):
    """Helper to report rich results on difference between two directories."""

    def _fillDiff(self, dc, rv, basepath="{0}"):
        rv["right_only"] += map(lambda l: basepath.format(l), dc.right_only)
        rv["left_only"] += map(lambda l: basepath.format(l), dc.left_only)
        rv["diff_files"] += map(lambda l: basepath.format(l), dc.diff_files)
        rv["funny"] += map(lambda l: basepath.format(l), dc.common_funny)
        rv["funny"] += map(lambda l: basepath.format(l), dc.funny_files)
        for subdir, _dc in six.iteritems(dc.subdirs):
            self._fillDiff(_dc, rv, basepath.format(subdir + "/{0}"))

    def allResults(self, left, right):
        rv = {"right_only": [], "left_only": [], "diff_files": [], "funny": []}
        self._fillDiff(self, rv)
        chunks = []
        if rv["right_only"]:
            chunks.append("{0} only in {1}".format(", ".join(rv["right_only"]), right))
        if rv["left_only"]:
            chunks.append("{0} only in {1}".format(", ".join(rv["left_only"]), left))
        if rv["diff_files"]:
            chunks.append("{0} differ".format(", ".join(rv["diff_files"])))
        if rv["funny"]:
            chunks.append("{0} don't compare".format(", ".join(rv["funny"])))
        return "; ".join(chunks)


class TestJarMaker(unittest.TestCase):
    """
    Unit tests for JarMaker.py
    """

    debug = False  # set to True to debug failing tests on disk

    def setUp(self):
        self.tmpdir = mkdtemp()
        self.srcdir = os.path.join(self.tmpdir, "src")
        os.mkdir(self.srcdir)
        self.builddir = os.path.join(self.tmpdir, "build")
        os.mkdir(self.builddir)
        self.refdir = os.path.join(self.tmpdir, "ref")
        os.mkdir(self.refdir)
        self.stagedir = os.path.join(self.tmpdir, "stage")
        os.mkdir(self.stagedir)

    def tearDown(self):
        if self.debug:
            print(self.tmpdir)
        elif sys.platform != "win32":
            # can't clean up on windows
            rmtree(self.tmpdir)

    def _jar_and_compare(self, infile, **kwargs):
        jm = JarMaker(outputFormat="jar")
        if "topsourcedir" not in kwargs:
            kwargs["topsourcedir"] = self.srcdir
        for attr in ("topsourcedir", "sourcedirs"):
            if attr in kwargs:
                setattr(jm, attr, kwargs[attr])
        jm.makeJar(infile, self.builddir)
        cwd = os.getcwd()
        os.chdir(self.builddir)
        try:
            # expand build to stage
            for path, dirs, files in os.walk("."):
                stagedir = os.path.join(self.stagedir, path)
                if not os.path.isdir(stagedir):
                    os.mkdir(stagedir)
                for file in files:
                    if file.endswith(".jar"):
                        # expand jar
                        stagepath = os.path.join(stagedir, file)
                        os.mkdir(stagepath)
                        zf = ZipFile(os.path.join(path, file))
                        # extractall is only in 2.6, do this manually :-(
                        for entry_name in zf.namelist():
                            segs = entry_name.split("/")
                            fname = segs.pop()
                            dname = os.path.join(stagepath, *segs)
                            if not os.path.isdir(dname):
                                os.makedirs(dname)
                            if not fname:
                                # directory, we're done
                                continue
                            _c = zf.read(entry_name)
                            open(os.path.join(dname, fname), "wb").write(_c)
                        zf.close()
                    else:
                        copy2(os.path.join(path, file), stagedir)
            # compare both dirs
            os.chdir("..")
            td = _TreeDiff("ref", "stage")
            return td.allResults("reference", "build")
        finally:
            os.chdir(cwd)

    def _create_simple_setup(self):
        # create src content
        jarf = open(os.path.join(self.srcdir, "jar.mn"), "w")
        jarf.write(
            """test.jar:
 dir/foo (bar)
"""
        )
        jarf.close()
        open(os.path.join(self.srcdir, "bar"), "w").write("content\n")
        # create reference
        refpath = os.path.join(self.refdir, "chrome", "test.jar", "dir")
        os.makedirs(refpath)
        open(os.path.join(refpath, "foo"), "w").write("content\n")

    def test_a_simple_jar(self):
        """Test a simple jar.mn"""
        self._create_simple_setup()
        # call JarMaker
        rv = self._jar_and_compare(
            os.path.join(self.srcdir, "jar.mn"), sourcedirs=[self.srcdir]
        )
        self.assertTrue(not rv, rv)

    def test_a_simple_symlink(self):
        """Test a simple jar.mn with a symlink"""
        if not symlinks_supported(self.srcdir):
            raise unittest.SkipTest("symlinks not supported")

        self._create_simple_setup()
        jm = JarMaker(outputFormat="symlink")
        jm.sourcedirs = [self.srcdir]
        jm.topsourcedir = self.srcdir
        jm.makeJar(os.path.join(self.srcdir, "jar.mn"), self.builddir)
        # All we do is check that srcdir/bar points to builddir/chrome/test/dir/foo
        srcbar = os.path.join(self.srcdir, "bar")
        destfoo = os.path.join(self.builddir, "chrome", "test", "dir", "foo")
        self.assertTrue(
            is_symlink_to(destfoo, srcbar),
            "{0} is not a symlink to {1}".format(destfoo, srcbar),
        )

    def _create_wildcard_setup(self):
        # create src content
        jarf = open(os.path.join(self.srcdir, "jar.mn"), "w")
        jarf.write(
            """test.jar:
 dir/bar (*.js)
 dir/hoge (qux/*)
"""
        )
        jarf.close()
        open(os.path.join(self.srcdir, "foo.js"), "w").write("foo.js\n")
        open(os.path.join(self.srcdir, "bar.js"), "w").write("bar.js\n")
        os.makedirs(os.path.join(self.srcdir, "qux", "foo"))
        open(os.path.join(self.srcdir, "qux", "foo", "1"), "w").write("1\n")
        open(os.path.join(self.srcdir, "qux", "foo", "2"), "w").write("2\n")
        open(os.path.join(self.srcdir, "qux", "baz"), "w").write("baz\n")
        # create reference
        refpath = os.path.join(self.refdir, "chrome", "test.jar", "dir")
        os.makedirs(os.path.join(refpath, "bar"))
        os.makedirs(os.path.join(refpath, "hoge", "foo"))
        open(os.path.join(refpath, "bar", "foo.js"), "w").write("foo.js\n")
        open(os.path.join(refpath, "bar", "bar.js"), "w").write("bar.js\n")
        open(os.path.join(refpath, "hoge", "foo", "1"), "w").write("1\n")
        open(os.path.join(refpath, "hoge", "foo", "2"), "w").write("2\n")
        open(os.path.join(refpath, "hoge", "baz"), "w").write("baz\n")

    def test_a_wildcard_jar(self):
        """Test a wildcard in jar.mn"""
        self._create_wildcard_setup()
        # call JarMaker
        rv = self._jar_and_compare(
            os.path.join(self.srcdir, "jar.mn"), sourcedirs=[self.srcdir]
        )
        self.assertTrue(not rv, rv)

    def test_a_wildcard_symlink(self):
        """Test a wildcard in jar.mn with symlinks"""
        if not symlinks_supported(self.srcdir):
            raise unittest.SkipTest("symlinks not supported")

        self._create_wildcard_setup()
        jm = JarMaker(outputFormat="symlink")
        jm.sourcedirs = [self.srcdir]
        jm.topsourcedir = self.srcdir
        jm.makeJar(os.path.join(self.srcdir, "jar.mn"), self.builddir)

        expected_symlinks = {
            ("bar", "foo.js"): ("foo.js",),
            ("bar", "bar.js"): ("bar.js",),
            ("hoge", "foo", "1"): ("qux", "foo", "1"),
            ("hoge", "foo", "2"): ("qux", "foo", "2"),
            ("hoge", "baz"): ("qux", "baz"),
        }
        for dest, src in six.iteritems(expected_symlinks):
            srcpath = os.path.join(self.srcdir, *src)
            destpath = os.path.join(self.builddir, "chrome", "test", "dir", *dest)
            self.assertTrue(
                is_symlink_to(destpath, srcpath),
                "{0} is not a symlink to {1}".format(destpath, srcpath),
            )


class Test_relativesrcdir(unittest.TestCase):
    def setUp(self):
        self.jm = JarMaker()
        self.jm.topsourcedir = "/TOPSOURCEDIR"
        self.jm.relativesrcdir = "browser/locales"
        self.fake_empty_file = StringIO()
        self.fake_empty_file.name = "fake_empty_file"

    def tearDown(self):
        del self.jm
        del self.fake_empty_file

    def test_en_US(self):
        jm = self.jm
        jm.makeJar(self.fake_empty_file, "/NO_OUTPUT_REQUIRED")
        self.assertEquals(
            jm.localedirs,
            [
                os.path.join(
                    os.path.abspath("/TOPSOURCEDIR"), "browser/locales", "en-US"
                )
            ],
        )

    def test_l10n_no_merge(self):
        jm = self.jm
        jm.l10nbase = "/L10N_BASE"
        jm.makeJar(self.fake_empty_file, "/NO_OUTPUT_REQUIRED")
        self.assertEquals(jm.localedirs, [os.path.join("/L10N_BASE", "browser")])

    def test_l10n_merge(self):
        jm = self.jm
        jm.l10nbase = "/L10N_MERGE"
        jm.makeJar(self.fake_empty_file, "/NO_OUTPUT_REQUIRED")
        self.assertEquals(
            jm.localedirs,
            [
                os.path.join("/L10N_MERGE", "browser"),
            ],
        )

    def test_override(self):
        jm = self.jm
        jm.outputFormat = "flat"  # doesn't touch chrome dir without files
        jarcontents = StringIO(
            """en-US.jar:
relativesrcdir dom/locales:
"""
        )
        jarcontents.name = "override.mn"
        jm.makeJar(jarcontents, "/NO_OUTPUT_REQUIRED")
        self.assertEquals(
            jm.localedirs,
            [os.path.join(os.path.abspath("/TOPSOURCEDIR"), "dom/locales", "en-US")],
        )

    def test_override_l10n(self):
        jm = self.jm
        jm.l10nbase = "/L10N_BASE"
        jm.outputFormat = "flat"  # doesn't touch chrome dir without files
        jarcontents = StringIO(
            """en-US.jar:
relativesrcdir dom/locales:
"""
        )
        jarcontents.name = "override.mn"
        jm.makeJar(jarcontents, "/NO_OUTPUT_REQUIRED")
        self.assertEquals(jm.localedirs, [os.path.join("/L10N_BASE", "dom")])


class Test_fluent(unittest.TestCase):
    """
    Unit tests for JarMaker interaction with Fluent
    """

    debug = False  # set to True to debug failing tests on disk

    def setUp(self):
        self.tmpdir = mkdtemp()
        self.srcdir = os.path.join(self.tmpdir, "src")
        os.mkdir(self.srcdir)
        self.builddir = os.path.join(self.tmpdir, "build")
        os.mkdir(self.builddir)
        self.l10nbase = os.path.join(self.tmpdir, "l10n-base")
        os.mkdir(self.l10nbase)
        self.l10nmerge = os.path.join(self.tmpdir, "l10n-merge")
        os.mkdir(self.l10nmerge)

    def tearDown(self):
        if self.debug:
            print(self.tmpdir)
        elif sys.platform != "win32":
            # can't clean up on windows
            rmtree(self.tmpdir)

    def _create_fluent_setup(self):
        # create src content
        jarf = open(os.path.join(self.srcdir, "jar.mn"), "w")
        jarf.write(
            """[localization] test.jar:
 app    (%app/**/*.ftl)
"""
        )
        jarf.close()
        appdir = os.path.join(self.srcdir, "app", "locales", "en-US", "app")
        os.makedirs(appdir)
        open(os.path.join(appdir, "test.ftl"), "w").write("id = Value")
        open(os.path.join(appdir, "test2.ftl"), "w").write("id2 = Value 2")

        l10ndir = os.path.join(self.l10nbase, "app", "app")
        os.makedirs(l10ndir)
        open(os.path.join(l10ndir, "test.ftl"), "w").write("id =  L10n Value")

    def test_l10n_not_merge_ftl(self):
        """Test that JarMaker doesn't merge source .ftl files"""
        self._create_fluent_setup()
        jm = JarMaker(outputFormat="symlink")
        jm.sourcedirs = [self.srcdir]
        jm.topsourcedir = self.srcdir
        jm.l10nbase = self.l10nbase
        jm.l10nmerge = self.l10nmerge
        jm.relativesrcdir = "app/locales"
        jm.makeJar(os.path.join(self.srcdir, "jar.mn"), self.builddir)

        # test.ftl should be taken from the l10ndir, since it is present there
        destpath = os.path.join(
            self.builddir, "localization", "test", "app", "test.ftl"
        )
        srcpath = os.path.join(self.l10nbase, "app", "app", "test.ftl")
        self.assertTrue(
            is_symlink_to(destpath, srcpath),
            "{0} should be a symlink to {1}".format(destpath, srcpath),
        )

        # test2.ftl on the other hand, is only present in en-US dir, and should
        # not be linked from the build dir
        destpath = os.path.join(
            self.builddir, "localization", "test", "app", "test2.ftl"
        )
        self.assertFalse(
            os.path.isfile(destpath), "test2.ftl should not be taken from en-US"
        )


if __name__ == "__main__":
    mozunit.main()
