
"""
TODO: add a docstring.

"""

from pystache import TemplateSpec

class Inverted(object):

    def t(self):
        return True

    def f(self):
        return False

    def two(self):
        return 'two'

    def empty_list(self):
        return []

    def populated_list(self):
        return ['some_value']

class InvertedLists(Inverted, TemplateSpec):
    template_name = 'inverted'

    def t(self):
        return [0, 1, 2]

    def f(self):
        return []
