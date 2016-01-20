# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Given a Python script and arguments describing the output file, and
# the arguments that can be used to generate the output file, call the
# script's |main| method with appropriate arguments.

from __future__ import absolute_import, print_function

import argparse
import imp
import os
import sys
import traceback

from mozbuild.pythonutil import iter_modules_in_path
from mozbuild.makeutil import Makefile
from mozbuild.util import FileAvoidWrite
import buildconfig


def main(argv):
    parser = argparse.ArgumentParser('Generate a file from a Python script',
                                     add_help=False)
    parser.add_argument('python_script', metavar='python-script', type=str,
                        help='The Python script to run')
    parser.add_argument('method_name', metavar='method-name', type=str,
                        help='The method of the script to invoke')
    parser.add_argument('output_file', metavar='output-file', type=str,
                        help='The file to generate')
    parser.add_argument('dep_file', metavar='dep-file', type=str,
                        help='File to write any additional make dependencies to')
    parser.add_argument('additional_arguments', metavar='arg',
                        nargs=argparse.REMAINDER,
                        help="Additional arguments to the script's main() method")

    args = parser.parse_args(argv)

    script = args.python_script
    # Permit the script to import modules from the same directory in which it
    # resides.  The justification for doing this is that if we were invoking
    # the script as:
    #
    #    python script arg1...
    #
    # then importing modules from the script's directory would come for free.
    # Since we're invoking the script in a roundabout way, we provide this
    # bit of convenience.
    sys.path.append(os.path.dirname(script))
    with open(script, 'r') as fh:
        module = imp.load_module('script', fh, script,
                                 ('.py', 'r', imp.PY_SOURCE))
    method = args.method_name
    if not hasattr(module, method):
        print('Error: script "{0}" is missing a {1} method'.format(script, method),
              file=sys.stderr)
        return 1

    ret = 1
    try:
        with FileAvoidWrite(args.output_file) as output:
            ret = module.__dict__[method](output, *args.additional_arguments)
            # We treat sets as a statement of success.  Everything else
            # is an error (so scripts can conveniently |return 1| or
            # similar).
            if isinstance(ret, set) and ret:
                ret |= set(iter_modules_in_path(buildconfig.topsrcdir,
                                                buildconfig.topobjdir))
                mk = Makefile()
                mk.create_rule([args.output_file]).add_dependencies(ret)
                with FileAvoidWrite(args.dep_file) as dep_file:
                    mk.dump(dep_file)
                # The script succeeded, so reset |ret| to indicate that.
                ret = None
        # Even when our file's contents haven't changed, we want to update
        # the file's mtime so make knows this target isn't still older than
        # whatever prerequisite caused it to be built this time around.
        try:
            os.utime(args.output_file, None)
        except:
            print('Error processing file "{0}"'.format(args.output_file),
                  file=sys.stderr)
            traceback.print_exc()
    except IOError as e:
        print('Error opening file "{0}"'.format(e.filename), file=sys.stderr)
        traceback.print_exc()
        return 1
    return ret

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
