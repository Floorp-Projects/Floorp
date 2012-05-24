#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os, tempfile, unittest, shutil, struct, platform
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

class TestExclude(unittest.TestCase):
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

if __name__ == '__main__':
  unittest.main()
