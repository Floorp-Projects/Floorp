#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from unittest.mock import patch

import buildconfig
import mozpack.path as mozpath
import mozunit
import symbolstore
from mozpack.manifests import InstallManifest
from symbolstore import realpath

# Some simple functions to mock out files that the platform-specific dumpers will accept.
# dump_syms itself will not be run (we mock that call out), but we can't override
# the ShouldProcessFile method since we actually want to test that.


def write_elf(filename):
    open(filename, "wb").write(
        struct.pack("<7B45x", 0x7F, ord("E"), ord("L"), ord("F"), 1, 1, 1)
    )


def write_macho(filename):
    open(filename, "wb").write(struct.pack("<I28x", 0xFEEDFACE))


def write_dll(filename):
    open(filename, "w").write("aaa")
    # write out a fake PDB too
    open(os.path.splitext(filename)[0] + ".pdb", "w").write("aaa")


def target_platform():
    return buildconfig.substs["OS_TARGET"]


def host_platform():
    return buildconfig.substs["HOST_OS_ARCH"]


writer = {
    "WINNT": write_dll,
    "Linux": write_elf,
    "Sunos5": write_elf,
    "Darwin": write_macho,
}[target_platform()]
extension = {"WINNT": ".dll", "Linux": ".so", "Sunos5": ".so", "Darwin": ".dylib"}[
    target_platform()
]
file_output = [
    {"WINNT": "bogus data", "Linux": "ELF executable", "Darwin": "Mach-O executable"}[
        target_platform()
    ]
]


def add_extension(files):
    return [f + extension for f in files]


class HelperMixin(object):
    """
    Test that passing filenames to exclude from processing works.
    """

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        if not self.test_dir.endswith(os.sep):
            self.test_dir += os.sep
        symbolstore.srcdirRepoInfo = {}
        symbolstore.vcsFileInfoCache = {}

        # Remove environment variables that can influence tests.
        for e in ("MOZ_SOURCE_CHANGESET", "MOZ_SOURCE_REPO"):
            try:
                del os.environ[e]
            except KeyError:
                pass

    def tearDown(self):
        shutil.rmtree(self.test_dir)
        symbolstore.srcdirRepoInfo = {}
        symbolstore.vcsFileInfoCache = {}

    def make_dirs(self, f):
        d = os.path.dirname(f)
        if d and not os.path.exists(d):
            os.makedirs(d)

    def make_file(self, path):
        self.make_dirs(path)
        with open(path, "wb"):
            pass

    def add_test_files(self, files):
        for f in files:
            f = os.path.join(self.test_dir, f)
            self.make_dirs(f)
            writer(f)


def mock_dump_syms(module_id, filename, extra=[]):
    return (
        ["MODULE os x86 %s %s" % (module_id, filename)]
        + extra
        + ["FILE 0 foo.c", "PUBLIC xyz 123"]
    )


