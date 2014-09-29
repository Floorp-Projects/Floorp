#! /usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""This script takes the file produced by DMD's test mode and checks its
correctness.

It produces the following output files: $TMP/full-{fixed,filtered,diff}.dmd.

It runs the appropriate fix* script to get nice stack traces.  It also
filters out platform-specific details from the test output file.

Note: you must run this from the same directory that you invoked DMD's test
mode, otherwise the fix* script will not work properly, because some of the
paths in the test output are relative.

"""

from __future__ import print_function

import os
import platform
import re
import subprocess
import sys
import tempfile

def test(src_dir, kind, options, i):
    # Filenames
    tmp_dir = tempfile.gettempdir()
    in_name        = os.path.join(src_dir, "full{:d}.json".format(i))
    fixed_name     = os.path.join(tmp_dir, "full-{:}-fixed{:d}.json".format(kind, i))
    converted_name = os.path.join(tmp_dir, "full-{:}-converted{:d}.txt".format(kind, i))
    filtered_name  = os.path.join(tmp_dir, "full-{:}-filtered{:d}.txt".format(kind, i))
    diff_name      = os.path.join(tmp_dir, "full-{:}-diff{:d}.txt".format(kind, i))
    expected_name  = os.path.join(src_dir, "memory", "replace", "dmd", "test", "full-{:}-expected{:d}.txt".format(kind, i))

    # Fix stack traces

    sys_name = platform.system()
    fix = os.path.join(src_dir, "tools", "rb")
    if sys_name == "Linux":
        fix = os.path.join(fix, "fix_linux_stack.py")
    elif sys_name == "Darwin":
        fix = os.path.join(fix, "fix_macosx_stack.py")
    else:
        print("unhandled platform: " + sys_name, file=sys.stderr)
        sys.exit(1)

    subprocess.call(fix, stdin=open(in_name, "r"),
                         stdout=open(fixed_name, "w"))

    # Convert from JSON

    convert = [os.path.join(src_dir, "memory", "replace", "dmd", "dmd.py")] + \
               options + ['--no-fix-stacks', fixed_name]
    subprocess.call(convert, stdout=open(converted_name, "w"))

    # Filter output

    # In heap block records we filter out most stack frames.  The only thing
    # we leave behind is a "DMD.cpp" entry if we see one or more frames that
    # have DMD.cpp in them.  There is simply too much variation to do anything
    # better than that.

    with open(converted_name, "r") as fin, \
         open(filtered_name, "w") as fout:

        test_frame_re = re.compile(r".*(DMD.cpp)")

        for line in fin:
            if re.match(r"  (Allocated at {|Reported( again)? at {)", line):
                # It's a heap block record.
                print(line, end='', file=fout)

                # Filter the stack trace -- print a single line if we see one
                # or more frames involving DMD.cpp.
                seen_DMD_frame = False
                for frame in fin:
                    if re.match(r"    ", frame):
                        m = test_frame_re.match(frame)
                        if m:
                            seen_DMD_frame = True
                    else:
                        # We're past the stack trace.
                        if seen_DMD_frame:
                            print("    ... DMD.cpp", file=fout)
                        print(frame, end='', file=fout)
                        break

            else:
                # A line that needs no special handling.  Copy it through.
                print(line, end='', file=fout)

    # Compare with expected output

    ret = subprocess.call(["diff", "-u", expected_name, filtered_name],
                          stdout=open(diff_name, "w"))

    if ret == 0:
        print("TEST-PASS | {:} {:d} | ok".format(kind, i))
    else:
        print("TEST-UNEXPECTED-FAIL | {:} {:d} | mismatch".format(kind, i))
        print("Output files:")
        print("- " + fixed_name);
        print("- " + converted_name);
        print("- " + filtered_name);
        print("- " + diff_name);


def main():
    if (len(sys.argv) != 2):
        print("usage:", sys.argv[0], "<topsrcdir>")
        sys.exit(1)

    src_dir = sys.argv[1]

    ntests = 4
    for i in range(1, ntests+1):
        test(src_dir, "reports", [], i)
        test(src_dir, "heap", ["--ignore-reports"], i)


if __name__ == "__main__":
    main()
