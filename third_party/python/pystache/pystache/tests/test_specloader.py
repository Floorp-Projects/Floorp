# coding: utf-8

"""
Unit tests for template_spec.py.

"""

import os.path
import sys
import unittest

import examples
from examples.simple import Simple
from examples.complex import Complex
from examples.lambdas import Lambdas
from examples.inverted import Inverted, InvertedLists
from pystache import Renderer
from pystache import TemplateSpec
from pystache.common import TemplateNotFoundError
from pystache.locator import Locator
from pystache.loader import Loader
from pystache.specloader import SpecLoader
from pystache.tests.common import DATA_DIR, EXAMPLES_DIR
from pystache.tests.common import AssertIsMixin, AssertStringMixin
from pystache.tests.data.views import SampleView
from pystache.tests.data.views import NonAscii


class Thing(object):
    pass


class AssertPathsMixin:

    """A unittest.TestCase mixin to check path equality."""

    def assertPaths(self, actual, expected):
        self.assertEqual(actual, expected)


class ViewTestCase(unittest.TestCase, AssertStringMixin):

    def test_template_rel_directory(self):
        """
        Test that View.template_rel_directory is respected.

        """
        class Tagless(TemplateSpec):
            pass

        view = Tagless()
        renderer = Renderer()

        self.assertRaises(TemplateNotFoundError, renderer.render, view)

        # TODO: change this test to remove the following brittle line.
        view.template_rel_directory = "examples"
        actual = renderer.render(view)
        self.assertEqual(actual, "No tags...")

    def test_template_path_for_partials(self):
        """
        Test that View.template_rel_path is respected for partials.

        """
        spec = TemplateSpec()
        spec.template = "Partial: {{>tagless}}"

        renderer1 = Renderer()
        renderer2 = Renderer(search_dirs=EXAMPLES_DIR)

        actual = renderer1.render(spec)
        self.assertString(actual, u"Partial: ")

        actual = renderer2.render(spec)
        self.assertEqual(actual, "Partial: No tags...")

    def test_basic_method_calls(self):
        renderer = Renderer()
        actual = renderer.render(Simple())

        self.assertString(actual, u"Hi pizza!")

    def test_non_callable_attributes(self):
        view = Simple()
        view.thing = 'Chris'

        renderer = Renderer()
        actual = renderer.render(view)
        self.assertEqual(actual, "Hi Chris!")

    def test_complex(self):
        renderer = Renderer()
        actual = renderer.render(Complex())
        self.assertString(actual, u"""\
<h1>Colors</h1>
<ul>
<li><strong>red</strong></li>
<li><a href="#Green">green</a></li>
<li><a href="#Blue">blue</a></li>
</ul>""")

    def test_higher_order_replace(self):
        renderer = Renderer()
        actual = renderer.render(Lambdas())
        self.assertEqual(actual, 'bar != bar. oh, it does!')

    def test_higher_order_rot13(self):
        view = Lambdas()
        view.template = '{{#rot13}}abcdefghijklm{{/rot13}}'

        renderer = Renderer()
        actual = renderer.render(view)
        self.assertString(actual, u'nopqrstuvwxyz')

    def test_higher_order_lambda(self):
        view = Lambdas()
        view.template = '{{#sort}}zyxwvutsrqponmlkjihgfedcba{{/sort}}'

        renderer = Renderer()
        actual = renderer.render(view)
        self.assertString(actual, u'abcdefghijklmnopqrstuvwxyz')

    def test_partials_with_lambda(self):
        view = Lambdas()
        view.template = '{{>partial_with_lambda}}'

        renderer = Renderer(search_dirs=EXAMPLES_DIR)
        actual = renderer.render(view)
        self.assertEqual(actual, u'nopqrstuvwxyz')

    def test_hierarchical_partials_with_lambdas(self):
        view = Lambdas()
        view.template = '{{>partial_with_partial_and_lambda}}'

        renderer = Renderer(search_dirs=EXAMPLES_DIR)
        actual = renderer.render(view)
        self.assertString(actual, u'nopqrstuvwxyznopqrstuvwxyz')

    def test_inverted(self):
        renderer = Renderer()
        actual = renderer.render(Inverted())
        self.assertString(actual, u"""one, two, three, empty list""")

    def test_accessing_properties_on_parent_object_from_child_objects(self):
        parent = Thing()
        parent.this = 'derp'
        parent.children = [Thing()]
        view = Simple()
        view.template = "{{#parent}}{{#children}}{{this}}{{/children}}{{/parent}}"

        renderer = Renderer()
        actual = renderer.render(view, {'parent': parent})

        self.assertString(actual, u'derp')

    def test_inverted_lists(self):
        renderer = Renderer()
        actual = renderer.render(InvertedLists())
        self.assertString(actual, u"""one, two, three, empty list""")


