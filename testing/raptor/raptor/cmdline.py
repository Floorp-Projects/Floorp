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

    if not mach_interface:
        add_arg('--app', default='firefox', dest='app',
                help="name of the application we are testing (default: firefox)",
                choices=['firefox', 'chrome'])
        add_arg('-b', '--binary', required=True, dest='binary',
                help="path to the browser executable that we are testing")
        add_arg('--branchName', dest="branch_name", default='',
                help="Name of the branch we are testing on")
        add_arg('--symbolsPath', dest='symbols_path',
                help="Path to the symbols for the build we are testing")
    # remaining arg is test name
    add_arg("test",
            nargs="*",
            help="name of raptor test to run")

    add_logging_group(parser)
    return parser


def verify_options(parser, args):
    ctx = vars(args)

    if not os.path.isfile(args.binary):
        parser.error("{binary} does not exist!".format(**ctx))


def parse_args(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)
    verify_options(parser, args)
    return args
