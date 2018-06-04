# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function

import argparse
import os

from mozlog.commandline import add_logging_group


def create_parser(mach_interface=False):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    add_arg('-t', '--test', required=True, dest='test',
            help="name of raptor test to run")
    add_arg('--app', default='firefox', dest='app',
            help="name of the application we are testing (default: firefox)",
            choices=['firefox', 'chrome'])
    add_arg('-b', '--binary', dest='binary',
            help="path to the browser executable that we are testing")
    if not mach_interface:
        add_arg('--branchName', dest="branch_name", default='',
                help="Name of the branch we are testing on")
        add_arg('--symbolsPath', dest='symbols_path',
                help="Path to the symbols for the build we are testing")
        add_arg('--run-local', dest="run_local", default=False, action="store_true",
                help="Flag that indicates if raptor is running locally or in production")
        add_arg('--obj-path', dest="obj_path", default=None,
                help="Browser build obj_path (received when running in production)")

    add_logging_group(parser)
    return parser


def verify_options(parser, args):
    ctx = vars(args)
    if args.binary is None:
        parser.error("--binary is required!")

    if not os.path.isfile(args.binary):
        parser.error("{binary} does not exist!".format(**ctx))


def parse_args(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)
    verify_options(parser, args)
    return args
