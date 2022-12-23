#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Hello I am a fake jsshell for testing purpose.
Add more features!
"""
import argparse
import sys


def run():
    parser = argparse.ArgumentParser(description="Process some integers.")
    parser.add_argument("-e", type=str, default=None)

    parser.add_argument("--fuzzing-safe", action="store_true", default=False)

    args = parser.parse_args()

    if args.e is not None:
        if "crash()" in args.e:
            sys.exit(1)


if __name__ == "__main__":
    run()
