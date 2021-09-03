
"""
TODO: add a docstring.

"""

from pystache import TemplateSpec

class TemplatePartial(TemplateSpec):

    def __init__(self, renderer):
        self.renderer = renderer

    def _context_get(self, key):
        return self.renderer.context.get(key)

    def title(self):
        return "Welcome"

    def title_bars(self):
        return '-' * len(self.title())

    def looping(self):
        return [{'item': 'one'}, {'item': 'two'}, {'item': 'three'}]

    def thing(self):
        return self._context_get('prop')
