# coding: utf-8

"""
Exposes a class that represents a parsed (or compiled) template.

"""


class ParsedTemplate(object):

    """
    Represents a parsed or compiled template.

    An instance wraps a list of unicode strings and node objects.  A node
    object must have a `render(engine, stack)` method that accepts a
    RenderEngine instance and a ContextStack instance and returns a unicode
    string.

    """

    def __init__(self):
        self._parse_tree = []

    def __repr__(self):
        return repr(self._parse_tree)

    def add(self, node):
        """
        Arguments:

          node: a unicode string or node object instance.  See the class
            docstring for information.

        """
        self._parse_tree.append(node)

    def render(self, engine, context):
        """
        Returns: a string of type unicode.

        """
        # We avoid use of the ternary operator for Python 2.4 support.
        def get_unicode(node):
            if type(node) is unicode:
                return node
            return node.render(engine, context)
        parts = map(get_unicode, self._parse_tree)
        s = ''.join(parts)

        return unicode(s)
