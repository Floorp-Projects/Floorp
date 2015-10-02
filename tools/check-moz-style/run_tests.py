#!/usr/bin/python
#
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/
#

from __future__ import print_function
from modules.scm import detect_scm_system
from contextlib import closing
import checkmozstyle
import os
import modules.cpplint as cpplint
import StringIO

TESTS = [
    # Empty patch
    {
        "patch": "tests/test1.patch",
        "cpp": "tests/test1.cpp",
        "out": "tests/test1.out"
    },
    # Bad header
    {
        "patch": "tests/test2.patch",
        "cpp": "tests/test2.cpp",
        "out": "tests/test2.out"
    },
    # Bad Description
    {
        "patch": "tests/test3.patch",
        "cpp": "tests/test3.cpp",
        "out": "tests/test3.out"
    },
    # readability tests
    {
        "patch": "tests/test4.patch",
        "cpp": "tests/test4.cpp",
        "out": "tests/test4.out"
    },
    # runtime tests
    {
        "patch": "tests/test5.patch",
        "cpp": "tests/test5.cpp",
        "out": "tests/test5.out"
    },
]


def main():
    cwd = os.path.abspath('.')
    scm = detect_scm_system(cwd)
    cpplint.use_mozilla_styles()
    (args, flags) = cpplint.parse_arguments([])

    for test in TESTS:
        with open(test["patch"]) as fh:
            patch = fh.read()

        with closing(StringIO.StringIO()) as output:
            cpplint.set_stream(output)
            checkmozstyle.process_patch(patch, cwd, cwd, scm)
            result = output.getvalue()

        with open(test["out"]) as fh:
            expected_output = fh.read()

        test_status = "PASSED"
        if result != expected_output:
            test_status = "FAILED"
            print("TEST " + test["patch"] + " " + test_status)
            print("Got result:\n" + result + "Expected:\n" + expected_output)
        else:
            print("TEST " + test["patch"] + " " + test_status)


if __name__ == "__main__":
        main()

