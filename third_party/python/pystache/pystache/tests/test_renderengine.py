# coding: utf-8

"""
Unit tests of renderengine.py.

"""

import sys
import unittest

from pystache.context import ContextStack, KeyNotFoundError
from pystache import defaults
from pystache.parser import ParsingError
from pystache.renderer import Renderer
from pystache.renderengine import context_get, RenderEngine
from pystache.tests.common import AssertStringMixin, AssertExceptionMixin, Attachable


def _get_unicode_char():
    if sys.version_info < (3, ):
        return 'u'
    return ''

_UNICODE_CHAR = _get_unicode_char()


def mock_literal(s):
    """
    For use as the literal keyword argument to the RenderEngine constructor.

    Arguments:

      s: a byte string or unicode string.

    """
    if isinstance(s, unicode):
        # Strip off unicode super classes, if present.
        u = unicode(s)
    else:
        u = unicode(s, encoding='ascii')

    # We apply upper() to make sure we are actually using our custom
    # function in the tests
    return u.upper()



class RenderEngineTestCase(unittest.TestCase):

    """Test the RenderEngine class."""

    def test_init(self):
        """
        Test that __init__() stores all of the arguments correctly.

        """
        # In real-life, these arguments would be functions
        engine = RenderEngine(resolve_partial="foo", literal="literal",
                              escape="escape", to_str="str")

        self.assertEqual(engine.escape, "escape")
        self.assertEqual(engine.literal, "literal")
        self.assertEqual(engine.resolve_partial, "foo")
        self.assertEqual(engine.to_str, "str")


