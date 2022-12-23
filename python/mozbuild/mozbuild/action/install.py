# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# A simple script to invoke mozinstall from the command line without depending
# on a build config.

import sys

import mozinstall


def main(args):
    if len(args) != 2:
        print("Usage: install.py [src] [dest]")
        return 1
    src, dest = args
    mozinstall.install(src, dest)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
