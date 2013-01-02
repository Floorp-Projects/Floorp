#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import sys
import tempfile

from smoketest import *

def main():
    parser = argparse.ArgumentParser(prog="run-smoketests.py")
    parser.add_argument("--build-dir", required=True,
        help="directory that contains builds with build ID subdirectories")
    parser.add_argument("--run-dir", default=None,
        help="directory where partials and testvars are generated. default: "
             "create a temp directory")
    parser.add_argument("--tests", action='append',
        help="which smoketest(s) to run. by default all tests are run")
    parser.add_argument("build_ids", nargs="+", metavar="BUILD_ID",
        help="a list of build IDs to run smoketests against. the IDs will be "
             "sorted numerically, and partials will be generated from each to "
             "the last update. this list of partials will be tested along with "
             "a full update from each build to the last")
    args = parser.parse_args()

    try:
        b2g = find_b2g()
    except EnvironmentError, e:
        parser.exit(1, "This tool must be run while inside the B2G directory, "
                       "or B2G_HOME must be set in the environment.")

    if not os.path.exists(args.build_dir):
        parser.exit(1, "Build dir doesn't exist: %s" % args.build_dir)

    if len(args.build_ids) < 2:
        parser.exit(1, "This script requires at least two build IDs")

    for test in args.tests:
        if not os.path.exists(test):
            parser.exit(1, "Smoketest does not exist: %s" % test)

    try:
        config = SmokeTestConfig(args.build_dir)
        runner = SmokeTestRunner(config, b2g, run_dir=args.run_dir)
        runner.run_smoketests(args.build_ids, args.tests)
    except KeyError, e:
        parser.exit(1, "Error: %s" % e)
    except SmokeTestError, e:
        parser.exit(1, "Error: %s" % e)

if __name__ == "__main__":
    main()