class TestCopyDebug(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)
        self.symbol_dir = tempfile.mkdtemp()
        self.mock_call = patch("subprocess.call").start()
        self.stdouts = []
        self.mock_popen = patch("subprocess.Popen").start()
        stdout_iter = self.next_mock_stdout()

        def next_popen(*args, **kwargs):
            m = mock.MagicMock()
            # Get the iterators over whatever output was provided.
            stdout_ = next(stdout_iter)
            # Eager evaluation for communicate(), below.
            stdout_ = list(stdout_)
            # stdout is really an iterator, so back to iterators we go.
            m.stdout = iter(stdout_)
            m.wait.return_value = 0
            # communicate returns the full text of stdout and stderr.
            m.communicate.return_value = ("\n".join(stdout_), "")
            return m

        self.mock_popen.side_effect = next_popen
        shutil.rmtree = patch("shutil.rmtree").start()

    def tearDown(self):
        HelperMixin.tearDown(self)
        patch.stopall()
        shutil.rmtree(self.symbol_dir)

    def next_mock_stdout(self):
        if not self.stdouts:
            yield iter([])
        for s in self.stdouts:
            yield iter(s)

    def test_copy_debug_universal(self):
        """
        Test that dumping symbols for multiple architectures only copies debug symbols once
        per file.
        """
        copied = []

        def mock_copy_debug(filename, debug_file, guid, code_file, code_id):
            copied.append(
                filename[len(self.symbol_dir) :]
                if filename.startswith(self.symbol_dir)
                else filename
            )

        self.add_test_files(add_extension(["foo"]))
        # Windows doesn't call file(1) to figure out if the file should be processed.
        if target_platform() != "WINNT":
            self.stdouts.append(file_output)
        self.stdouts.append(mock_dump_syms("X" * 33, add_extension(["foo"])[0]))
        self.stdouts.append(mock_dump_syms("Y" * 33, add_extension(["foo"])[0]))

        def mock_dsymutil(args, **kwargs):
            filename = args[-1]
            os.makedirs(filename + ".dSYM")
            return 0

        self.mock_call.side_effect = mock_dsymutil
        d = symbolstore.GetPlatformSpecificDumper(
            dump_syms="dump_syms",
            symbol_path=self.symbol_dir,
            copy_debug=True,
            archs="abc xyz",
        )
        d.CopyDebug = mock_copy_debug
        d.Process(os.path.join(self.test_dir, add_extension(["foo"])[0]))
        self.assertEqual(1, len(copied))

    def test_copy_debug_copies_binaries(self):
        """
        Test that CopyDebug copies binaries as well on Windows.
        """
        test_file = os.path.join(self.test_dir, "foo.dll")
        write_dll(test_file)
        code_file = "foo.dll"
        code_id = "abc123"
        self.stdouts.append(
            mock_dump_syms(
                "X" * 33, "foo.pdb", ["INFO CODE_ID %s %s" % (code_id, code_file)]
            )
        )

        def mock_compress(args, **kwargs):
            filename = args[-1]
            open(filename, "w").write("stuff")
            return 0

        self.mock_call.side_effect = mock_compress
        d = symbolstore.Dumper_Win32(
            dump_syms="dump_syms", symbol_path=self.symbol_dir, copy_debug=True
        )
        d.Process(test_file)
        self.assertTrue(
            os.path.isfile(os.path.join(self.symbol_dir, code_file, code_id, code_file))
        )


class TestGetVCSFilename(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)

    def tearDown(self):
        HelperMixin.tearDown(self)

    @patch("subprocess.Popen")
    def testVCSFilenameHg(self, mock_Popen):
        # mock calls to `hg parent` and `hg showconfig paths.default`
        mock_communicate = mock_Popen.return_value.communicate
        mock_communicate.side_effect = [
            ("abcd1234", ""),
            ("http://example.com/repo", ""),
        ]
        os.mkdir(os.path.join(self.test_dir, ".hg"))
        filename = os.path.join(self.test_dir, "foo.c")
        self.assertEqual(
            "hg:example.com/repo:foo.c:abcd1234",
            symbolstore.GetVCSFilename(filename, [self.test_dir])[0],
        )

    @patch("subprocess.Popen")
    def testVCSFilenameHgMultiple(self, mock_Popen):
        # mock calls to `hg parent` and `hg showconfig paths.default`
        mock_communicate = mock_Popen.return_value.communicate
        mock_communicate.side_effect = [
            ("abcd1234", ""),
            ("http://example.com/repo", ""),
            ("0987ffff", ""),
            ("http://example.com/other", ""),
        ]
        srcdir1 = os.path.join(self.test_dir, "one")
        srcdir2 = os.path.join(self.test_dir, "two")
        os.makedirs(os.path.join(srcdir1, ".hg"))
        os.makedirs(os.path.join(srcdir2, ".hg"))
        filename1 = os.path.join(srcdir1, "foo.c")
        filename2 = os.path.join(srcdir2, "bar.c")
        self.assertEqual(
            "hg:example.com/repo:foo.c:abcd1234",
            symbolstore.GetVCSFilename(filename1, [srcdir1, srcdir2])[0],
        )
        self.assertEqual(
            "hg:example.com/other:bar.c:0987ffff",
            symbolstore.GetVCSFilename(filename2, [srcdir1, srcdir2])[0],
        )

    def testVCSFilenameEnv(self):
        # repo URL and changeset read from environment variables if defined.
        os.environ["MOZ_SOURCE_REPO"] = "https://somewhere.com/repo"
        os.environ["MOZ_SOURCE_CHANGESET"] = "abcdef0123456"
        os.mkdir(os.path.join(self.test_dir, ".hg"))
        filename = os.path.join(self.test_dir, "foo.c")
        self.assertEqual(
            "hg:somewhere.com/repo:foo.c:abcdef0123456",
            symbolstore.GetVCSFilename(filename, [self.test_dir])[0],
        )