def _make_specloader():
    """
    Return a default SpecLoader instance for testing purposes.

    """
    # Python 2 and 3 have different default encodings.  Thus, to have
    # consistent test results across both versions, we need to specify
    # the string and file encodings explicitly rather than relying on
    # the defaults.
    def to_unicode(s, encoding=None):
        """
        Raises a TypeError exception if the given string is already unicode.

        """
        if encoding is None:
            encoding = 'ascii'
        return unicode(s, encoding, 'strict')

    loader = Loader(file_encoding='ascii', to_unicode=to_unicode)
    return SpecLoader(loader=loader)


class SpecLoaderTests(unittest.TestCase, AssertIsMixin, AssertStringMixin,
                      AssertPathsMixin):

    """
    Tests template_spec.SpecLoader.

    """

    def _make_specloader(self):
        return _make_specloader()

    def test_init__defaults(self):
        spec_loader = SpecLoader()

        # Check the loader attribute.
        loader = spec_loader.loader
        self.assertEqual(loader.extension, 'mustache')
        self.assertEqual(loader.file_encoding, sys.getdefaultencoding())
        # TODO: finish testing the other Loader attributes.
        to_unicode = loader.to_unicode

    def test_init__loader(self):
        loader = Loader()
        custom = SpecLoader(loader=loader)

        self.assertIs(custom.loader, loader)

    # TODO: rename to something like _assert_load().
    def _assert_template(self, loader, custom, expected):
        self.assertString(loader.load(custom), expected)

    def test_load__template__type_str(self):
        """
        Test the template attribute: str string.

        """
        custom = TemplateSpec()
        custom.template = "abc"

        spec_loader = self._make_specloader()
        self._assert_template(spec_loader, custom, u"abc")

    def test_load__template__type_unicode(self):
        """
        Test the template attribute: unicode string.

        """
        custom = TemplateSpec()
        custom.template = u"abc"

        spec_loader = self._make_specloader()
        self._assert_template(spec_loader, custom, u"abc")

    def test_load__template__unicode_non_ascii(self):
        """
        Test the template attribute: non-ascii unicode string.

        """
        custom = TemplateSpec()
        custom.template = u"é"

        spec_loader = self._make_specloader()
        self._assert_template(spec_loader, custom, u"é")

    def test_load__template__with_template_encoding(self):
        """
        Test the template attribute: with template encoding attribute.

        """
        custom = TemplateSpec()
        custom.template = u'é'.encode('utf-8')

        spec_loader = self._make_specloader()

        self.assertRaises(UnicodeDecodeError, self._assert_template, spec_loader, custom, u'é')

        custom.template_encoding = 'utf-8'
        self._assert_template(spec_loader, custom, u'é')

    # TODO: make this test complete.
    def test_load__template__correct_loader(self):
        """
        Test that reader.unicode() is called correctly.

        This test tests that the correct reader is called with the correct
        arguments.  This is a catch-all test to supplement the other
        test cases.  It tests SpecLoader.load() independent of reader.unicode()
        being implemented correctly (and tested).

        """
        class MockLoader(Loader):

            def __init__(self):
                self.s = None
                self.encoding = None

            # Overrides the existing method.
            def unicode(self, s, encoding=None):
                self.s = s
                self.encoding = encoding
                return u"foo"

        loader = MockLoader()
        custom_loader = SpecLoader()
        custom_loader.loader = loader

        view = TemplateSpec()
        view.template = "template-foo"
        view.template_encoding = "encoding-foo"

        # Check that our unicode() above was called.
        self._assert_template(custom_loader, view, u'foo')
        self.assertEqual(loader.s, "template-foo")
        self.assertEqual(loader.encoding, "encoding-foo")

    def test_find__template_path(self):
        """Test _find() with TemplateSpec.template_path."""
        loader = self._make_specloader()
        custom = TemplateSpec()
        custom.template_path = "path/foo"
        actual = loader._find(custom)
        self.assertPaths(actual, "path/foo")


