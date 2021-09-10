
"""
TODO: add a docstring.

"""

from pystache import TemplateSpec

class UnicodeInput(TemplateSpec):

    template_encoding = 'utf8'

    def age(self):
        return 156
