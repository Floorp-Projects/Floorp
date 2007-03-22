#!/usr/bin/env python
#
# Copyright (c) 2004 Trent Mick

"""Test relicensing of inputs/... with relic.py."""

import sys
import os
import unittest
import difflib
import pprint
import shutil
import StringIO

import testsupport

#---- globals

gInputsDir = "relicense_inputs"
gOutputsDir = "relicense_outputs"
gTmpDir = "relicense_tmp"


#----- test cases

class RelicInputsTestCase(unittest.TestCase):
    def setUp(self):
        if not os.path.exists(gTmpDir):
            os.mkdir(gTmpDir)

    def tearDown(self):
        testsupport.rmtree(gTmpDir)


def _testOneInputFile(self, fname):
    import relic
    _debug = 0  # Set to true to dump status info for each test run.

    infile = os.path.join(gInputsDir, fname) # input
    outfile = os.path.join(gOutputsDir, fname) # expected output
    tmpfile = os.path.join(gTmpDir, fname) # actual output
    errfile = os.path.join(gOutputsDir, fname+'.error')  # expected error
    # An options file is a set of kwargs for the relic.relicense()
    # method call. One key-value pair per-line like this:
    #   key=value
    # Whitespace is stripped off the value.
    optsfile = os.path.join(gInputsDir, fname+'.options') # input options

    if _debug:
        print
        print "*"*50, "relic '%s'" % fname

    # Determine input options to use, if any.
    opts = {}
    if os.path.exists(optsfile):
        for line in open(optsfile, 'r').read().splitlines(0):
            name, value = line.split('=', 1)
            value = value.strip()
            try: # allow value to be a type other than string
                value = eval(value)
            except Exception:
                pass
            opts[name] = value
        if _debug:
            print "*"*50, "options"
            pprint.pprint(opts)

    # Copy the input file to the tmp location where relicensing is done.
    shutil.copy(infile, tmpfile)

    # Relicense the file, capturing stdout and stderr and any possible
    # error.
    oldStdout = sys.stdout
    oldStderr = sys.stderr
    sys.stdout = StringIO.StringIO()
    sys.stderr = StringIO.StringIO()
    try:
        try:
            relic.relicense([tmpfile], **opts)
        except relic.RelicError, ex:
            error = ex
        else:
            error = None
    finally:
        stdout = sys.stdout.getvalue()
        stderr = sys.stderr.getvalue()
        sys.stdout = oldStdout
        sys.stderr = oldStderr
    if _debug:
        print "*"*50, "stdout"
        print stdout
        print "*"*50, "stderr"
        print stderr
        print "*"*50, "error"
        print str(error)
        print "*" * 50

    # Verify that the results are as expected.
    if os.path.exists(outfile) and error:
        self.fail("relicensing '%s' raised an error but success was "
                  "expected: error='%s'" % (fname, str(error)))
    elif os.path.exists(outfile):
        expected = open(outfile, 'r').readlines()
        actual = open(tmpfile, 'r').readlines()
        if expected != actual:
            diff = list(difflib.ndiff(expected, actual))
            self.fail("%r != %r:\n%s"\
                      % (outfile, tmpfile, pprint.pformat(diff)))
    elif os.path.exists(errfile):
        # There is no reference output file. This means that processing
        # this file is expected to fail.
        expectedError = open(errfile, 'r').read()
        actualError = str(error)
        self.failUnlessEqual(actualError.strip(), expectedError.strip())
    else:
        self.fail("No reference ouput file or error file for '%s'." % infile)

    # Ensure next test file gets a clean relic.
    del sys.modules['relic']
        

#for fname in ["separated_license_comment_blocks.pl"]:
for fname in os.listdir(gInputsDir):
    if fname.endswith(".options"): continue # skip input option files
    testFunction = lambda self, fname=fname: _testOneInputFile(self, fname)
    name = 'test_relicense_'+fname
    setattr(RelicInputsTestCase, name, testFunction)


#---- mainline

def suite():
    """Return a unittest.TestSuite to be used by test.py."""
    return unittest.makeSuite(RelicInputsTestCase)

if __name__ == "__main__":
    runner = unittest.TextTestRunner(sys.stdout, verbosity=2)
    result = runner.run(suite())

