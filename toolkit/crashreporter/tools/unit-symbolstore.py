#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, tempfile, unittest, shutil, struct, platform, subprocess, multiprocessing.dummy
import mock
from mock import patch
import symbolstore

from mozpack.manifests import InstallManifest

# Some simple functions to mock out files that the platform-specific dumpers will accept.
# dump_syms itself will not be run (we mock that call out), but we can't override
# the ShouldProcessFile method since we actually want to test that.
def write_elf(filename):
    open(filename, "wb").write(struct.pack("<7B45x", 0x7f, ord("E"), ord("L"), ord("F"), 1, 1, 1))

def write_macho(filename):
    open(filename, "wb").write(struct.pack("<I28x", 0xfeedface))

def write_pdb(filename):
    open(filename, "w").write("aaa")
    # write out a fake DLL too
    open(os.path.splitext(filename)[0] + ".dll", "w").write("aaa")

writer = {'Windows': write_pdb,
          'Microsoft': write_pdb,
          'Linux': write_elf,
          'Sunos5': write_elf,
          'Darwin': write_macho}[platform.system()]
extension = {'Windows': ".pdb",
             'Microsoft': ".pdb",
             'Linux': ".so",
             'Sunos5': ".so",
             'Darwin': ".dylib"}[platform.system()]

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

    def tearDown(self):
        shutil.rmtree(self.test_dir)
        symbolstore.srcdirRepoInfo = {}
        symbolstore.vcsFileInfoCache = {}

    def add_test_files(self, files):
        for f in files:
            f = os.path.join(self.test_dir, f)
            d = os.path.dirname(f)
            if d and not os.path.exists(d):
                os.makedirs(d)
            writer(f)

class TestExclude(HelperMixin, unittest.TestCase):
    def test_exclude_wildcard(self):
        """
        Test that using an exclude list with a wildcard pattern works.
        """
        processed = []
        def mock_process_file(filenames):
            for filename in filenames:
                processed.append((filename[len(self.test_dir):] if filename.startswith(self.test_dir) else filename).replace('\\', '/'))
            return True
        self.add_test_files(add_extension(["foo", "bar", "abc/xyz", "abc/fooxyz", "def/asdf", "def/xyzfoo"]))
        d = symbolstore.GetPlatformSpecificDumper(dump_syms="dump_syms",
                                                  symbol_path="symbol_path",
                                                  exclude=["*foo*"])
        d.ProcessFiles = mock_process_file
        d.Process(self.test_dir)
        d.Finish(stop_pool=False)
        processed.sort()
        expected = add_extension(["bar", "abc/xyz", "def/asdf"])
        expected.sort()
        self.assertEqual(processed, expected)

    def test_exclude_filenames(self):
        """
        Test that excluding a filename without a wildcard works.
        """
        processed = []
        def mock_process_file(filenames):
            for filename in filenames:
                processed.append((filename[len(self.test_dir):] if filename.startswith(self.test_dir) else filename).replace('\\', '/'))
            return True
        self.add_test_files(add_extension(["foo", "bar", "abc/foo", "abc/bar", "def/foo", "def/bar"]))
        d = symbolstore.GetPlatformSpecificDumper(dump_syms="dump_syms",
                                                  symbol_path="symbol_path",
                                                  exclude=add_extension(["foo"]))
        d.ProcessFiles = mock_process_file
        d.Process(self.test_dir)
        d.Finish(stop_pool=False)
        processed.sort()
        expected = add_extension(["bar", "abc/bar", "def/bar"])
        expected.sort()
        self.assertEqual(processed, expected)

