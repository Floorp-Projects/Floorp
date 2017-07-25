from __future__ import absolute_import, print_function, unicode_literals

import re
from .render import renderValue
from .shared import JSONTemplateError, DeleteMarker
from .builtins import builtins

_context_re = re.compile(r'[a-zA-Z_][a-zA-Z0-9_]*$')


def render(template, context):
    if not all(_context_re.match(c) for c in context):
        raise JSONTemplateError('top level keys of context must follow '
                                '/[a-zA-Z_][a-zA-Z0-9_]*/')
    full_context = builtins.copy()
    full_context.update(context)
    rv = renderValue(template, full_context)
    if rv is DeleteMarker:
        return None
    return rv
