# coding: utf-8

"""
Exposes a get_doctests() function for the project's test harness.

"""

import doctest
import os
import pkgutil
import sys
import traceback

if sys.version_info >= (3,):
    # Then pull in modules needed for 2to3 conversion.  The modules
    # below are not necessarily available in older versions of Python.
    from lib2to3.main import main as lib2to3main  # new in Python 2.6?
    from shutil import copyfile

from pystache.tests.common import TEXT_DOCTEST_PATHS
from pystache.tests.common import get_module_names


# This module follows the guidance documented here:
#
#   http://docs.python.org/library/doctest.html#unittest-api
#

def get_doctests(text_file_dir):
    """
    Return a list of TestSuite instances for all doctests in the project.

    Arguments:

      text_file_dir: the directory in which to search for all text files
        (i.e. non-module files) containing doctests.

    """
    # Since module_relative is False in our calls to DocFileSuite below,
    # paths should be OS-specific.  See the following for more info--
    #
    #   http://docs.python.org/library/doctest.html#doctest.DocFileSuite
    #
    paths = [os.path.normpath(os.path.join(text_file_dir, path)) for path in TEXT_DOCTEST_PATHS]

    if sys.version_info >= (3,):
        # Skip the README doctests in Python 3 for now because examples
        # rendering to unicode do not give consistent results
        # (e.g. 'foo' vs u'foo').
        # paths = _convert_paths(paths)
        paths = []

    suites = []

    for path in paths:
        suite = doctest.DocFileSuite(path, module_relative=False)
        suites.append(suite)

    modules = get_module_names()
    for module in modules:
        suite = doctest.DocTestSuite(module)
        suites.append(suite)

    return suites


def _convert_2to3(path):
    """
    Convert the given file, and return the path to the converted files.

    """
    base, ext = os.path.splitext(path)
    # For example, "README.temp2to3.rst".
    new_path = "%s.temp2to3%s" % (base, ext)

    copyfile(path, new_path)

    args = ['--doctests_only', '--no-diffs', '--write', '--nobackups', new_path]
    lib2to3main("lib2to3.fixes", args=args)

    return new_path


def _convert_paths(paths):
    """
    Convert the given files, and return the paths to the converted files.

    """
    new_paths = []
    for path in paths:
        new_path = _convert_2to3(path)
        new_paths.append(new_path)

    return new_paths
