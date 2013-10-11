# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import importlib

from sphinx.util.compat import Directive
from sphinx.util.docstrings import prepare_docstring


def function_reference(f, attr, args, doc):
    lines = []

    lines.extend([
        f,
        '-' * len(f),
        '',
    ])

    docstring = prepare_docstring(doc)

    lines.extend([
        docstring[0],
        '',
    ])

    arg_types = []

    for t in args:
        if isinstance(t, list):
            inner_types = [t2.__name__ for t2 in t]
            arg_types.append(' | ' .join(inner_types))
            continue

        arg_types.append(t.__name__)

    arg_s = '(%s)' % ', '.join(arg_types)

    lines.extend([
        ':Arguments: %s' % arg_s,
        '',
    ])

    lines.extend(docstring[1:])
    lines.append('')

    return lines


def variable_reference(v, st_type, in_type, default, doc, tier):
    lines = [
        v,
        '-' * len(v),
        '',
    ]

    docstring = prepare_docstring(doc)

    lines.extend([
        docstring[0],
        '',
    ])

    lines.extend([
        ':Storage Type: ``%s``' % st_type.__name__,
        ':Input Type: ``%s``' % in_type.__name__,
        ':Default Value: %s' % default,
        '',
    ])

    lines.extend(docstring[1:])
    lines.append('')

    return lines


def special_reference(v, typ, doc):
    lines = [
        v,
        '-' * len(v),
        '',
    ]

    docstring = prepare_docstring(doc)

    lines.extend([
        docstring[0],
        '',
        ':Type: ``%s``' % typ.__name__,
        '',
    ])

    lines.extend(docstring[1:])
    lines.append('')

    return lines


def format_module(m):
    lines = []
    lines.extend([
        'Variables',
        '=========',
        '',
    ])

    for v in sorted(m.VARIABLES):
        lines.extend(variable_reference(v, *m.VARIABLES[v]))

    lines.extend([
        'Functions',
        '=========',
        '',
    ])

    for func in sorted(m.FUNCTIONS):
        lines.extend(function_reference(func, *m.FUNCTIONS[func]))

    lines.extend([
        'Special Variables',
        '=================',
        '',
    ])

    for v in sorted(m.SPECIAL_VARIABLES):
        lines.extend(special_reference(v, *m.SPECIAL_VARIABLES[v]))

    return lines


class MozbuildSymbols(Directive):
    """Directive to insert mozbuild sandbox symbol information."""

    required_arguments = 1

    def run(self):
        module = importlib.import_module(self.arguments[0])
        fname = module.__file__
        if fname.endswith('.pyc'):
            fname = fname[0:-1]

        self.state.document.settings.record_dependencies.add(fname)

        # We simply format out the documentation as rst then feed it back
        # into the parser for conversion. We don't even emit ourselves, so
        # there's no record of us.
        self.state_machine.insert_input(format_module(module), fname)

        return []


def setup(app):
    app.add_directive('mozbuildsymbols', MozbuildSymbols)
