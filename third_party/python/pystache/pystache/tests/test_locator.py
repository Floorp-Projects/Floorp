# encoding: utf-8

"""
Unit tests for locator.py.

"""

from datetime import datetime
import os
import sys
import unittest

# TODO: remove this alias.
from pystache.common import TemplateNotFoundError
from pystache.loader import Loader as Reader
from pystache.locator import Locator

from pystache.tests.common import DATA_DIR, EXAMPLES_DIR, AssertExceptionMixin
from pystache.tests.data.views import SayHello


LOCATOR_DATA_DIR = os.path.join(DATA_DIR, 'locator')


class LocatorTests(unittest.TestCase, AssertExceptionMixin):

    def _locator(self):
        return Locator(search_dirs=DATA_DIR)

    def test_init__extension(self):
        # Test the default value.
        locator = Locator()
        self.assertEqual(locator.template_extension, 'mustache')

        locator = Locator(extension='txt')
        self.assertEqual(locator.template_extension, 'txt')

        locator = Locator(extension=False)
        self.assertTrue(locator.template_extension is False)

    def _assert_paths(self, actual, expected):
        """
        Assert that two paths are the same.

        """
        self.assertEqual(actual, expected)

    def test_get_object_directory(self):
        locator = Locator()

        obj = SayHello()
        actual = locator.get_object_directory(obj)

        self._assert_paths(actual, DATA_DIR)

    def test_get_object_directory__not_hasattr_module(self):
        locator = Locator()

        # Previously, we used a genuine object -- a datetime instance --
        # because datetime instances did not have the __module__ attribute
        # in CPython.  See, for example--
        #
        #   http://bugs.python.org/issue15223
        #
        # However, since datetime instances do have the __module__ attribute
        # in PyPy, we needed to switch to something else once we added
        # support for PyPi.  This was so that our test runs would pass
        # in all systems.
        obj = "abc"
        self.assertFalse(hasattr(obj, '__module__'))
        self.assertEqual(locator.get_object_directory(obj), None)

        self.assertFalse(hasattr(None, '__module__'))
        self.assertEqual(locator.get_object_directory(None), None)

    def test_make_file_name(self):
        locator = Locator()

        locator.template_extension = 'bar'
        self.assertEqual(locator.make_file_name('foo'), 'foo.bar')

        locator.template_extension = False
        self.assertEqual(locator.make_file_name('foo'), 'foo')

        locator.template_extension = ''
        self.assertEqual(locator.make_file_name('foo'), 'foo.')

    def test_make_file_name__template_extension_argument(self):
        locator = Locator()

        self.assertEqual(locator.make_file_name('foo', template_extension='bar'), 'foo.bar')

    def test_find_file(self):
        locator = Locator()
        path = locator.find_file('template.txt', [LOCATOR_DATA_DIR])

        expected_path = os.path.join(LOCATOR_DATA_DIR, 'template.txt')
        self.assertEqual(path, expected_path)

    def test_find_name(self):
        locator = Locator()
        path = locator.find_name(search_dirs=[EXAMPLES_DIR], template_name='simple')

        self.assertEqual(os.path.basename(path), 'simple.mustache')

    def test_find_name__using_list_of_paths(self):
        locator = Locator()
        path = locator.find_name(search_dirs=[EXAMPLES_DIR, 'doesnt_exist'], template_name='simple')

        self.assertTrue(path)

    def test_find_name__precedence(self):
        """
        Test the order in which find_name() searches directories.

        """
        locator = Locator()

        dir1 = DATA_DIR
        dir2 = LOCATOR_DATA_DIR

        self.assertTrue(locator.find_name(search_dirs=[dir1], template_name='duplicate'))
        self.assertTrue(locator.find_name(search_dirs=[dir2], template_name='duplicate'))

        path = locator.find_name(search_dirs=[dir2, dir1], template_name='duplicate')
        dirpath = os.path.dirname(path)
        dirname = os.path.split(dirpath)[-1]

        self.assertEqual(dirname, 'locator')

    def test_find_name__non_existent_template_fails(self):
        locator = Locator()

        self.assertException(TemplateNotFoundError, "File 'doesnt_exist.mustache' not found in dirs: []",
                             locator.find_name, search_dirs=[], template_name='doesnt_exist')

    def test_find_object(self):
        locator = Locator()

        obj = SayHello()

        actual = locator.find_object(search_dirs=[], obj=obj, file_name='sample_view.mustache')
        expected = os.path.join(DATA_DIR, 'sample_view.mustache')

        self._assert_paths(actual, expected)

    def test_find_object__none_file_name(self):
        locator = Locator()

        obj = SayHello()

        actual = locator.find_object(search_dirs=[], obj=obj)
        expected = os.path.join(DATA_DIR, 'say_hello.mustache')

        self.assertEqual(actual, expected)

    def test_find_object__none_object_directory(self):
        locator = Locator()

        obj = None
        self.assertEqual(None, locator.get_object_directory(obj))

        actual = locator.find_object(search_dirs=[DATA_DIR], obj=obj, file_name='say_hello.mustache')
        expected = os.path.join(DATA_DIR, 'say_hello.mustache')

        self.assertEqual(actual, expected)

    def test_make_template_name(self):
        """
        Test make_template_name().

        """
        locator = Locator()

        class FooBar(object):
            pass
        foo = FooBar()

        self.assertEqual(locator.make_template_name(foo), 'foo_bar')