def mock_dump_syms(module_id, filename, extra=[]):
    return ["MODULE os x86 %s %s" % (module_id, filename)
            ] + extra + [
            "FILE 0 foo.c",
            "PUBLIC xyz 123"]


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
            m.stdout = stdout_iter.next()
            m.wait.return_value = 0
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
            copied.append(filename[len(self.symbol_dir):] if filename.startswith(self.symbol_dir) else filename)
        self.add_test_files(add_extension(["foo"]))
        self.stdouts.append(mock_dump_syms("X" * 33, add_extension(["foo"])[0]))
        self.stdouts.append(mock_dump_syms("Y" * 33, add_extension(["foo"])[0]))
        def mock_dsymutil(args, **kwargs):
            filename = args[-1]
            os.makedirs(filename + ".dSYM")
            return 0
        self.mock_call.side_effect = mock_dsymutil
        d = symbolstore.GetPlatformSpecificDumper(dump_syms="dump_syms",
                                                  symbol_path=self.symbol_dir,
                                                  copy_debug=True,
                                                  archs="abc xyz")
        d.CopyDebug = mock_copy_debug
        d.Process(self.test_dir)
        d.Finish(stop_pool=False)
        self.assertEqual(1, len(copied))

    def test_copy_debug_copies_binaries(self):
        """
        Test that CopyDebug copies binaries as well on Windows.
        """
        test_file = os.path.join(self.test_dir, 'foo.pdb')
        write_pdb(test_file)
        code_file = 'foo.dll'
        code_id = 'abc123'
        self.stdouts.append(mock_dump_syms('X' * 33, 'foo.pdb',
                                           ['INFO CODE_ID %s %s' % (code_id, code_file)]))
        def mock_compress(args, **kwargs):
            filename = args[-1]
            open(filename, 'w').write('stuff')
            return 0
        self.mock_call.side_effect = mock_compress
        d = symbolstore.Dumper_Win32(dump_syms='dump_syms',
                                     symbol_path=self.symbol_dir,
                                     copy_debug=True)
        d.FixFilenameCase = lambda f: f
        d.Process(self.test_dir)
        d.Finish(stop_pool=False)
        self.assertTrue(os.path.isfile(os.path.join(self.symbol_dir, code_file, code_id, code_file[:-1] + '_')))

class TestGetVCSFilename(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)

    def tearDown(self):
        HelperMixin.tearDown(self)

    @patch("subprocess.Popen")
    def testVCSFilenameHg(self, mock_Popen):
        # mock calls to `hg parent` and `hg showconfig paths.default`
        mock_communicate = mock_Popen.return_value.communicate
        mock_communicate.side_effect = [("abcd1234", ""),
                                        ("http://example.com/repo", "")]
        os.mkdir(os.path.join(self.test_dir, ".hg"))
        filename = os.path.join(self.test_dir, "foo.c")
        self.assertEqual("hg:example.com/repo:foo.c:abcd1234",
                         symbolstore.GetVCSFilename(filename, [self.test_dir])[0])

    @patch("subprocess.Popen")
    def testVCSFilenameHgMultiple(self, mock_Popen):
        # mock calls to `hg parent` and `hg showconfig paths.default`
        mock_communicate = mock_Popen.return_value.communicate
        mock_communicate.side_effect = [("abcd1234", ""),
                                        ("http://example.com/repo", ""),
                                        ("0987ffff", ""),
                                        ("http://example.com/other", "")]
        srcdir1 = os.path.join(self.test_dir, "one")
        srcdir2 = os.path.join(self.test_dir, "two")
        os.makedirs(os.path.join(srcdir1, ".hg"))
        os.makedirs(os.path.join(srcdir2, ".hg"))
        filename1 = os.path.join(srcdir1, "foo.c")
        filename2 = os.path.join(srcdir2, "bar.c")
        self.assertEqual("hg:example.com/repo:foo.c:abcd1234",
                         symbolstore.GetVCSFilename(filename1, [srcdir1, srcdir2])[0])
        self.assertEqual("hg:example.com/other:bar.c:0987ffff",
                         symbolstore.GetVCSFilename(filename2, [srcdir1, srcdir2])[0])

