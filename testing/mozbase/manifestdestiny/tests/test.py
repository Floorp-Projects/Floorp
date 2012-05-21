#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""tests for ManifestDestiny"""

import doctest
import os
import sys
from optparse import OptionParser

def run_tests(raise_on_error=False, report_first=False):

    # add results here
    results = {}

    # doctest arguments
    directory = os.path.dirname(os.path.abspath(__file__))
    extraglobs = {}
    doctest_args = dict(extraglobs=extraglobs,
                        module_relative=False,
                        raise_on_error=raise_on_error)
    if report_first:
        doctest_args['optionflags'] = doctest.REPORT_ONLY_FIRST_FAILURE
                                
    # gather tests
    directory = os.path.dirname(os.path.abspath(__file__))
    tests =  [ test for test in os.listdir(directory)
               if test.endswith('.txt') and test.startswith('test_')]
    os.chdir(directory)

    # run the tests
    for test in tests:
        try:
            results[test] = doctest.testfile(test, **doctest_args)
        except doctest.DocTestFailure, failure:
            raise
        except doctest.UnexpectedException, failure:
            raise failure.exc_info[0], failure.exc_info[1], failure.exc_info[2]
        
    return results
                                

def main(args=sys.argv[1:]):

    # parse command line options
    parser = OptionParser(description=__doc__)
    parser.add_option('--raise', dest='raise_on_error',
                      default=False, action='store_true',
                      help="raise on first error")
    parser.add_option('--report-first', dest='report_first',
                      default=False, action='store_true',
                      help="report the first error only (all tests will still run)")
    parser.add_option('-q', '--quiet', dest='quiet',
                      default=False, action='store_true',
                      help="minimize output")
    options, args = parser.parse_args(args)
    quiet = options.__dict__.pop('quiet')

    # run the tests
    results = run_tests(**options.__dict__)

    # check for failure
    failed = False
    for result in results.values():
        if result[0]: # failure count; http://docs.python.org/library/doctest.html#basic-api
            failed = True
            break
    if failed:
        sys.exit(1) # error
    if not quiet:
        # print results
        print "manifestparser.py: All tests pass!"
        for test in sorted(results.keys()):
            result = results[test]
            print "%s: failed=%s, attempted=%s" % (test, result[0], result[1])
               
if __name__ == '__main__':
    main()
