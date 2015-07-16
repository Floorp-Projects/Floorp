#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
run mozbase tests from a manifest,
by default https://github.com/mozilla/mozbase/blob/master/test-manifest.ini
"""

import imp
import manifestparser
import mozinfo
import optparse
import os
import sys
import unittest

import mozlog
from moztest.results import TestResultCollection
from moztest.adapters.unit import StructuredTestRunner

here = os.path.dirname(os.path.abspath(__file__))

def unittests(path):
    """return the unittests in a .py file"""

    path = os.path.abspath(path)
    unittests = []
    assert os.path.exists(path)
    directory = os.path.dirname(path)
    sys.path.insert(0, directory) # insert directory into path for top-level imports
    modname = os.path.splitext(os.path.basename(path))[0]
    module = imp.load_source(modname, path)
    sys.path.pop(0) # remove directory from global path
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(module)
    for test in suite:
        unittests.append(test)
    return unittests

def main(args=sys.argv[1:]):

    # parse command line options
    usage = '%prog [options] manifest.ini <manifest.ini> <...>'
    parser = optparse.OptionParser(usage=usage, description=__doc__)
    parser.add_option('-b', "--binary",
                  dest="binary", help="Binary path",
                  metavar=None, default=None)
    parser.add_option('--list', dest='list_tests',
                      action='store_true', default=False,
                      help="list paths of tests to be run")
    mozlog.commandline.add_logging_group(parser)
    options, args = parser.parse_args(args)
    logger = mozlog.commandline.setup_logging("mozbase", options,
                                              {"tbpl": sys.stdout})

    # read the manifest
    if args:
        manifests = args
    else:
        manifests = [os.path.join(here, 'test-manifest.ini')]
    missing = []
    for manifest in manifests:
        # ensure manifests exist
        if not os.path.exists(manifest):
            missing.append(manifest)
    assert not missing, 'manifest(s) not found: %s' % ', '.join(missing)
    manifest = manifestparser.TestManifest(manifests=manifests)

    if options.binary:
        # A specified binary should override the environment variable
        os.environ['BROWSER_PATH'] = options.binary

    # gather the tests
    tests = manifest.active_tests(disabled=False, **mozinfo.info)
    tests = [test['path'] for test in tests]
    logger.suite_start(tests)

    if options.list_tests:
        # print test paths
        print '\n'.join(tests)
        sys.exit(0)

    # create unittests
    unittestlist = []
    for test in tests:
        unittestlist.extend(unittests(test))

    # run the tests
    suite = unittest.TestSuite(unittestlist)
    runner = StructuredTestRunner(logger=logger)
    unittest_results = runner.run(suite)
    results = TestResultCollection.from_unittest_results(None, unittest_results)
    logger.suite_end()

    # exit according to results
    sys.exit(1 if results.num_failures else 0)

if __name__ == '__main__':
    main()