class TestRepoManifest(HelperMixin, unittest.TestCase):
    def testRepoManifest(self):
        manifest = os.path.join(self.test_dir, "sources.xml")
        open(manifest, "w").write("""<?xml version="1.0" encoding="UTF-8"?>
<manifest>
<remote fetch="http://example.com/foo/" name="foo"/>
<remote fetch="git://example.com/bar/" name="bar"/>
<default remote="bar"/>
<project name="projects/one" revision="abcd1234"/>
<project name="projects/two" path="projects/another" revision="ffffffff" remote="foo"/>
<project name="something_else" revision="00000000" remote="bar"/>
</manifest>
""")
        # Use a source file from each of the three projects
        file1 = os.path.join(self.test_dir, "projects", "one", "src1.c")
        file2 = os.path.join(self.test_dir, "projects", "another", "src2.c")
        file3 = os.path.join(self.test_dir, "something_else", "src3.c")
        d = symbolstore.Dumper("dump_syms", "symbol_path",
                               repo_manifest=manifest)
        self.assertEqual("git:example.com/bar/projects/one:src1.c:abcd1234",
                         symbolstore.GetVCSFilename(file1, d.srcdirs)[0])
        self.assertEqual("git:example.com/foo/projects/two:src2.c:ffffffff",
                         symbolstore.GetVCSFilename(file2, d.srcdirs)[0])
        self.assertEqual("git:example.com/bar/something_else:src3.c:00000000",
                         symbolstore.GetVCSFilename(file3, d.srcdirs)[0])

if platform.system() in ("Windows", "Microsoft"):
    class TestSourceServer(HelperMixin, unittest.TestCase):
        @patch("subprocess.call")
        @patch("subprocess.Popen")
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
            # srcsrv needs PDBSTR_PATH set
            os.environ["PDBSTR_PATH"] = "pdbstr"
            # mock calls to `dump_syms`, `hg parent` and
            # `hg showconfig paths.default`
            mock_Popen.return_value.stdout = iter([
                    "MODULE os x86 %s %s" % ("X" * 33, test_files[0]),
                    "FILE 0 %s" % sourcefile,
                    "PUBLIC xyz 123"
                    ])
            mock_communicate = mock_Popen.return_value.communicate
            mock_communicate.side_effect = [("abcd1234", ""),
                                            ("http://example.com/repo", ""),
                                            ]
            # And mock the call to pdbstr to capture the srcsrv stream data.
            global srcsrv_stream
            srcsrv_stream = None
            def mock_pdbstr(args, cwd="", **kwargs):
                for arg in args:
                    if arg.startswith("-i:"):
                        global srcsrv_stream
                        srcsrv_stream = open(os.path.join(cwd, arg[3:]), 'r').read()
                return 0
            mock_call.side_effect = mock_pdbstr
            d = symbolstore.GetPlatformSpecificDumper(dump_syms="dump_syms",
                                                      symbol_path=symbolpath,
                                                      srcdirs=[srcdir],
                                                      vcsinfo=True,
                                                      srcsrv=True,
                                                      copy_debug=True)
            # stub out CopyDebug
            d.CopyDebug = lambda *args: True
            d.Process(self.test_dir)
            d.Finish(stop_pool=False)
            self.assertNotEqual(srcsrv_stream, None)
            hgserver = [x.rstrip() for x in srcsrv_stream.splitlines() if x.startswith("HGSERVER=")]
            self.assertEqual(len(hgserver), 1)
            self.assertEqual(hgserver[0].split("=")[1], "http://example.com/repo")

