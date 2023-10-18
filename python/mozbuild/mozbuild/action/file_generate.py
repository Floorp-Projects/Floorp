# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Given a Python script and arguments describing the output file, and
# the arguments that can be used to generate the output file, call the
# script's |main| method with appropriate arguments.

import argparse
import importlib.util
import os
import sys
import traceback

import buildconfig
import six

from mozbuild.makeutil import Makefile
from mozbuild.pythonutil import iter_modules_in_path
from mozbuild.util import FileAvoidWrite


def main(argv):
    parser = argparse.ArgumentParser(
        "Generate a file from a Python script", add_help=False
    )
    parser.add_argument(
        "--locale", metavar="locale", type=six.text_type, help="The locale in use."
    )
    parser.add_argument(
        "python_script",
        metavar="python-script",
        type=six.text_type,
        help="The Python script to run",
    )
    parser.add_argument(
        "method_name",
        metavar="method-name",
        type=six.text_type,
        help="The method of the script to invoke",
    )
    parser.add_argument(
        "output_file",
        metavar="output-file",
        type=six.text_type,
        help="The file to generate",
    )
    parser.add_argument(
        "dep_file",
        metavar="dep-file",
        type=six.text_type,
        help="File to write any additional make dependencies to",
    )
    parser.add_argument(
        "dep_target",
        metavar="dep-target",
        type=six.text_type,
        help="Make target to use in the dependencies file",
    )
    parser.add_argument(
        "additional_arguments",
        metavar="arg",
        nargs=argparse.REMAINDER,
        help="Additional arguments to the script's main() method",
    )

    args = parser.parse_args(argv)

    kwargs = {}
    if args.locale:
        kwargs["locale"] = args.locale
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
    spec = importlib.util.spec_from_file_location("script", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    method = args.method_name
    if not hasattr(module, method):
        print(
            'Error: script "{0}" is missing a {1} method'.format(script, method),
            file=sys.stderr,
        )
        return 1

    ret = 1
    try:
        with FileAvoidWrite(args.output_file, readmode="rb") as output:
            try:
                ret = module.__dict__[method](
                    output, *args.additional_arguments, **kwargs
                )
            except Exception:
                # Ensure that we don't overwrite the file if the script failed.
                output.avoid_writing_to_file()
                raise

            # The following values indicate a statement of success:
            #  - a set() (see below)
            #  - 0
            #  - False
            #  - None
            #
            # Everything else is an error (so scripts can conveniently |return
            # 1| or similar). If a set is returned, the elements of the set
            # indicate additional dependencies that will be listed in the deps
            # file. Python module imports are automatically included as
            # dependencies.
            if isinstance(ret, set):
                deps = set(six.ensure_text(s) for s in ret)
                # The script succeeded, so reset |ret| to indicate that.
                ret = None
            else:
                deps = set()

            # Only write out the dependencies if the script was successful
            if not ret:
                # Add dependencies on any python modules that were imported by
                # the script.
                deps |= set(
                    six.ensure_text(s)
                    for s in iter_modules_in_path(
                        buildconfig.topsrcdir, buildconfig.topobjdir
                    )
                )
                # Add dependencies on any buildconfig items that were accessed
                # by the script.
                deps |= set(six.ensure_text(s) for s in buildconfig.get_dependencies())

                mk = Makefile()
                mk.create_rule([args.dep_target]).add_dependencies(deps)
                with FileAvoidWrite(args.dep_file) as dep_file:
                    mk.dump(dep_file)
            else:
                # Ensure that we don't overwrite the file if the script failed.
                output.avoid_writing_to_file()

    except IOError as e:
        print('Error opening file "{0}"'.format(e.filename), file=sys.stderr)
        traceback.print_exc()
        return 1
    return ret


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
