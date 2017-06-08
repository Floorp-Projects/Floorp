#!/usr/bin/env python
# Copyright (c) 2002-2003 ActiveState Corp.
# Author: Trent Mick (TrentM@ActiveState.com)

"""Test suite for which.py."""

import sys
import os
import re
import tempfile
import unittest

import testsupport

#XXX:TODO
#   - def test_registry_success(self): ...App Paths setting
#   - def test_registry_noexist(self):
#   - test all the other options
#   - test on linux
#   - test the module API

class WhichTestCase(unittest.TestCase):
    def setUp(self):
        """Create a temp directory with a couple test "commands".
        The temp dir can be added to the PATH, etc, for testing purposes.
        """
        # Find the which.py to call.
        whichPy = os.path.join(os.path.dirname(__file__),
                               os.pardir, "which.py")
        self.which = sys.executable + " " + whichPy

        # Setup the test environment.
        self.tmpdir = tempfile.mktemp()
        os.makedirs(self.tmpdir)
        if sys.platform.startswith("win"):
            self.testapps = ['whichtestapp1.exe',
                             'whichtestapp2.exe',
                             'whichtestapp3.wta']
        else:
            self.testapps = ['whichtestapp1', 'whichtestapp2']
        for app in self.testapps:
            path = os.path.join(self.tmpdir, app)
            open(path, 'wb').write('\n')
            os.chmod(path, 0755)

    def tearDown(self):
        testsupport.rmtree(self.tmpdir)

    def test_opt_h(self):
        output, error, retval = testsupport.run(self.which+' --h')
        token = 'Usage:'
        self.failUnless(output.find(token) != -1,
                        "'%s' was not found in 'which -h' output: '%s' "\
                        % (token, output))
        self.failUnless(retval == 0,
                        "'which -h' did not return 0: retval=%d" % retval)

    def test_opt_help(self):
        output, error, retval = testsupport.run(self.which+' --help')
        token = 'Usage:'
        self.failUnless(output.find(token) != -1,
                        "'%s' was not found in 'which --help' output: '%s' "\
                        % (token, output))
        self.failUnless(retval == 0,
                        "'which --help' did not return 0: retval=%d" % retval)

    def test_opt_version(self):
        output, error, retval = testsupport.run(self.which+' --version')
        versionRe = re.compile("^which \d+\.\d+\.\d+$")
        versionMatch = versionRe.search(output.strip())
        self.failUnless(versionMatch,
                        "Version, '%s', from 'which --version' does not "\
                        "match pattern, '%s'."\
                        % (output.strip(), versionRe.pattern))
        self.failUnless(retval == 0,
                        "'which --version' did not return 0: retval=%d"\
                        % retval)

    def test_no_args(self):
        output, error, retval = testsupport.run(self.which)
        self.failUnless(retval == -1,
                        "'which' with no args should return -1: retval=%d"\
                        % retval)

    def test_one_failure(self):
        output, error, retval = testsupport.run(
            self.which+' whichtestapp1')
        self.failUnless(retval == 1,
            "One failure did not return 1: retval=%d" % retval)

    def test_two_failures(self):
        output, error, retval = testsupport.run(
            self.which+' whichtestapp1 whichtestapp2')
        self.failUnless(retval == 2,
            "Two failures did not return 2: retval=%d" % retval)

    def _match(self, path1, path2):
        #print "_match: %r =?= %r" % (path1, path2)
        if sys.platform.startswith('win'):
            path1 = os.path.normpath(os.path.normcase(path1))
            path2 = os.path.normpath(os.path.normcase(path2))
            path1 = os.path.splitext(path1)[0]
            path2 = os.path.splitext(path2)[0]
            return path1 == path2
        else:
            return os.path.samefile(path1, path2)

    def test_one_success(self):
        os.environ["PATH"] += os.pathsep + self.tmpdir 
        output, error, retval = testsupport.run(self.which+' -q whichtestapp1')
        expectedOutput = os.path.join(self.tmpdir, "whichtestapp1")
        self.failUnless(self._match(output.strip(), expectedOutput),
            "Output, %r, and expected output, %r, do not match."\
            % (output.strip(), expectedOutput))
        self.failUnless(retval == 0,
            "'which ...' should have returned 0: retval=%d" % retval)

    def test_two_successes(self):
        os.environ["PATH"] += os.pathsep + self.tmpdir 
        apps = ['whichtestapp1', 'whichtestapp2']
        output, error, retval = testsupport.run(
            self.which + ' -q ' + ' '.join(apps))
        lines = output.strip().split("\n")
        for app, line in zip(apps, lines):
            expected = os.path.join(self.tmpdir, app)
            self.failUnless(self._match(line, expected),
                "Output, %r, and expected output, %r, do not match."\
                % (line, expected))
        self.failUnless(retval == 0,
            "'which ...' should have returned 0: retval=%d" % retval)

    if sys.platform.startswith("win"):
        def test_PATHEXT_failure(self):
            os.environ["PATH"] += os.pathsep + self.tmpdir 
            output, error, retval = testsupport.run(self.which+' whichtestapp3')
            self.failUnless(retval == 1,
                "'which ...' should have returned 1: retval=%d" % retval)

        def test_PATHEXT_success(self):
            os.environ["PATH"] += os.pathsep + self.tmpdir 
            os.environ["PATHEXT"] += os.pathsep + '.wta'
            output, error, retval = testsupport.run(self.which+' whichtestapp3')
            expectedOutput = os.path.join(self.tmpdir, "whichtestapp3")
            self.failUnless(self._match(output.strip(), expectedOutput),
                "Output, %r, and expected output, %r, do not match."\
                % (output.strip(), expectedOutput))
            self.failUnless(retval == 0,
                "'which ...' should have returned 0: retval=%d" % retval)

        def test_exts(self):
            os.environ["PATH"] += os.pathsep + self.tmpdir 
            output, error, retval = testsupport.run(self.which+' -e .wta whichtestapp3')
            expectedOutput = os.path.join(self.tmpdir, "whichtestapp3")
            self.failUnless(self._match(output.strip(), expectedOutput),
                "Output, %r, and expected output, %r, do not match."\
                % (output.strip(), expectedOutput))
            self.failUnless(retval == 0,
                "'which ...' should have returned 0: retval=%d" % retval)



def suite():
    """Return a unittest.TestSuite to be used by test.py."""
    return unittest.makeSuite(WhichTestCase)

if __name__ == "__main__":
    unittest.main()

