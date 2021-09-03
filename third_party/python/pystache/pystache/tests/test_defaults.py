# coding: utf-8

"""
Unit tests for defaults.py.

"""

import unittest

import pystache

from pystache.tests.common import AssertStringMixin


# TODO: make sure each default has at least one test.
class DefaultsConfigurableTestCase(unittest.TestCase, AssertStringMixin):

    """Tests that the user can change the defaults at runtime."""

    # TODO: switch to using a context manager after 2.4 is deprecated.
    def setUp(self):
        """Save the defaults."""
        defaults = [
            'DECODE_ERRORS', 'DELIMITERS',
            'FILE_ENCODING', 'MISSING_TAGS',
            'SEARCH_DIRS', 'STRING_ENCODING',
            'TAG_ESCAPE', 'TEMPLATE_EXTENSION'
        ]
        self.saved = {}
        for e in defaults:
            self.saved[e] = getattr(pystache.defaults, e)

    def tearDown(self):
        for key, value in self.saved.items():
            setattr(pystache.defaults, key, value)

    def test_tag_escape(self):
        """Test that changes to defaults.TAG_ESCAPE take effect."""
        template = u"{{foo}}"
        context = {'foo': '<'}
        actual = pystache.render(template, context)
        self.assertString(actual, u"&lt;")

        pystache.defaults.TAG_ESCAPE = lambda u: u
        actual = pystache.render(template, context)
        self.assertString(actual, u"<")

    def test_delimiters(self):
        """Test that changes to defaults.DELIMITERS take effect."""
        template = u"[[foo]]{{foo}}"
        context = {'foo': 'FOO'}
        actual = pystache.render(template, context)
        self.assertString(actual, u"[[foo]]FOO")

        pystache.defaults.DELIMITERS = ('[[', ']]')
        actual = pystache.render(template, context)
        self.assertString(actual, u"FOO{{foo}}")

    def test_missing_tags(self):
        """Test that changes to defaults.MISSING_TAGS take effect."""
        template = u"{{foo}}"
        context = {}
        actual = pystache.render(template, context)
        self.assertString(actual, u"")

        pystache.defaults.MISSING_TAGS = 'strict'
        self.assertRaises(pystache.context.KeyNotFoundError,
                          pystache.render, template, context)
