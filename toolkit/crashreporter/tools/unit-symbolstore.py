#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, tempfile, unittest, shutil, struct, platform, subprocess
import symbolstore

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
        if not self.test_dir.endswith("/"):
            self.test_dir += "/"
        
    def tearDown(self):
        shutil.rmtree(self.test_dir)

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
        def mock_process_file(filename):
            processed.append((filename[len(self.test_dir):] if filename.startswith(self.test_dir) else filename).replace('\\', '/'))
            return True
        self.add_test_files(add_extension(["foo", "bar", "abc/xyz", "abc/fooxyz", "def/asdf", "def/xyzfoo"]))
        d = symbolstore.GetPlatformSpecificDumper(dump_syms="dump_syms",
                                                  symbol_path="symbol_path",
                                                  exclude=["*foo*"])
        d.ProcessFile = mock_process_file
        self.assertTrue(d.Process(self.test_dir))
        processed.sort()
        expected = add_extension(["bar", "abc/xyz", "def/asdf"])
        expected.sort()
        self.assertEqual(processed, expected)

    def test_exclude_filenames(self):
        """
        Test that excluding a filename without a wildcard works.
        """
        processed = []
        def mock_process_file(filename):
            processed.append((filename[len(self.test_dir):] if filename.startswith(self.test_dir) else filename).replace('\\', '/'))
            return True
        self.add_test_files(add_extension(["foo", "bar", "abc/foo", "abc/bar", "def/foo", "def/bar"]))
        d = symbolstore.GetPlatformSpecificDumper(dump_syms="dump_syms",
                                                  symbol_path="symbol_path",
                                                  exclude=add_extension(["foo"]))
        d.ProcessFile = mock_process_file
        self.assertTrue(d.Process(self.test_dir))
        processed.sort()
        expected = add_extension(["bar", "abc/bar", "def/bar"])
        expected.sort()
        self.assertEqual(processed, expected)

def popen_factory(stdouts):
    """
    Generate a class that can mock subprocess.Popen. |stdouts| is an iterable that
    should return an iterable for the stdout of each process in turn.
    """
    class mock_popen(object):
        def __init__(self, args, *args_rest, **kwargs):
            self.stdout = stdouts.next()

        def wait(self):
            return 0
    return mock_popen

def mock_dump_syms(module_id, filename):
    return ["MODULE os x86 %s %s" % (module_id, filename),
            "FILE 0 foo.c",
            "PUBLIC xyz 123"]

class TestCopyDebugUniversal(HelperMixin, unittest.TestCase):
    """
    Test that CopyDebug does the right thing when dumping multiple architectures.
    """
    def setUp(self):
        HelperMixin.setUp(self)
        self.symbol_dir = tempfile.mkdtemp()
        self._subprocess_call = subprocess.call
        subprocess.call = self.mock_call
        self._subprocess_popen = subprocess.Popen
        subprocess.Popen = popen_factory(self.next_mock_stdout())
        self.stdouts = []
        
    def tearDown(self):
        HelperMixin.tearDown(self)
        shutil.rmtree(self.symbol_dir)
        subprocess.call = self._subprocess_call
        subprocess.Popen = self._subprocess_popen

    def mock_call(self, args, **kwargs):
        if args[0].endswith("dsymutil"):
            filename = args[-1]
            os.makedirs(filename + ".dSYM")
        return 0

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
        def mock_copy_debug(filename, debug_file, guid):
            copied.append(filename[len(self.symbol_dir):] if filename.startswith(self.symbol_dir) else filename)
        self.add_test_files(add_extension(["foo"]))
        self.stdouts.append(mock_dump_syms("X" * 33, add_extension(["foo"])[0]))
        self.stdouts.append(mock_dump_syms("Y" * 33, add_extension(["foo"])[0]))
        d = symbolstore.GetPlatformSpecificDumper(dump_syms="dump_syms",
                                                  symbol_path=self.symbol_dir,
                                                  copy_debug=True,
                                                  archs="abc xyz")
        d.CopyDebug = mock_copy_debug
        self.assertTrue(d.Process(self.test_dir))
        self.assertEqual(1, len(copied))

if __name__ == '__main__':
  unittest.main()