# SHA-512 of a zero-byte file
EMPTY_SHA512 = (
    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff"
)
EMPTY_SHA512 += "8318d2877eec2f63b931bd47417a81a538327af927da3e"


class TestGeneratedFilePath(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)

    def tearDown(self):
        HelperMixin.tearDown(self)

    def test_generated_file_path(self):
        # Make an empty generated file
        g = os.path.join(self.test_dir, "generated")
        rel_path = "a/b/generated"
        with open(g, "wb"):
            pass
        expected = "s3:bucket:{}/{}:".format(EMPTY_SHA512, rel_path)
        self.assertEqual(
            expected, symbolstore.get_generated_file_s3_path(g, rel_path, "bucket")
        )


if host_platform() == "WINNT":

    class TestRealpath(HelperMixin, unittest.TestCase):
        def test_realpath(self):
            # self.test_dir is going to be 8.3 paths...
            junk = os.path.join(self.test_dir, "x")
            with open(junk, "w") as o:
                o.write("x")
            fixed_dir = os.path.dirname(realpath(junk))
            files = [
                "one\\two.c",
                "three\\Four.d",
                "Five\\Six.e",
                "seven\\Eight\\nine.F",
            ]
            for rel_path in files:
                full_path = os.path.normpath(os.path.join(self.test_dir, rel_path))
                self.make_dirs(full_path)
                with open(full_path, "w") as o:
                    o.write("x")
                fixed_path = realpath(full_path.lower())
                fixed_path = os.path.relpath(fixed_path, fixed_dir)
                self.assertEqual(rel_path, fixed_path)


if target_platform() == "WINNT":

    class TestSourceServer(HelperMixin, unittest.TestCase):
        @patch("subprocess.call")
        @patch("subprocess.Popen")
        @patch.dict("buildconfig.substs._dict", {"PDBSTR": "pdbstr"})
        def test_HGSERVER(self, mock_Popen, mock_call):
            """
            Test that HGSERVER gets set correctly in the source server index.
            """
            symbolpath = os.path.join(self.test_dir, "symbols")
            os.makedirs(symbolpath)
            srcdir = os.path.join(self.test_dir, "srcdir")
            os.makedirs(os.path.join(srcdir, ".hg"))
            sourcefile = os.path.join(srcdir, "foo.c")
            test_files = add_extension(["foo"])
            self.add_test_files(test_files)
            # mock calls to `dump_syms`, `hg parent` and
            # `hg showconfig paths.default`
            mock_Popen.return_value.stdout = iter(
                [
                    "MODULE os x86 %s %s" % ("X" * 33, test_files[0]),
                    "FILE 0 %s" % sourcefile,
                    "PUBLIC xyz 123",
                ]
            )
            mock_Popen.return_value.wait.return_value = 0
            mock_communicate = mock_Popen.return_value.communicate
            mock_communicate.side_effect = [
                ("abcd1234", ""),
                ("http://example.com/repo", ""),
            ]
            # And mock the call to pdbstr to capture the srcsrv stream data.
            global srcsrv_stream
            srcsrv_stream = None

            def mock_pdbstr(args, cwd="", **kwargs):
                for arg in args:
                    if arg.startswith("-i:"):
                        global srcsrv_stream
                        srcsrv_stream = open(os.path.join(cwd, arg[3:]), "r").read()
                return 0

            mock_call.side_effect = mock_pdbstr
            d = symbolstore.GetPlatformSpecificDumper(
                dump_syms="dump_syms",
                symbol_path=symbolpath,
                srcdirs=[srcdir],
                vcsinfo=True,
                srcsrv=True,
                copy_debug=True,
            )
            # stub out CopyDebug
            d.CopyDebug = lambda *args: True
            d.Process(os.path.join(self.test_dir, test_files[0]))
            self.assertNotEqual(srcsrv_stream, None)
            hgserver = [
                x.rstrip()
                for x in srcsrv_stream.splitlines()
                if x.startswith("HGSERVER=")
            ]
            self.assertEqual(len(hgserver), 1)
            self.assertEqual(hgserver[0].split("=")[1], "http://example.com/repo")


