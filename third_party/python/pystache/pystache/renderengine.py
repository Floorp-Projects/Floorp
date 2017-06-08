# coding: utf-8

"""
Defines a class responsible for rendering logic.

"""

import re

from pystache.common import is_string
from pystache.parser import parse


def context_get(stack, name):
    """
    Find and return a name from a ContextStack instance.

    """
    return stack.get(name)


class RenderEngine(object):

    """
    Provides a render() method.

    This class is meant only for internal use.

    As a rule, the code in this class operates on unicode strings where
    possible rather than, say, strings of type str or markupsafe.Markup.
    This means that strings obtained from "external" sources like partials
    and variable tag values are immediately converted to unicode (or
    escaped and converted to unicode) before being operated on further.
    This makes maintaining, reasoning about, and testing the correctness
    of the code much simpler.  In particular, it keeps the implementation
    of this class independent of the API details of one (or possibly more)
    unicode subclasses (e.g. markupsafe.Markup).

    """

    # TODO: it would probably be better for the constructor to accept
    #   and set as an attribute a single RenderResolver instance
    #   that encapsulates the customizable aspects of converting
    #   strings and resolving partials and names from context.
    def __init__(self, literal=None, escape=None, resolve_context=None,
                 resolve_partial=None, to_str=None):
        """
        Arguments:

          literal: the function used to convert unescaped variable tag
            values to unicode, e.g. the value corresponding to a tag
            "{{{name}}}".  The function should accept a string of type
            str or unicode (or a subclass) and return a string of type
            unicode (but not a proper subclass of unicode).
                This class will only pass basestring instances to this
            function.  For example, it will call str() on integer variable
            values prior to passing them to this function.

          escape: the function used to escape and convert variable tag
            values to unicode, e.g. the value corresponding to a tag
            "{{name}}".  The function should obey the same properties
            described above for the "literal" function argument.
                This function should take care to convert any str
            arguments to unicode just as the literal function should, as
            this class will not pass tag values to literal prior to passing
            them to this function.  This allows for more flexibility,
            for example using a custom escape function that handles
            incoming strings of type markupsafe.Markup differently
            from plain unicode strings.

          resolve_context: the function to call to resolve a name against
            a context stack.  The function should accept two positional
            arguments: a ContextStack instance and a name to resolve.

          resolve_partial: the function to call when loading a partial.
            The function should accept a template name string and return a
            template string of type unicode (not a subclass).

          to_str: a function that accepts an object and returns a string (e.g.
            the built-in function str).  This function is used for string
            coercion whenever a string is required (e.g. for converting None
            or 0 to a string).

        """
        self.escape = escape
        self.literal = literal
        self.resolve_context = resolve_context
        self.resolve_partial = resolve_partial
        self.to_str = to_str

    # TODO: Rename context to stack throughout this module.

    # From the spec:
    #
    #   When used as the data value for an Interpolation tag, the lambda
    #   MUST be treatable as an arity 0 function, and invoked as such.
    #   The returned value MUST be rendered against the default delimiters,
    #   then interpolated in place of the lambda.
    #
    def fetch_string(self, context, name):
        """
        Get a value from the given context as a basestring instance.

        """
        val = self.resolve_context(context, name)

        if callable(val):
            # Return because _render_value() is already a string.
            return self._render_value(val(), context)

        if not is_string(val):
            return self.to_str(val)

        return val

    def fetch_section_data(self, context, name):
        """
        Fetch the value of a section as a list.

        """
        data = self.resolve_context(context, name)

        # From the spec:
        #
        #   If the data is not of a list type, it is coerced into a list
        #   as follows: if the data is truthy (e.g. `!!data == true`),
        #   use a single-element list containing the data, otherwise use
        #   an empty list.
        #
        if not data:
            data = []
        else:
            # The least brittle way to determine whether something
            # supports iteration is by trying to call iter() on it:
            #
            #   http://docs.python.org/library/functions.html#iter
            #
            # It is not sufficient, for example, to check whether the item
            # implements __iter__ () (the iteration protocol).  There is
            # also __getitem__() (the sequence protocol).  In Python 2,
            # strings do not implement __iter__(), but in Python 3 they do.
            try:
                iter(data)
            except TypeError:
                # Then the value does not support iteration.
                data = [data]
            else:
                if is_string(data) or isinstance(data, dict):
                    # Do not treat strings and dicts (which are iterable) as lists.
                    data = [data]
                # Otherwise, treat the value as a list.

        return data

    def _render_value(self, val, context, delimiters=None):
        """
        Render an arbitrary value.

        """
        if not is_string(val):
            # In case the template is an integer, for example.
            val = self.to_str(val)
        if type(val) is not unicode:
            val = self.literal(val)
        return self.render(val, context, delimiters)

    def render(self, template, context_stack, delimiters=None):
        """
        Render a unicode template string, and return as unicode.

        Arguments:

          template: a template string of type unicode (but not a proper
            subclass of unicode).

          context_stack: a ContextStack instance.

        """
        parsed_template = parse(template, delimiters)

        return parsed_template.render(self, context_stack)
