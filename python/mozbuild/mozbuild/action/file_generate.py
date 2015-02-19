# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Given a Python script and arguments describing the output file, and
# the arguments that can be used to generate the output file, call the
# script's |main| method with appropriate arguments.

from __future__ import print_function
import argparse
import imp
import os
import sys
import traceback

from mozbuild.util import FileAvoidWrite

def main(argv):
    parser = argparse.ArgumentParser('Generate a file from a Python script',
                                     add_help=False)
    parser.add_argument('python_script', metavar='python-script', type=str,
                        help='The Python script to run')
    parser.add_argument('output_file', metavar='output-file', type=str,
                        help='The file to generate')
    parser.add_argument('additional_arguments', metavar='arg', nargs='*',
                        help="Additional arguments to the script's main() method")

    args = parser.parse_args(argv)

    script = args.python_script
    with open(script, 'r') as fh:
        module = imp.load_module('script', fh, script,
                                 ('.py', 'r', imp.PY_SOURCE))
    if not hasattr(module, 'main'):
        print('Error: script "{0}" is missing a main method'.format(script),
              file=sys.stderr)
        return 1

    ret = 1
    try:
        with FileAvoidWrite(args.output_file) as output:
            ret = module.main(output, *args.additional_arguments)
    except IOError as e:
        print('Error opening file "{0}"'.format(e.filename), file=sys.stderr)
        traceback.print_exc()
        return 1
    return ret

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