class TestInstallManifest(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)
        self.srcdir = os.path.join(self.test_dir, "src")
        os.mkdir(self.srcdir)
        self.objdir = os.path.join(self.test_dir, "obj")
        os.mkdir(self.objdir)
        self.manifest = InstallManifest()
        self.canonical_mapping = {}
        for s in ["src1", "src2"]:
            srcfile = realpath(os.path.join(self.srcdir, s))
            objfile = realpath(os.path.join(self.objdir, s))
            self.canonical_mapping[objfile] = srcfile
            self.manifest.add_copy(srcfile, s)
        self.manifest_file = os.path.join(self.test_dir, "install-manifest")
        self.manifest.write(self.manifest_file)

    def testMakeFileMapping(self):
        """
        Test that valid arguments are validated.
        """
        arg = "%s,%s" % (self.manifest_file, self.objdir)
        ret = symbolstore.validate_install_manifests([arg])
        self.assertEqual(len(ret), 1)
        manifest, dest = ret[0]
        self.assertTrue(isinstance(manifest, InstallManifest))
        self.assertEqual(dest, self.objdir)

        file_mapping = symbolstore.make_file_mapping(ret)
        for obj, src in self.canonical_mapping.items():
            self.assertTrue(obj in file_mapping)
            self.assertEqual(file_mapping[obj], src)

    def testMissingFiles(self):
        """
        Test that missing manifest files or install directories give errors.
        """
        missing_manifest = os.path.join(self.test_dir, "missing-manifest")
        arg = "%s,%s" % (missing_manifest, self.objdir)
        with self.assertRaises(IOError) as e:
            symbolstore.validate_install_manifests([arg])
            self.assertEqual(e.filename, missing_manifest)

        missing_install_dir = os.path.join(self.test_dir, "missing-dir")
        arg = "%s,%s" % (self.manifest_file, missing_install_dir)
        with self.assertRaises(IOError) as e:
            symbolstore.validate_install_manifests([arg])
            self.assertEqual(e.filename, missing_install_dir)

    def testBadManifest(self):
        """
        Test that a bad manifest file give errors.
        """
        bad_manifest = os.path.join(self.test_dir, "bad-manifest")
        with open(bad_manifest, "w") as f:
            f.write("junk\n")
        arg = "%s,%s" % (bad_manifest, self.objdir)
        with self.assertRaises(IOError) as e:
            symbolstore.validate_install_manifests([arg])
            self.assertEqual(e.filename, bad_manifest)

    def testBadArgument(self):
        """
        Test that a bad manifest argument gives an error.
        """
        with self.assertRaises(ValueError):
            symbolstore.validate_install_manifests(["foo"])


class TestFileMapping(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)
        self.srcdir = os.path.join(self.test_dir, "src")
        os.mkdir(self.srcdir)
        self.objdir = os.path.join(self.test_dir, "obj")
        os.mkdir(self.objdir)
        self.symboldir = os.path.join(self.test_dir, "symbols")
        os.mkdir(self.symboldir)

    @patch("subprocess.Popen")
    def testFileMapping(self, mock_Popen):
        files = [("a/b", "mozilla/b"), ("c/d", "foo/d")]
        if os.sep != "/":
            files = [[f.replace("/", os.sep) for f in x] for x in files]
        file_mapping = {}
        dumped_files = []
        expected_files = []
        self.make_dirs(os.path.join(self.objdir, "x", "y"))
        for s, o in files:
            srcfile = os.path.join(self.srcdir, s)
            self.make_file(srcfile)
            expected_files.append(realpath(srcfile))
            objfile = os.path.join(self.objdir, o)
            self.make_file(objfile)
            file_mapping[realpath(objfile)] = realpath(srcfile)
            dumped_files.append(os.path.join(self.objdir, "x", "y", "..", "..", o))
        # mock the dump_syms output
        file_id = ("X" * 33, "somefile")

        def mk_output(files):
            return iter(
                ["MODULE os x86 %s %s\n" % file_id]
                + ["FILE %d %s\n" % (i, s) for i, s in enumerate(files)]
                + ["PUBLIC xyz 123\n"]
            )

        mock_Popen.return_value.stdout = mk_output(dumped_files)
        mock_Popen.return_value.wait.return_value = 0

        d = symbolstore.Dumper("dump_syms", self.symboldir, file_mapping=file_mapping)
        f = os.path.join(self.objdir, "somefile")
        open(f, "w").write("blah")
        d.Process(f)
        expected_output = "".join(mk_output(expected_files))
        symbol_file = os.path.join(
            self.symboldir, file_id[1], file_id[0], file_id[1] + ".sym"
        )
        self.assertEqual(open(symbol_file, "r").read(), expected_output)


