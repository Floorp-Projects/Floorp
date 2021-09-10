
"""
TODO: add a docstring.

"""

from pystache import TemplateSpec

class NestedContext(TemplateSpec):

    def __init__(self, renderer):
        self.renderer = renderer

    def _context_get(self, key):
        return self.renderer.context.get(key)

    def outer_thing(self):
        return "two"

    def foo(self):
        return {'thing1': 'one', 'thing2': 'foo'}

    def derp(self):
        return [{'inner': 'car'}]

    def herp(self):
        return [{'outer': 'car'}]

    def nested_context_in_view(self):
        if self._context_get('outer') == self._context_get('inner'):
            return 'it works!'
        return ''
