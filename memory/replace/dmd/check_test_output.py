#! /usr/bin/python

"""This script takes the file produced by DMD's test mode and checks its
correctness.

It produces the following output files: $TMP/test-{fixed,filtered,diff}.dmd.

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


def main():

    # Arguments

    if (len(sys.argv) != 3):
        print("usage:", sys.argv[0], "<topsrcdir> <test.dmd>")
        sys.exit(1)

    srcdir = sys.argv[1]

    # Filenames

    tempdir = tempfile.gettempdir()
    in_name       = sys.argv[2]
    fixed_name    = tempdir + os.sep + "test-fixed.dmd"
    filtered_name = tempdir + os.sep + "test-filtered.dmd"
    diff_name     = tempdir + os.sep + "test-diff.dmd"
    expected_name = srcdir + os.sep + \
                    "memory/replace/dmd/test-expected.dmd"

    # Fix stack traces

    print("fixing output to", fixed_name)

    sysname = platform.system()
    if sysname == "Linux":
        fix = srcdir + os.sep + "tools/rb/fix_linux_stack.py"
    elif sysname == "Darwin":
        fix = srcdir + os.sep + "tools/rb/fix_macosx_stack.py"
    else:
        print("unhandled platform: " + sysname, file=sys.stderr)
        sys.exit(1)

    subprocess.call(fix, stdin=open(in_name, "r"),
                         stdout=open(fixed_name, "w"))

    # Filter output

    # In heap block records we filter out most stack frames.  The only thing
    # we leave behind is a "DMD.cpp" entry if we see one or more frames that
    # have DMD.cpp in them.  There is simply too much variation to do anything
    # better than that.

    print("filtering output to", filtered_name)

    with open(fixed_name, "r") as fin, \
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

    print("diffing output to", diff_name)

    ret = subprocess.call(["diff", "-u", expected_name, filtered_name],
                          stdout=open(diff_name, "w"))

    if ret == 0:
        print("test PASSED")
    else:
        print("test FAILED (did you remember to run this script and Firefox "
              "in the same directory?)")


if __name__ == "__main__":
    main()
