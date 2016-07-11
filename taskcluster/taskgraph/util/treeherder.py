# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
import re


def split_symbol(treeherder_symbol):
    """Split a symbol expressed as grp(sym) into its two parts.  If no group is
    given, the returned group is '?'"""
    groupSymbol = '?'
    symbol = treeherder_symbol
    if '(' in symbol:
        groupSymbol, symbol = re.match(r'([^(]*)\(([^)]*)\)', symbol).groups()
    return groupSymbol, symbol


def join_symbol(group, symbol):
    """Perform the reverse of split_symbol, combining the given group and
    symbol.  If the group is '?', then it is omitted."""
    if group == '?':
        return symbol
    return '{}({})'.format(group, symbol)
