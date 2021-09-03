import unittest

import pystache
from pystache import Renderer
from examples.nested_context import NestedContext
from examples.complex import Complex
from examples.lambdas import Lambdas
from examples.template_partial import TemplatePartial
from examples.simple import Simple

from pystache.tests.common import EXAMPLES_DIR
from pystache.tests.common import AssertStringMixin


class TestSimple(unittest.TestCase, AssertStringMixin):

    def test_nested_context(self):
        renderer = Renderer()
        view = NestedContext(renderer)
        view.template = '{{#foo}}{{thing1}} and {{thing2}} and {{outer_thing}}{{/foo}}{{^foo}}Not foo!{{/foo}}'

        actual = renderer.render(view)
        self.assertString(actual, u"one and foo and two")

    def test_looping_and_negation_context(self):
        template = '{{#item}}{{header}}: {{name}} {{/item}}{{^item}} Shouldnt see me{{/item}}'
        context = Complex()

        renderer = Renderer()
        actual = renderer.render(template, context)
        self.assertEqual(actual, "Colors: red Colors: green Colors: blue ")

    def test_empty_context(self):
        template = '{{#empty_list}}Shouldnt see me {{/empty_list}}{{^empty_list}}Should see me{{/empty_list}}'
        self.assertEqual(pystache.Renderer().render(template), "Should see me")

    def test_callables(self):
        view = Lambdas()
        view.template = '{{#replace_foo_with_bar}}foo != bar. oh, it does!{{/replace_foo_with_bar}}'

        renderer = Renderer()
        actual = renderer.render(view)
        self.assertString(actual, u'bar != bar. oh, it does!')

    def test_rendering_partial(self):
        renderer = Renderer(search_dirs=EXAMPLES_DIR)

        view = TemplatePartial(renderer=renderer)
        view.template = '{{>inner_partial}}'

        actual = renderer.render(view)
        self.assertString(actual, u'Again, Welcome!')

        view.template = '{{#looping}}{{>inner_partial}} {{/looping}}'
        actual = renderer.render(view)
        self.assertString(actual, u"Again, Welcome! Again, Welcome! Again, Welcome! ")

    def test_non_existent_value_renders_blank(self):
        view = Simple()
        template = '{{not_set}} {{blank}}'
        self.assertEqual(pystache.Renderer().render(template), ' ')


    def test_template_partial_extension(self):
        """
        Side note:

        From the spec--

            Partial tags SHOULD be treated as standalone when appropriate.

        In particular, this means that trailing newlines should be removed.

        """
        renderer = Renderer(search_dirs=EXAMPLES_DIR, file_extension='txt')

        view = TemplatePartial(renderer=renderer)

        actual = renderer.render(view)
        self.assertString(actual, u"""Welcome
-------

## Again, Welcome! ##""")
