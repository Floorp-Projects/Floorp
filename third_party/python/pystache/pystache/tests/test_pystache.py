# encoding: utf-8

import unittest

import pystache
from pystache import defaults
from pystache import renderer
from pystache.tests.common import html_escape


class PystacheTests(unittest.TestCase):


    def setUp(self):
        self.original_escape = defaults.TAG_ESCAPE
        defaults.TAG_ESCAPE = html_escape

    def tearDown(self):
        defaults.TAG_ESCAPE = self.original_escape

    def _assert_rendered(self, expected, template, context):
        actual = pystache.render(template, context)
        self.assertEqual(actual, expected)

    def test_basic(self):
        ret = pystache.render("Hi {{thing}}!", { 'thing': 'world' })
        self.assertEqual(ret, "Hi world!")

    def test_kwargs(self):
        ret = pystache.render("Hi {{thing}}!", thing='world')
        self.assertEqual(ret, "Hi world!")

    def test_less_basic(self):
        template = "It's a nice day for {{beverage}}, right {{person}}?"
        context = { 'beverage': 'soda', 'person': 'Bob' }
        self._assert_rendered("It's a nice day for soda, right Bob?", template, context)

    def test_even_less_basic(self):
        template = "I think {{name}} wants a {{thing}}, right {{name}}?"
        context = { 'name': 'Jon', 'thing': 'racecar' }
        self._assert_rendered("I think Jon wants a racecar, right Jon?", template, context)

    def test_ignores_misses(self):
        template = "I think {{name}} wants a {{thing}}, right {{name}}?"
        context = { 'name': 'Jon' }
        self._assert_rendered("I think Jon wants a , right Jon?", template, context)

    def test_render_zero(self):
        template = 'My value is {{value}}.'
        context = { 'value': 0 }
        self._assert_rendered('My value is 0.', template, context)

    def test_comments(self):
        template = "What {{! the }} what?"
        actual = pystache.render(template)
        self.assertEqual("What  what?", actual)

    def test_false_sections_are_hidden(self):
        template = "Ready {{#set}}set {{/set}}go!"
        context = { 'set': False }
        self._assert_rendered("Ready go!", template, context)

    def test_true_sections_are_shown(self):
        template = "Ready {{#set}}set{{/set}} go!"
        context = { 'set': True }
        self._assert_rendered("Ready set go!", template, context)

    non_strings_expected = """(123 & [&#x27;something&#x27;])(chris & 0.9)"""

    def test_non_strings(self):
        template = "{{#stats}}({{key}} & {{value}}){{/stats}}"
        stats = []
        stats.append({'key': 123, 'value': ['something']})
        stats.append({'key': u"chris", 'value': 0.900})
        context = { 'stats': stats }
        self._assert_rendered(self.non_strings_expected, template, context)

    def test_unicode(self):
        template = 'Name: {{name}}; Age: {{age}}'
        context = {'name': u'Henri Poincaré', 'age': 156 }
        self._assert_rendered(u'Name: Henri Poincaré; Age: 156', template, context)

    def test_sections(self):
        template = """<ul>{{#users}}<li>{{name}}</li>{{/users}}</ul>"""

        context = { 'users': [ {'name': 'Chris'}, {'name': 'Tom'}, {'name': 'PJ'} ] }
        expected = """<ul><li>Chris</li><li>Tom</li><li>PJ</li></ul>"""
        self._assert_rendered(expected, template, context)

    def test_implicit_iterator(self):
        template = """<ul>{{#users}}<li>{{.}}</li>{{/users}}</ul>"""
        context = { 'users': [ 'Chris', 'Tom','PJ' ] }
        expected = """<ul><li>Chris</li><li>Tom</li><li>PJ</li></ul>"""
        self._assert_rendered(expected, template, context)

    # The spec says that sections should not alter surrounding whitespace.
    def test_surrounding_whitepace_not_altered(self):
        template = "first{{#spacing}} second {{/spacing}}third"
        context = {"spacing": True}
        self._assert_rendered("first second third", template, context)

    def test__section__non_false_value(self):
        """
        Test when a section value is a (non-list) "non-false value".

        From mustache(5):

            When the value [of a section key] is non-false but not a list, it
            will be used as the context for a single rendering of the block.

        """
        template = """{{#person}}Hi {{name}}{{/person}}"""
        context = {"person": {"name": "Jon"}}
        self._assert_rendered("Hi Jon", template, context)

    def test_later_list_section_with_escapable_character(self):
        """
        This is a simple test case intended to cover issue #53.

        The test case failed with markupsafe enabled, as follows:

        AssertionError: Markup(u'foo &lt;') != 'foo <'

        """
        template = """{{#s1}}foo{{/s1}} {{#s2}}<{{/s2}}"""
        context = {'s1': True, 's2': [True]}
        self._assert_rendered("foo <", template, context)