class TestInstallManifest(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)
        self.srcdir = os.path.join(self.test_dir, 'src')
        os.mkdir(self.srcdir)
        self.objdir = os.path.join(self.test_dir, 'obj')
        os.mkdir(self.objdir)
        self.manifest = InstallManifest()
        self.canonical_mapping = {}
        for s in ['src1', 'src2']:
            srcfile = os.path.join(self.srcdir, s)
            objfile = os.path.join(self.objdir, s)
            self.canonical_mapping[objfile] = srcfile
            self.manifest.add_copy(srcfile, s)
        self.manifest_file = os.path.join(self.test_dir, 'install-manifest')
        self.manifest.write(self.manifest_file)

    def testMakeFileMapping(self):
        '''
        Test that valid arguments are validated.
        '''
        arg = '%s,%s' % (self.manifest_file, self.objdir)
        ret = symbolstore.validate_install_manifests([arg])
        self.assertEqual(len(ret), 1)
        manifest, dest = ret[0]
        self.assertTrue(isinstance(manifest, InstallManifest))
        self.assertEqual(dest, self.objdir)

        file_mapping = symbolstore.make_file_mapping(ret)
        for obj, src in self.canonical_mapping.iteritems():
            self.assertTrue(obj in file_mapping)
            self.assertEqual(file_mapping[obj], src)

    def testMissingFiles(self):
        '''
        Test that missing manifest files or install directories give errors.
        '''
        missing_manifest = os.path.join(self.test_dir, 'missing-manifest')
        arg = '%s,%s' % (missing_manifest, self.objdir)
        with self.assertRaises(IOError) as e:
            symbolstore.validate_install_manifests([arg])
            self.assertEqual(e.filename, missing_manifest)

        missing_install_dir = os.path.join(self.test_dir, 'missing-dir')
        arg = '%s,%s' % (self.manifest_file, missing_install_dir)
        with self.assertRaises(IOError) as e:
            symbolstore.validate_install_manifests([arg])
            self.assertEqual(e.filename, missing_install_dir)

    def testBadManifest(self):
        '''
        Test that a bad manifest file give errors.
        '''
        bad_manifest = os.path.join(self.test_dir, 'bad-manifest')
        with open(bad_manifest, 'wb') as f:
            f.write('junk\n')
        arg = '%s,%s' % (bad_manifest, self.objdir)
        with self.assertRaises(IOError) as e:
            symbolstore.validate_install_manifests([arg])
            self.assertEqual(e.filename, bad_manifest)

    def testBadArgument(self):
        '''
        Test that a bad manifest argument gives an error.
        '''
        with self.assertRaises(ValueError) as e:
            symbolstore.validate_install_manifests(['foo'])

class TestFileMapping(HelperMixin, unittest.TestCase):
    def setUp(self):
        HelperMixin.setUp(self)
        self.srcdir = os.path.join(self.test_dir, 'src')
        os.mkdir(self.srcdir)
        self.objdir = os.path.join(self.test_dir, 'obj')
        os.mkdir(self.objdir)
        self.symboldir = os.path.join(self.test_dir, 'symbols')
        os.mkdir(self.symboldir)

    @patch("subprocess.Popen")
    def testFileMapping(self, mock_Popen):
        files = [('a/b', 'mozilla/b'),
                 ('c/d', 'foo/d')]
        if os.sep != '/':
            files = [[f.replace('/', os.sep) for f in x] for x in files]
        file_mapping = {}
        dumped_files = []
        expected_files = []
        for s, o in files:
            srcfile = os.path.join(self.srcdir, s)
            expected_files.append(srcfile)
            file_mapping[os.path.join(self.objdir, o)] = srcfile
            dumped_files.append(os.path.join(self.objdir, 'x', 'y',
                                             '..', '..', o))
        # mock the dump_syms output
        file_id = ("X" * 33, 'somefile')
        def mk_output(files):
            return iter(
                [
                    'MODULE os x86 %s %s\n' % file_id
                ] +
                [
                    'FILE %d %s\n' % (i,s) for i, s in enumerate(files)
                ] +
                [
                    'PUBLIC xyz 123\n'
                ]
            )
        mock_Popen.return_value.stdout = mk_output(dumped_files)

        d = symbolstore.Dumper('dump_syms', self.symboldir,
                               file_mapping=file_mapping)
        f = os.path.join(self.objdir, 'somefile')
        open(f, 'wb').write('blah')
        d.Process(f)
        d.Finish(stop_pool=False)
        expected_output = ''.join(mk_output(expected_files))
        symbol_file = os.path.join(self.symboldir,
                                   file_id[1], file_id[0], file_id[1] + '.sym')
        self.assertEqual(open(symbol_file, 'r').read(), expected_output)

if __name__ == '__main__':
    # use the multiprocessing.dummy module to use threading wrappers so
    # that our mocking/module-patching works
    symbolstore.Dumper.GlobalInit(module=multiprocessing.dummy)

    unittest.main()

    symbolstore.Dumper.pool.close()
    symbolstore.Dumper.pool.join()
