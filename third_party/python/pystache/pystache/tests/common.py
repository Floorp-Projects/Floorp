# coding: utf-8

"""
Provides test-related code that can be used by all tests.

"""

import os

import pystache
from pystache import defaults
from pystache.tests import examples

# Save a reference to the original function to avoid recursion.
_DEFAULT_TAG_ESCAPE = defaults.TAG_ESCAPE
_TESTS_DIR = os.path.dirname(pystache.tests.__file__)

DATA_DIR = os.path.join(_TESTS_DIR, 'data')  # i.e. 'pystache/tests/data'.
EXAMPLES_DIR = os.path.dirname(examples.__file__)
PACKAGE_DIR = os.path.dirname(pystache.__file__)
PROJECT_DIR = os.path.join(PACKAGE_DIR, '..')
# TEXT_DOCTEST_PATHS: the paths to text files (i.e. non-module files)
# containing doctests.  The paths should be relative to the project directory.
TEXT_DOCTEST_PATHS = ['README.md']

UNITTEST_FILE_PREFIX = "test_"


def get_spec_test_dir(project_dir):
    return os.path.join(project_dir, 'ext', 'spec', 'specs')


def html_escape(u):
    """
    An html escape function that behaves the same in both Python 2 and 3.

    This function is needed because single quotes are escaped in Python 3
    (to '&#x27;'), but not in Python 2.

    The global defaults.TAG_ESCAPE can be set to this function in the
    setUp() and tearDown() of unittest test cases, for example, for
    consistent test results.

    """
    u = _DEFAULT_TAG_ESCAPE(u)
    return u.replace("'", '&#x27;')


def get_data_path(file_name=None):
    """Return the path to a file in the test data directory."""
    if file_name is None:
        file_name = ""
    return os.path.join(DATA_DIR, file_name)


# Functions related to get_module_names().

def _find_files(root_dir, should_include):
    """
    Return a list of paths to all modules below the given directory.

    Arguments:

      should_include: a function that accepts a file path and returns True or False.

    """
    paths = []  # Return value.

    is_module = lambda path: path.endswith(".py")

    # os.walk() is new in Python 2.3
    #   http://docs.python.org/library/os.html#os.walk
    for dir_path, dir_names, file_names in os.walk(root_dir):
        new_paths = [os.path.join(dir_path, file_name) for file_name in file_names]
        new_paths = filter(is_module, new_paths)
        new_paths = filter(should_include, new_paths)
        paths.extend(new_paths)

    return paths


def _make_module_names(package_dir, paths):
    """
    Return a list of fully-qualified module names given a list of module paths.

    """
    package_dir = os.path.abspath(package_dir)
    package_name = os.path.split(package_dir)[1]

    prefix_length = len(package_dir)

    module_names = []
    for path in paths:
        path = os.path.abspath(path)  # for example <path_to_package>/subpackage/module.py
        rel_path = path[prefix_length:]  # for example /subpackage/module.py
        rel_path = os.path.splitext(rel_path)[0]  # for example /subpackage/module

        parts = []
        while True:
            (rel_path, tail) = os.path.split(rel_path)
            if not tail:
                break
            parts.insert(0, tail)
        # We now have, for example, ['subpackage', 'module'].
        parts.insert(0, package_name)
        module = ".".join(parts)
        module_names.append(module)

    return module_names


def get_module_names(package_dir=None, should_include=None):
    """
    Return a list of fully-qualified module names in the given package.

    """
    if package_dir is None:
        package_dir = PACKAGE_DIR

    if should_include is None:
        should_include = lambda path: True

    paths = _find_files(package_dir, should_include)
    names = _make_module_names(package_dir, paths)
    names.sort()

    return names


class AssertStringMixin:

    """A unittest.TestCase mixin to check string equality."""

    def assertString(self, actual, expected, format=None):
        """
        Assert that the given strings are equal and have the same type.

        Arguments:

          format: a format string containing a single conversion specifier %s.
            Defaults to "%s".

        """
        if format is None:
            format = "%s"

        # Show both friendly and literal versions.
        details = """String mismatch: %%s

        Expected: \"""%s\"""
        Actual:   \"""%s\"""

        Expected: %s
        Actual:   %s""" % (expected, actual, repr(expected), repr(actual))

        def make_message(reason):
            description = details % reason
            return format % description

        self.assertEqual(actual, expected, make_message("different characters"))

        reason = "types different: %s != %s (actual)" % (repr(type(expected)), repr(type(actual)))
        self.assertEqual(type(expected), type(actual), make_message(reason))


class AssertIsMixin:

    """A unittest.TestCase mixin adding assertIs()."""

    # unittest.assertIs() is not available until Python 2.7:
    #   http://docs.python.org/library/unittest.html#unittest.TestCase.assertIsNone
    def assertIs(self, first, second):
        self.assertTrue(first is second, msg="%s is not %s" % (repr(first), repr(second)))


class AssertExceptionMixin:

    """A unittest.TestCase mixin adding assertException()."""

    # unittest.assertRaisesRegexp() is not available until Python 2.7:
    #   http://docs.python.org/library/unittest.html#unittest.TestCase.assertRaisesRegexp
    def assertException(self, exception_type, msg, callable, *args, **kwds):
        try:
            callable(*args, **kwds)
            raise Exception("Expected exception: %s: %s" % (exception_type, repr(msg)))
        except exception_type, err:
            self.assertEqual(str(err), msg)


class SetupDefaults(object):

    """
    Mix this class in to a unittest.TestCase for standard defaults.

    This class allows for consistent test results across Python 2/3.

    """

    def setup_defaults(self):
        self.original_decode_errors = defaults.DECODE_ERRORS
        self.original_file_encoding = defaults.FILE_ENCODING
        self.original_string_encoding = defaults.STRING_ENCODING

        defaults.DECODE_ERRORS = 'strict'
        defaults.FILE_ENCODING = 'ascii'
        defaults.STRING_ENCODING = 'ascii'

    def teardown_defaults(self):
        defaults.DECODE_ERRORS = self.original_decode_errors
        defaults.FILE_ENCODING = self.original_file_encoding
        defaults.STRING_ENCODING = self.original_string_encoding


class Attachable(object):
    """
    A class that attaches all constructor named parameters as attributes.

    For example--

    >>> obj = Attachable(foo=42, size="of the universe")
    >>> repr(obj)
    "Attachable(foo=42, size='of the universe')"
    >>> obj.foo
    42
    >>> obj.size
    'of the universe'

    """
    def __init__(self, **kwargs):
        self.__args__ = kwargs
        for arg, value in kwargs.iteritems():
            setattr(self, arg, value)

    def __repr__(self):
        return "%s(%s)" % (self.__class__.__name__,
                           ", ".join("%s=%s" % (k, repr(v))
                                     for k, v in self.__args__.iteritems()))