class TestFunctional(HelperMixin, unittest.TestCase):
    """Functional tests of symbolstore.py, calling it with a real
    dump_syms binary and passing in a real binary to dump symbols from.

    Since the rest of the tests in this file mock almost everything and
    don't use the actual process pool like buildsymbols does, this tests
    that the way symbolstore.py gets called in buildsymbols works.
    """

    def setUp(self):
        HelperMixin.setUp(self)
        self.skip_test = False
        if buildconfig.substs["MOZ_BUILD_APP"] != "browser":
            self.skip_test = True
        if buildconfig.substs.get("ENABLE_STRIP"):
            self.skip_test = True
        # Bug 1608146.
        if buildconfig.substs.get("MOZ_CODE_COVERAGE"):
            self.skip_test = True
        self.topsrcdir = buildconfig.topsrcdir
        self.script_path = os.path.join(
            self.topsrcdir, "toolkit", "crashreporter", "tools", "symbolstore.py"
        )
        self.dump_syms = buildconfig.substs.get("DUMP_SYMS")
        if not self.dump_syms:
            self.skip_test = True

        if target_platform() == "WINNT":
            self.target_bin = os.path.join(
                buildconfig.topobjdir, "dist", "bin", "firefox.exe"
            )
        elif target_platform() == "Darwin":
            self.target_bin = os.path.join(
                buildconfig.topobjdir, "dist", "bin", "firefox"
            )
        else:
            self.target_bin = os.path.join(
                buildconfig.topobjdir, "dist", "bin", "firefox-bin"
            )

    def tearDown(self):
        HelperMixin.tearDown(self)

    def testSymbolstore(self):
        if self.skip_test:
            raise unittest.SkipTest("Skipping test in non-Firefox product")
        dist_include_manifest = os.path.join(
            buildconfig.topobjdir, "_build_manifests/install/dist_include"
        )
        dist_include = os.path.join(buildconfig.topobjdir, "dist/include")
        browser_app = os.path.join(buildconfig.topobjdir, "browser/app")
        output = subprocess.check_output(
            [
                sys.executable,
                self.script_path,
                "--vcs-info",
                "-s",
                self.topsrcdir,
                "--install-manifest=%s,%s" % (dist_include_manifest, dist_include),
                self.dump_syms,
                self.test_dir,
                self.target_bin,
            ],
            universal_newlines=True,
            stderr=None,
            cwd=browser_app,
        )
        lines = [l for l in output.splitlines() if l.strip()]
        self.assertEqual(
            1,
            len(lines),
            "should have one filename in the output; got %s" % repr(output),
        )
        symbol_file = os.path.join(self.test_dir, lines[0])
        self.assertTrue(os.path.isfile(symbol_file))
        symlines = open(symbol_file, "r").readlines()
        file_lines = [l for l in symlines if l.startswith("FILE")]

        def check_hg_path(lines, match):
            match_lines = [l for l in file_lines if match in l]
            self.assertTrue(
                len(match_lines) >= 1, "should have a FILE line for " + match
            )
            # Skip this check for local git repositories.
            if not os.path.isdir(mozpath.join(self.topsrcdir, ".hg")):
                return
            for line in match_lines:
                filename = line.split(None, 2)[2]
                self.assertEqual("hg:", filename[:3])

        # Check that nsBrowserApp.cpp is listed as a FILE line, and that
        # it was properly mapped to the source repo.
        check_hg_path(file_lines, "nsBrowserApp.cpp")
        # Also check Sprintf.h to verify that files from dist/include
        # are properly mapped.
        check_hg_path(file_lines, "mfbt/Sprintf.h")


if __name__ == "__main__":
    mozunit.main()
