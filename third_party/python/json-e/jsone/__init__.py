from __future__ import absolute_import, print_function, unicode_literals

import re
from .render import renderValue
from .shared import JSONTemplateError, DeleteMarker, TemplateError, fromNow
from . import builtins

_context_re = re.compile(r'[a-zA-Z_][a-zA-Z0-9_]*$')


def render(template, context):
    if not all(_context_re.match(c) for c in context):
        raise TemplateError('top level keys of context must follow '
                            '/[a-zA-Z_][a-zA-Z0-9_]*/')
    full_context = {'now': fromNow('0 seconds', None)}
    full_context.update(builtins.build(full_context))
    full_context.update(context)
    rv = renderValue(template, full_context)
    if rv is DeleteMarker:
        return None
    return rv