class RenderTests(unittest.TestCase, AssertStringMixin, AssertExceptionMixin):

    """
    Tests RenderEngine.render().

    Explicit spec-test-like tests best go in this class since the
    RenderEngine class contains all parsing logic.  This way, the unit tests
    will be more focused and fail "closer to the code".

    """

    def _engine(self):
        """
        Create and return a default RenderEngine for testing.

        """
        renderer = Renderer(string_encoding='utf-8', missing_tags='strict')
        engine = renderer._make_render_engine()

        return engine

    def _assert_render(self, expected, template, *context, **kwargs):
        """
        Test rendering the given template using the given context.

        """
        partials = kwargs.get('partials')
        engine = kwargs.get('engine', self._engine())

        if partials is not None:
            engine.resolve_partial = lambda key: unicode(partials[key])

        context = ContextStack(*context)

        # RenderEngine.render() only accepts unicode template strings.
        actual = engine.render(unicode(template), context)

        self.assertString(actual=actual, expected=expected)

    def test_render(self):
        self._assert_render(u'Hi Mom', 'Hi {{person}}', {'person': 'Mom'})

    def test__resolve_partial(self):
        """
        Test that render() uses the load_template attribute.

        """
        engine = self._engine()
        partials = {'partial': u"{{person}}"}
        engine.resolve_partial = lambda key: partials[key]

        self._assert_render(u'Hi Mom', 'Hi {{>partial}}', {'person': 'Mom'}, engine=engine)

    def test__literal(self):
        """
        Test that render() uses the literal attribute.

        """
        engine = self._engine()
        engine.literal = lambda s: s.upper()

        self._assert_render(u'BAR', '{{{foo}}}', {'foo': 'bar'}, engine=engine)

    def test_literal__sigil(self):
        template = "<h1>{{& thing}}</h1>"
        context = {'thing': 'Bear > Giraffe'}

        expected = u"<h1>Bear > Giraffe</h1>"

        self._assert_render(expected, template, context)

    def test__escape(self):
        """
        Test that render() uses the escape attribute.

        """
        engine = self._engine()
        engine.escape = lambda s: "**" + s

        self._assert_render(u'**bar', '{{foo}}', {'foo': 'bar'}, engine=engine)

    def test__escape_does_not_call_literal(self):
        """
        Test that render() does not call literal before or after calling escape.

        """
        engine = self._engine()
        engine.literal = lambda s: s.upper()  # a test version
        engine.escape = lambda s: "**" + s

        template = 'literal: {{{foo}}} escaped: {{foo}}'
        context = {'foo': 'bar'}

        self._assert_render(u'literal: BAR escaped: **bar', template, context, engine=engine)

    def test__escape_preserves_unicode_subclasses(self):
        """
        Test that render() preserves unicode subclasses when passing to escape.

        This is useful, for example, if one wants to respect whether a
        variable value is markupsafe.Markup when escaping.

        """
        class MyUnicode(unicode):
            pass

        def escape(s):
            if type(s) is MyUnicode:
                return "**" + s
            else:
                return s + "**"

        engine = self._engine()
        engine.escape = escape

        template = '{{foo1}} {{foo2}}'
        context = {'foo1': MyUnicode('bar'), 'foo2': 'bar'}

        self._assert_render(u'**bar bar**', template, context, engine=engine)

    # Custom to_str for testing purposes.
    def _to_str(self, val):
        if not val:
            return ''
        else:
            return str(val)

    def test_to_str(self):
        """Test the to_str attribute."""
        engine = self._engine()
        template = '{{value}}'
        context = {'value': None}

        self._assert_render(u'None', template, context, engine=engine)
        engine.to_str = self._to_str
        self._assert_render(u'', template, context, engine=engine)

    def test_to_str__lambda(self):
        """Test the to_str attribute for a lambda."""
        engine = self._engine()
        template = '{{value}}'
        context = {'value': lambda: None}

        self._assert_render(u'None', template, context, engine=engine)
        engine.to_str = self._to_str
        self._assert_render(u'', template, context, engine=engine)

    def test_to_str__section_list(self):
        """Test the to_str attribute for a section list."""
        engine = self._engine()
        template = '{{#list}}{{.}}{{/list}}'
        context = {'list': [None, None]}

        self._assert_render(u'NoneNone', template, context, engine=engine)
        engine.to_str = self._to_str
        self._assert_render(u'', template, context, engine=engine)

    def test_to_str__section_lambda(self):
        # TODO: add a test for a "method with an arity of 1".
        pass

    def test__non_basestring__literal_and_escaped(self):
        """
        Test a context value that is not a basestring instance.

        """
        engine = self._engine()
        engine.escape = mock_literal
        engine.literal = mock_literal

        self.assertRaises(TypeError, engine.literal, 100)

        template = '{{text}} {{int}} {{{int}}}'
        context = {'int': 100, 'text': 'foo'}

        self._assert_render(u'FOO 100 100', template, context, engine=engine)

    def test_tag__output_not_interpolated(self):
        """
        Context values should not be treated as templates (issue #44).

        """
        template = '{{template}}: {{planet}}'
        context = {'template': '{{planet}}', 'planet': 'Earth'}
        self._assert_render(u'{{planet}}: Earth', template, context)

    def test_tag__output_not_interpolated__section(self):
        """
        Context values should not be treated as templates (issue #44).

        """
        template = '{{test}}'
        context = {'test': '{{#hello}}'}
        self._assert_render(u'{{#hello}}', template, context)

    ## Test interpolation with "falsey" values
    #
    # In these test cases, we test the part of the spec that says that
    # "data should be coerced into a string (and escaped, if appropriate)
    # before interpolation."  We test this for data that is "falsey."

    def test_interpolation__falsey__zero(self):
        template = '{{.}}'
        context = 0
        self._assert_render(u'0', template, context)

    def test_interpolation__falsey__none(self):
        template = '{{.}}'
        context = None
        self._assert_render(u'None', template, context)

    def test_interpolation__falsey__zero(self):
        template = '{{.}}'
        context = False
        self._assert_render(u'False', template, context)

    # Built-in types:
    #
    #   Confirm that we not treat instances of built-in types as objects,
    #   for example by calling a method on a built-in type instance when it
    #   has a method whose name matches the current key.
    #
    #   Each test case puts an instance of a built-in type on top of the
    #   context stack before interpolating a tag whose key matches an
    #   attribute (method or property) of the instance.
    #

    def _assert_builtin_attr(self, item, attr_name, expected_attr):
        self.assertTrue(hasattr(item, attr_name))
        actual = getattr(item, attr_name)
        if callable(actual):
            actual = actual()
        self.assertEqual(actual, expected_attr)

    def _assert_builtin_type(self, item, attr_name, expected_attr, expected_template):
        self._assert_builtin_attr(item, attr_name, expected_attr)

        template = '{{#section}}{{%s}}{{/section}}' % attr_name
        context = {'section': item, attr_name: expected_template}
        self._assert_render(expected_template, template, context)

    def test_interpolation__built_in_type__string(self):
        """
        Check tag interpolation with a built-in type: string.

        """
        self._assert_builtin_type('abc', 'upper', 'ABC', u'xyz')

    def test_interpolation__built_in_type__integer(self):
        """
        Check tag interpolation with a built-in type: integer.

        """
        # Since public attributes weren't added to integers until Python 2.6
        # (for example the "real" attribute of the numeric type hierarchy)--
        #
        #   http://docs.python.org/library/numbers.html
        #
        # we need to resort to built-in attributes (double-underscored) on
        # the integer type.
        self._assert_builtin_type(15, '__neg__', -15, u'999')

    def test_interpolation__built_in_type__list(self):
        """
        Check tag interpolation with a built-in type: list.

        """
        item = [[1, 2, 3]]
        attr_name = 'pop'
        # Make a copy to prevent changes to item[0].
        self._assert_builtin_attr(list(item[0]), attr_name, 3)

        template = '{{#section}}{{%s}}{{/section}}' % attr_name
        context = {'section': item, attr_name: 7}
        self._assert_render(u'7', template, context)

    # This test is also important for testing 2to3.
    def test_interpolation__nonascii_nonunicode(self):
        """
        Test a tag whose value is a non-ascii, non-unicode string.

        """
        template = '{{nonascii}}'
        context = {'nonascii': u'abcdé'.encode('utf-8')}
        self._assert_render(u'abcdé', template, context)

    def test_implicit_iterator__literal(self):
        """
        Test an implicit iterator in a literal tag.

        """
        template = """{{#test}}{{{.}}}{{/test}}"""
        context = {'test': ['<', '>']}

        self._assert_render(u'<>', template, context)

    def test_implicit_iterator__escaped(self):
        """
        Test an implicit iterator in a normal tag.

        """
        template = """{{#test}}{{.}}{{/test}}"""
        context = {'test': ['<', '>']}

        self._assert_render(u'&lt;&gt;', template, context)

    def test_literal__in_section(self):
        """
        Check that literals work in sections.

        """
        template = '{{#test}}1 {{{less_than}}} 2{{/test}}'
        context = {'test': {'less_than': '<'}}

        self._assert_render(u'1 < 2', template, context)

    def test_literal__in_partial(self):
        """
        Check that literals work in partials.

        """
        template = '{{>partial}}'
        partials = {'partial': '1 {{{less_than}}} 2'}
        context = {'less_than': '<'}

        self._assert_render(u'1 < 2', template, context, partials=partials)

    def test_partial(self):
        partials = {'partial': "{{person}}"}
        self._assert_render(u'Hi Mom', 'Hi {{>partial}}', {'person': 'Mom'}, partials=partials)

    def test_partial__context_values(self):
        """
        Test that escape and literal work on context values in partials.

        """
        engine = self._engine()

        template = '{{>partial}}'
        partials = {'partial': 'unescaped: {{{foo}}} escaped: {{foo}}'}
        context = {'foo': '<'}

        self._assert_render(u'unescaped: < escaped: &lt;', template, context, engine=engine, partials=partials)

    ## Test cases related specifically to lambdas.

    # This test is also important for testing 2to3.
    def test_section__nonascii_nonunicode(self):
        """
        Test a section whose value is a non-ascii, non-unicode string.

        """
        template = '{{#nonascii}}{{.}}{{/nonascii}}'
        context = {'nonascii': u'abcdé'.encode('utf-8')}
        self._assert_render(u'abcdé', template, context)

    # This test is also important for testing 2to3.
    def test_lambda__returning_nonascii_nonunicode(self):
        """
        Test a lambda tag value returning a non-ascii, non-unicode string.

        """
        template = '{{lambda}}'
        context = {'lambda': lambda: u'abcdé'.encode('utf-8')}
        self._assert_render(u'abcdé', template, context)

    ## Test cases related specifically to sections.

    def test_section__end_tag_with_no_start_tag(self):
        """
        Check what happens if there is an end tag with no start tag.

        """
        template = '{{/section}}'
        try:
            self._assert_render(None, template)
        except ParsingError, err:
            self.assertEqual(str(err), "Section end tag mismatch: section != None")

    def test_section__end_tag_mismatch(self):
        """
        Check what happens if the end tag doesn't match.

        """
        template = '{{#section_start}}{{/section_end}}'
        try:
            self._assert_render(None, template)
        except ParsingError, err:
            self.assertEqual(str(err), "Section end tag mismatch: section_end != section_start")

    def test_section__context_values(self):
        """
        Test that escape and literal work on context values in sections.

        """
        engine = self._engine()

        template = '{{#test}}unescaped: {{{foo}}} escaped: {{foo}}{{/test}}'
        context = {'test': {'foo': '<'}}

        self._assert_render(u'unescaped: < escaped: &lt;', template, context, engine=engine)

    def test_section__context_precedence(self):
        """
        Check that items higher in the context stack take precedence.

        """
        template = '{{entree}} : {{#vegetarian}}{{entree}}{{/vegetarian}}'
        context = {'entree': 'chicken', 'vegetarian': {'entree': 'beans and rice'}}
        self._assert_render(u'chicken : beans and rice', template, context)

    def test_section__list_referencing_outer_context(self):
        """
        Check that list items can access the parent context.

        For sections whose value is a list, check that items in the list
        have access to the values inherited from the parent context
        when rendering.

        """
        context = {
            "greeting": "Hi",
            "list": [{"name": "Al"}, {"name": "Bob"}],
        }

        template = "{{#list}}{{greeting}} {{name}}, {{/list}}"

        self._assert_render(u"Hi Al, Hi Bob, ", template, context)

    def test_section__output_not_interpolated(self):
        """
        Check that rendered section output is not interpolated.

        """
        template = '{{#section}}{{template}}{{/section}}: {{planet}}'
        context = {'section': True, 'template': '{{planet}}', 'planet': 'Earth'}
        self._assert_render(u'{{planet}}: Earth', template, context)

    # TODO: have this test case added to the spec.
    def test_section__string_values_not_lists(self):
        """
        Check that string section values are not interpreted as lists.

        """
        template = '{{#section}}foo{{/section}}'
        context = {'section': '123'}
        # If strings were interpreted as lists, this would give "foofoofoo".
        self._assert_render(u'foo', template, context)

    def test_section__nested_truthy(self):
        """
        Check that "nested truthy" sections get rendered.

        Test case for issue #24: https://github.com/defunkt/pystache/issues/24

        This test is copied from the spec.  We explicitly include it to
        prevent regressions for those who don't pull down the spec tests.

        """
        template = '| A {{#bool}}B {{#bool}}C{{/bool}} D{{/bool}} E |'
        context = {'bool': True}
        self._assert_render(u'| A B C D E |', template, context)

    def test_section__nested_with_same_keys(self):
        """
        Check a doubly-nested section with the same context key.

        Test case for issue #36: https://github.com/defunkt/pystache/issues/36

        """
        # Start with an easier, working case.
        template = '{{#x}}{{#z}}{{y}}{{/z}}{{/x}}'
        context = {'x': {'z': {'y': 1}}}
        self._assert_render(u'1', template, context)

        template = '{{#x}}{{#x}}{{y}}{{/x}}{{/x}}'
        context = {'x': {'x': {'y': 1}}}
        self._assert_render(u'1', template, context)

    def test_section__lambda(self):
        template = '{{#test}}Mom{{/test}}'
        context = {'test': (lambda text: 'Hi %s' % text)}
        self._assert_render(u'Hi Mom', template, context)

    # This test is also important for testing 2to3.
    def test_section__lambda__returning_nonascii_nonunicode(self):
        """
        Test a lambda section value returning a non-ascii, non-unicode string.

        """
        template = '{{#lambda}}{{/lambda}}'
        context = {'lambda': lambda text: u'abcdé'.encode('utf-8')}
        self._assert_render(u'abcdé', template, context)

    def test_section__lambda__returning_nonstring(self):
        """
        Test a lambda section value returning a non-string.

        """
        template = '{{#lambda}}foo{{/lambda}}'
        context = {'lambda': lambda text: len(text)}
        self._assert_render(u'3', template, context)

    def test_section__iterable(self):
        """
        Check that objects supporting iteration (aside from dicts) behave like lists.

        """
        template = '{{#iterable}}{{.}}{{/iterable}}'

        context = {'iterable': (i for i in range(3))}  # type 'generator'
        self._assert_render(u'012', template, context)

        context = {'iterable': xrange(4)}  # type 'xrange'
        self._assert_render(u'0123', template, context)

        d = {'foo': 0, 'bar': 0}
        # We don't know what order of keys we'll be given, but from the
        # Python documentation:
        #  "If items(), keys(), values(), iteritems(), iterkeys(), and
        #   itervalues() are called with no intervening modifications to
        #   the dictionary, the lists will directly correspond."
        expected = u''.join(d.keys())
        context = {'iterable': d.iterkeys()}  # type 'dictionary-keyiterator'
        self._assert_render(expected, template, context)

    def test_section__lambda__tag_in_output(self):
        """
        Check that callable output is treated as a template string (issue #46).

        The spec says--

            When used as the data value for a Section tag, the lambda MUST
            be treatable as an arity 1 function, and invoked as such (passing
            a String containing the unprocessed section contents).  The
            returned value MUST be rendered against the current delimiters,
            then interpolated in place of the section.

        """
        template = '{{#test}}Hi {{person}}{{/test}}'
        context = {'person': 'Mom', 'test': (lambda text: text + " :)")}
        self._assert_render(u'Hi Mom :)', template, context)

    def test_section__lambda__list(self):
        """
        Check that lists of lambdas are processed correctly for sections.

        This test case is equivalent to a test submitted to the Mustache spec here:

          https://github.com/mustache/spec/pull/47 .

        """
        template = '<{{#lambdas}}foo{{/lambdas}}>'
        context = {'foo': 'bar',
                   'lambdas': [lambda text: "~{{%s}}~" % text,
                               lambda text: "#{{%s}}#" % text]}

        self._assert_render(u'<~bar~#bar#>', template, context)

    def test_section__lambda__mixed_list(self):
        """
        Test a mixed list of lambdas and non-lambdas as a section value.

        This test case is equivalent to a test submitted to the Mustache spec here:

          https://github.com/mustache/spec/pull/47 .

        """
        template = '<{{#lambdas}}foo{{/lambdas}}>'
        context = {'foo': 'bar',
                   'lambdas': [lambda text: "~{{%s}}~" % text, 1]}

        self._assert_render(u'<~bar~foo>', template, context)

    def test_section__lambda__not_on_context_stack(self):
        """
        Check that section lambdas are not pushed onto the context stack.

        Even though the sections spec says that section data values should be
        pushed onto the context stack prior to rendering, this does not apply
        to lambdas.  Lambdas obey their own special case.

        This test case is equivalent to a test submitted to the Mustache spec here:

          https://github.com/mustache/spec/pull/47 .

        """
        context = {'foo': 'bar', 'lambda': (lambda text: "{{.}}")}
        template = '{{#foo}}{{#lambda}}blah{{/lambda}}{{/foo}}'
        self._assert_render(u'bar', template, context)

    def test_section__lambda__no_reinterpolation(self):
        """
        Check that section lambda return values are not re-interpolated.

        This test is a sanity check that the rendered lambda return value
        is not re-interpolated as could be construed by reading the
        section part of the Mustache spec.

        This test case is equivalent to a test submitted to the Mustache spec here:

          https://github.com/mustache/spec/pull/47 .

        """
        template = '{{#planet}}{{#lambda}}dot{{/lambda}}{{/planet}}'
        context = {'planet': 'Earth', 'dot': '~{{.}}~', 'lambda': (lambda text: "#{{%s}}#" % text)}
        self._assert_render(u'#~{{.}}~#', template, context)

    def test_comment__multiline(self):
        """
        Check that multiline comments are permitted.

        """
        self._assert_render(u'foobar', 'foo{{! baz }}bar')
        self._assert_render(u'foobar', 'foo{{! \nbaz }}bar')

    def test_custom_delimiters__sections(self):
        """
        Check that custom delimiters can be used to start a section.

        Test case for issue #20: https://github.com/defunkt/pystache/issues/20

        """
        template = '{{=[[ ]]=}}[[#foo]]bar[[/foo]]'
        context = {'foo': True}
        self._assert_render(u'bar', template, context)

    def test_custom_delimiters__not_retroactive(self):
        """
        Check that changing custom delimiters back is not "retroactive."

        Test case for issue #35: https://github.com/defunkt/pystache/issues/35

        """
        expected = u' {{foo}} '
        self._assert_render(expected, '{{=$ $=}} {{foo}} ')
        self._assert_render(expected, '{{=$ $=}} {{foo}} $={{ }}=$')  # was yielding u'  '.

    def test_dot_notation(self):
        """
        Test simple dot notation cases.

        Check that we can use dot notation when the variable is a dict,
        user-defined object, or combination of both.

        """
        template = 'Hello, {{person.name}}. I see you are {{person.details.age}}.'
        person = Attachable(name='Biggles', details={'age': 42})
        context = {'person': person}
        self._assert_render(u'Hello, Biggles. I see you are 42.', template, context)

    def test_dot_notation__multiple_levels(self):
        """
        Test dot notation with multiple levels.

        """
        template = """Hello, Mr. {{person.name.lastname}}.
        I see you're back from {{person.travels.last.country.city}}."""
        expected = u"""Hello, Mr. Pither.
        I see you're back from Cornwall."""
        context = {'person': {'name': {'firstname': 'unknown', 'lastname': 'Pither'},
                            'travels': {'last': {'country': {'city': 'Cornwall'}}},
                            'details': {'public': 'likes cycling'}}}
        self._assert_render(expected, template, context)

        # It should also work with user-defined objects
        context = {'person': Attachable(name={'firstname': 'unknown', 'lastname': 'Pither'},
                                        travels=Attachable(last=Attachable(country=Attachable(city='Cornwall'))),
                                        details=Attachable())}
        self._assert_render(expected, template, context)

    def test_dot_notation__missing_part_terminates_search(self):
        """
        Test that dotted name resolution terminates on a later part not found.

        Check that if a later dotted name part is not found in the result from
        the former resolution, then name resolution terminates rather than
        starting the search over with the next element of the context stack.
        From the spec (interpolation section)--

          5) If any name parts were retained in step 1, each should be resolved
          against a context stack containing only the result from the former
          resolution.  If any part fails resolution, the result should be considered
          falsey, and should interpolate as the empty string.

        This test case is equivalent to the test case in the following pull
        request:

          https://github.com/mustache/spec/pull/48

        """
        context = {'a': {'b': 'A.B'}, 'c': {'a': 'A'} }

        template = '{{a.b}}'
        self._assert_render(u'A.B', template, context)

        template = '{{#c}}{{a}}{{/c}}'
        self._assert_render(u'A', template, context)

        template = '{{#c}}{{a.b}}{{/c}}'
        self.assertException(KeyNotFoundError, "Key %(unicode)s'a.b' not found: missing %(unicode)s'b'" %
                             {'unicode': _UNICODE_CHAR},
                             self._assert_render, 'A.B :: (A :: )', template, context)
