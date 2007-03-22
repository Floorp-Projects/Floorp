#!python

# Copyright (c) 2004 Trent Mick

"""
    relic.py Regression Test Suite Harness

    Usage:
        python test.py [<options>...] [<tests>...]

    Options:
        -x <testname>, --exclude=<testname>
                        Exclude the named test from the set of tests to be
                        run.  This can be used multiple times to specify
                        multiple exclusions.
        -v, --verbose   run tests in verbose mode with output to stdout
        -q, --quiet     don't print anything except if a test fails
        -h, --help      print this text and exit

    This will find all modules whose name is "test_*" in the test
    directory, and run them.  Various command line options provide
    additional facilities.

    If non-option arguments are present, they are names for tests to run.
    If no test names are given, all tests are run.

    Test Setup Options:
        -c, --clean     Don't setup, just clean up the test workspace.
        -n, --no-clean  Don't clean up after setting up and running the
                        test suite.
"""

import os
import sys
import getopt
import glob
import time
import types
import tempfile
import unittest



#---- exceptions

class TestError(Exception):
    pass


#---- globals

gVerbosity = 2


#---- utility routines

def _rmtreeOnError(rmFunction, filePath, excInfo):
    if excInfo[0] == OSError:
        # presuming because file is read-only
        os.chmod(filePath, 0777)
        rmFunction(filePath)

def _rmtree(dirname):
    import shutil
    shutil.rmtree(dirname, 0, _rmtreeOnError)


def _getAllTests(testDir):
    """Return a list of all tests to run."""
    testPyFiles = glob.glob(os.path.join(testDir, "test_*.py"))
    modules = [f[:-3] for f in testPyFiles if f and f.endswith(".py")]

    packages = []
    for f in glob.glob(os.path.join(testDir, "test_*")):
        if os.path.isdir(f) and "." not in f:
            if os.path.isfile(os.path.join(testDir, f, "__init__.py")):
                packages.append(f)

    return modules + packages


def _setUp():
    # Ensure the *development* check is tested.
    topDir = os.path.abspath(os.pardir)
    sys.path.insert(0, topDir)
    print "Setup to test:"
    import relic
    ver = "%s.%s.%s" % relic._version_
    print "relic %s at '%s'" % (ver, relic.__file__)
    print "-"*70 + '\n'
    

def _tearDown():
    pass


def test(testModules, testDir=os.curdir, exclude=[]):
    """Run the given regression tests and report the results."""
    # Determine the test modules to run.
    if not testModules:
        testModules = _getAllTests(testDir)
    testModules = [t for t in testModules if t not in exclude]

    # Aggregate the TestSuite's from each module into one big one.
    allSuites = []
    for moduleFile in testModules:
        module = __import__(moduleFile, globals(), locals(), [])
        suite = getattr(module, "suite", None)
        if suite is not None:
            allSuites.append(suite())
        else:
            if gVerbosity >= 2:
                print "WARNING: module '%s' did not have a suite() method."\
                      % moduleFile
    suite = unittest.TestSuite(allSuites)

    # Run the suite.
    runner = unittest.TextTestRunner(sys.stdout, verbosity=gVerbosity)
    result = runner.run(suite)


#---- mainline

def main(argv):
    testDir = os.path.dirname(sys.argv[0])

    # parse options
    global gVerbosity
    try:
        opts, testModules = getopt.getopt(sys.argv[1:], 'hvqx:cn',
            ['help', 'verbose', 'quiet', 'exclude=', 'clean',
             'no-clean'])
    except getopt.error, ex:
        print "%s: ERROR: %s" % (argv[0], ex)
        print __doc__
        sys.exit(2)  
    exclude = []
    setupOpts = {}
    justClean = 0
    clean = 1
    for opt, optarg in opts:
        if opt in ("-h", "--help"):
            print __doc__
            sys.exit(0)
        elif opt in ("-v", "--verbose"):
            gVerbosity += 1
        elif opt in ("-q", "--quiet"):
            gVerbosity -= 1
        elif opt in ("-x", "--exclude"):
            exclude.append(optarg)
        elif opt in ("-c", "--clean"):
            justClean = 1
        elif opt in ("-n", "--no-clean"):
            clean = 0

    retval = None
    if not justClean:
        _setUp(**setupOpts)
    try:
        if not justClean:
            retval = test(testModules, testDir=testDir, exclude=exclude)
    finally:
        if clean:
            _tearDown(**setupOpts)
    return retval

if __name__ == '__main__':
    sys.exit( main(sys.argv) )