# TODO: migrate these tests into the SpecLoaderTests class.
# TODO: rename the get_template() tests to test load().
# TODO: condense, reorganize, and rename the tests so that it is
#   clear whether we have full test coverage (e.g. organized by
#   TemplateSpec attributes or something).
class TemplateSpecTests(unittest.TestCase, AssertPathsMixin):

    def _make_loader(self):
        return _make_specloader()

    def _assert_template_location(self, view, expected):
        loader = self._make_loader()
        actual = loader._find_relative(view)
        self.assertEqual(actual, expected)

    def test_find_relative(self):
        """
        Test _find_relative(): default behavior (no attributes set).

        """
        view = SampleView()
        self._assert_template_location(view, (None, 'sample_view.mustache'))

    def test_find_relative__template_rel_path__file_name_only(self):
        """
        Test _find_relative(): template_rel_path attribute.

        """
        view = SampleView()
        view.template_rel_path = 'template.txt'
        self._assert_template_location(view, ('', 'template.txt'))

    def test_find_relative__template_rel_path__file_name_with_directory(self):
        """
        Test _find_relative(): template_rel_path attribute.

        """
        view = SampleView()
        view.template_rel_path = 'foo/bar/template.txt'
        self._assert_template_location(view, ('foo/bar', 'template.txt'))

    def test_find_relative__template_rel_directory(self):
        """
        Test _find_relative(): template_rel_directory attribute.

        """
        view = SampleView()
        view.template_rel_directory = 'foo'

        self._assert_template_location(view, ('foo', 'sample_view.mustache'))

    def test_find_relative__template_name(self):
        """
        Test _find_relative(): template_name attribute.

        """
        view = SampleView()
        view.template_name = 'new_name'
        self._assert_template_location(view, (None, 'new_name.mustache'))

    def test_find_relative__template_extension(self):
        """
        Test _find_relative(): template_extension attribute.

        """
        view = SampleView()
        view.template_extension = 'txt'
        self._assert_template_location(view, (None, 'sample_view.txt'))

    def test_find__with_directory(self):
        """
        Test _find() with a view that has a directory specified.

        """
        loader = self._make_loader()

        view = SampleView()
        view.template_rel_path = os.path.join('foo', 'bar.txt')
        self.assertTrue(loader._find_relative(view)[0] is not None)

        actual = loader._find(view)
        expected = os.path.join(DATA_DIR, 'foo', 'bar.txt')

        self.assertPaths(actual, expected)

    def test_find__without_directory(self):
        """
        Test _find() with a view that doesn't have a directory specified.

        """
        loader = self._make_loader()

        view = SampleView()
        self.assertTrue(loader._find_relative(view)[0] is None)

        actual = loader._find(view)
        expected = os.path.join(DATA_DIR, 'sample_view.mustache')

        self.assertPaths(actual, expected)

    def _assert_get_template(self, custom, expected):
        loader = self._make_loader()
        actual = loader.load(custom)

        self.assertEqual(type(actual), unicode)
        self.assertEqual(actual, expected)

    def test_get_template(self):
        """
        Test get_template(): default behavior (no attributes set).

        """
        view = SampleView()

        self._assert_get_template(view, u"ascii: abc")

    def test_get_template__template_encoding(self):
        """
        Test get_template(): template_encoding attribute.

        """
        view = NonAscii()

        self.assertRaises(UnicodeDecodeError, self._assert_get_template, view, 'foo')

        view.template_encoding = 'utf-8'
        self._assert_get_template(view, u"non-ascii: é")
