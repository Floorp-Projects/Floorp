# coding: utf-8

"""
Exposes a main() function that runs all tests in the project.

This module is for our test console script.

"""

import os
import sys
import unittest
from unittest import TestCase, TestProgram

import pystache
from pystache.tests.common import PACKAGE_DIR, PROJECT_DIR, UNITTEST_FILE_PREFIX
from pystache.tests.common import get_module_names, get_spec_test_dir
from pystache.tests.doctesting import get_doctests
from pystache.tests.spectesting import get_spec_tests


# If this command option is present, then the spec test and doctest directories
# will be inserted if not provided.
FROM_SOURCE_OPTION = "--from-source"


def make_extra_tests(text_doctest_dir, spec_test_dir):
    tests = []

    if text_doctest_dir is not None:
        doctest_suites = get_doctests(text_doctest_dir)
        tests.extend(doctest_suites)

    if spec_test_dir is not None:
        spec_testcases = get_spec_tests(spec_test_dir)
        tests.extend(spec_testcases)

    return unittest.TestSuite(tests)


def make_test_program_class(extra_tests):
    """
    Return a subclass of unittest.TestProgram.

    """
    # The function unittest.main() is an alias for unittest.TestProgram's
    # constructor.  TestProgram's constructor does the following:
    #
    # 1. calls self.parseArgs(argv),
    # 2. which in turn calls self.createTests().
    # 3. then the constructor calls self.runTests().
    #
    # The createTests() method sets the self.test attribute by calling one
    # of self.testLoader's "loadTests" methods.  Each loadTest method returns
    # a unittest.TestSuite instance.  Thus, self.test is set to a TestSuite
    # instance prior to calling runTests().
    class PystacheTestProgram(TestProgram):

        """
        Instantiating an instance of this class runs all tests.

        """

        def createTests(self):
            """
            Load tests and set self.test to a unittest.TestSuite instance

            Compare--

              http://docs.python.org/library/unittest.html#unittest.TestSuite

            """
            super(PystacheTestProgram, self).createTests()
            self.test.addTests(extra_tests)

    return PystacheTestProgram


# Do not include "test" in this function's name to avoid it getting
# picked up by nosetests.
def main(sys_argv):
    """
    Run all tests in the project.

    Arguments:

      sys_argv: a reference to sys.argv.

    """
    # TODO: use logging module
    print "pystache: running tests: argv: %s" % repr(sys_argv)

    should_source_exist = False
    spec_test_dir = None
    project_dir = None

    if len(sys_argv) > 1 and sys_argv[1] == FROM_SOURCE_OPTION:
        # This usually means the test_pystache.py convenience script
        # in the source directory was run.
        should_source_exist = True
        sys_argv.pop(1)

    try:
        # TODO: use optparse command options instead.
        project_dir = sys_argv[1]
        sys_argv.pop(1)
    except IndexError:
        if should_source_exist:
            project_dir = PROJECT_DIR

    try:
        # TODO: use optparse command options instead.
        spec_test_dir = sys_argv[1]
        sys_argv.pop(1)
    except IndexError:
        if project_dir is not None:
            # Then auto-detect the spec test directory.
            _spec_test_dir = get_spec_test_dir(project_dir)
            if not os.path.exists(_spec_test_dir):
                # Then the user is probably using a downloaded sdist rather
                # than a repository clone (since the sdist does not include
                # the spec test directory).
                print("pystache: skipping spec tests: spec test directory "
                      "not found")
            else:
                spec_test_dir = _spec_test_dir

    if len(sys_argv) <= 1 or sys_argv[-1].startswith("-"):
        # Then no explicit module or test names were provided, so
        # auto-detect all unit tests.
        module_names = _discover_test_modules(PACKAGE_DIR)
        sys_argv.extend(module_names)
        if project_dir is not None:
            # Add the current module for unit tests contained here (e.g.
            # to include SetupTests).
            sys_argv.append(__name__)

    SetupTests.project_dir = project_dir

    extra_tests = make_extra_tests(project_dir, spec_test_dir)
    test_program_class = make_test_program_class(extra_tests)

    # We pass None for the module because we do not want the unittest
    # module to resolve module names relative to a given module.
    # (This would require importing all of the unittest modules from
    # this module.)  See the loadTestsFromName() method of the
    # unittest.TestLoader class for more details on this parameter.
    test_program_class(argv=sys_argv, module=None)
    # No need to return since unitttest.main() exits.


def _discover_test_modules(package_dir):
    """
    Discover and return a sorted list of the names of unit-test modules.

    """
    def is_unittest_module(path):
        file_name = os.path.basename(path)
        return file_name.startswith(UNITTEST_FILE_PREFIX)

    names = get_module_names(package_dir=package_dir, should_include=is_unittest_module)

    # This is a sanity check to ensure that the unit-test discovery
    # methods are working.
    if len(names) < 1:
        raise Exception("No unit-test modules found--\n  in %s" % package_dir)

    return names


class SetupTests(TestCase):

    """Tests about setup.py."""

    project_dir = None

    def test_version(self):
        """
        Test that setup.py's version matches the package's version.

        """
        original_path = list(sys.path)

        sys.path.insert(0, self.project_dir)

        try:
            from setup import VERSION
            self.assertEqual(VERSION, pystache.__version__)
        finally:
            sys.path = original_path
