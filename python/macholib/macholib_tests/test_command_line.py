from macholib import macho_dump
from macholib import macho_find
from macholib import _cmdline
from macholib import util

import sys
import shutil
import os

if sys.version_info[:2] <= (2,6):
    import unittest2 as unittest
else:
    import unittest

try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO

class TestCmdLine (unittest.TestCase):

    # This test is no longer valid:
    def no_test_main_is_shared(self):
        self.assertTrue(macho_dump.main is _cmdline.main)
        self.assertTrue(macho_find.main is _cmdline.main)

    def test_check_file(self):
        record = []
        def record_cb(fp, path):
            record.append((fp, path))

        self.assertEqual(_cmdline.check_file(sys.stdout, '/bin/sh', record_cb), 0)
        self.assertEqual(record, [(sys.stdout, '/bin/sh')])

        saved_stderr = sys.stderr
        saved_argv = sys.argv
        try:
            sys.stderr = StringIO()
            sys.argv = ['macho_test']

            record[:] = []
            self.assertEqual(_cmdline.check_file(sys.stdout, '/bin/no-shell', record_cb), 1)
            self.assertEqual(record, [])
            self.assertEqual(sys.stderr.getvalue(), "macho_test: /bin/no-shell: No such file or directory\n")
            self.assertEqual(record, [])

            shutil.copy('/bin/sh', 'test.exec')
            os.chmod('test.exec', 0)

            sys.stderr = StringIO()
            self.assertEqual(_cmdline.check_file(sys.stdout, 'test.exec', record_cb), 1)
            self.assertEqual(record, [])
            self.assertEqual(sys.stderr.getvalue(), "macho_test: test.exec: [Errno 13] Permission denied: 'test.exec'\n")
            self.assertEqual(record, [])


        finally:
            sys.stderr = sys.stderr
            sys.argv = saved_argv
            if os.path.exists('test.exec'):
                os.unlink('test.exec')

    def test_shared_main(self):

        saved_stderr = sys.stderr
        saved_argv = sys.argv
        try:
            sys.stderr = StringIO()

            sys.argv = ['macho_tool']

            self.assertEqual(_cmdline.main(lambda *args: None), 1)
            self.assertEqual(sys.stderr.getvalue(), 'Usage: macho_tool filename...\n')

            names = []
            def record_names(fp, name):
                self.assertEqual(fp, sys.stdout)
                names.append(name)


            sys.stderr = StringIO()
            sys.argv = ['macho_tool', '/bin/sh']
            self.assertEqual(_cmdline.main(record_names), 0)
            self.assertEqual(sys.stderr.getvalue(), '')
            self.assertEqual(names, ['/bin/sh'])

            names = []
            sys.stderr = StringIO()
            sys.argv = ['macho_tool', '/bin/sh', '/bin/ls']
            self.assertEqual(_cmdline.main(record_names), 0)
            self.assertEqual(sys.stderr.getvalue(), '')
            self.assertEqual(names, ['/bin/sh', '/bin/ls'])

            names = []
            sys.stderr = StringIO()
            sys.argv = ['macho_tool', '/bin']
            self.assertEqual(_cmdline.main(record_names), 0)
            self.assertEqual(sys.stderr.getvalue(), '')
            names.sort()
            dn = '/bin'
            real_names = [
                    os.path.join(dn, fn) for fn in os.listdir(dn)
                    if util.is_platform_file(os.path.join(dn, fn)) ]
            real_names.sort()

            self.assertEqual(names, real_names)

        finally:
            sys.stderr = saved_stderr
            sys.argv = saved_argv

    def test_macho_find(self):
        fp = StringIO()
        macho_find.print_file(fp, "file1")
        macho_find.print_file(fp, "file2")
        self.assertEqual(fp.getvalue(), "file1\nfile2\n")

    def test_macho_dump(self):
        fp = StringIO()
        macho_dump.print_file(fp, "/bin/sh")
        lines = fp.getvalue().splitlines()

        self.assertEqual(lines[0], "/bin/sh")
        self.assertTrue(len(lines) > 3)

        self.assertEqual(lines[-1], '')
        del lines[-1]

        idx = 1
        while idx < len(lines):
            self.assertTrue(lines[idx].startswith('    [MachOHeader endian'))
            idx+=1

            lc = 0
            while idx < len(lines):
                if not lines[idx].startswith('\t'):
                    break

                lc +=1
                self.assertTrue(os.path.exists(lines[idx].lstrip()))
                idx += 1

            self.assertTrue(lc > 1)


if __name__ == "__main__":
    unittest.main()
