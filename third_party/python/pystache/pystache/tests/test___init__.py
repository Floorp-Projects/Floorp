# coding: utf-8

"""
Tests of __init__.py.

"""

# Calling "import *" is allowed only at the module level.
GLOBALS_INITIAL = globals().keys()
from pystache import *
GLOBALS_PYSTACHE_IMPORTED = globals().keys()

import unittest

import pystache


class InitTests(unittest.TestCase):

    def test___all__(self):
        """
        Test that "from pystache import *" works as expected.

        """
        actual = set(GLOBALS_PYSTACHE_IMPORTED) - set(GLOBALS_INITIAL)
        expected = set(['parse', 'render', 'Renderer', 'TemplateSpec', 'GLOBALS_INITIAL'])

        self.assertEqual(actual, expected)

    def test_version_defined(self):
        """
        Test that pystache.__version__ is set.

        """
        actual_version = pystache.__version__
        self.assertTrue(actual_version)
