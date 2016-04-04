#!/usr/bin/env python
#
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import buildconfig
import mozpack.path as mozpath
import mozunit
import subprocess
import unittest

class TestZip(unittest.TestCase):
    def test_zip(self):
        srcdir = mozpath.dirname(__file__)
        relsrcdir = mozpath.relpath(srcdir, buildconfig.topsrcdir)
        test_bin = mozpath.join(buildconfig.topobjdir, relsrcdir,
                                'TestZip' + buildconfig.substs['BIN_SUFFIX'])
        self.assertEqual(0, subprocess.call([test_bin, srcdir]))

if __name__ == '__main__':
    mozunit.main()
