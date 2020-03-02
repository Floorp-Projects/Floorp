# -*- coding: utf-8 -*-
# Copyright (C) 2018 ClearScore
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Use this rule to forbid any string values that are not quoted.
You can also enforce the type of the quote used using the ``quote-type`` option
(``single``, ``double`` or ``any``).

**Note**: Multi-line strings (with ``|`` or ``>``) will not be checked.

.. rubric:: Examples

#. With ``quoted-strings: {quote-type: any}``

   the following code snippet would **PASS**:
   ::

    foo: "bar"
    bar: 'foo'
    number: 123
    boolean: true

   the following code snippet would **FAIL**:
   ::

    foo: bar
"""

import yaml

from yamllint.linter import LintProblem

ID = 'quoted-strings'
TYPE = 'token'
CONF = {'quote-type': ('any', 'single', 'double')}
DEFAULT = {'quote-type': 'any'}


def check(conf, token, prev, next, nextnext, context):
    quote_type = conf['quote-type']

    if (isinstance(token, yaml.tokens.ScalarToken) and
            isinstance(prev, (yaml.ValueToken, yaml.TagToken))):
        # Ignore explicit types, e.g. !!str testtest or !!int 42
        if (prev and isinstance(prev, yaml.tokens.TagToken) and
                prev.value[0] == '!!'):
            return

        # Ignore numbers, booleans, etc.
        resolver = yaml.resolver.Resolver()
        if resolver.resolve(yaml.nodes.ScalarNode, token.value,
                            (True, False)) != 'tag:yaml.org,2002:str':
            return

        # Ignore multi-line strings
        if (not token.plain) and (token.style == "|" or token.style == ">"):
            return

        if ((quote_type == 'single' and token.style != "'") or
                (quote_type == 'double' and token.style != '"') or
                (quote_type == 'any' and token.style is None)):
            yield LintProblem(
                token.start_mark.line + 1,
                token.start_mark.column + 1,
                "string value is not quoted with %s quotes" % (quote_type))
