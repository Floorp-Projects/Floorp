#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import platform
import sys

from mock import MagicMock, patch
from fix_stack_using_bpsyms import fixSymbols, SymbolFile


def test_fixSymbols_non_stack_line():
    line = 'foobar\n'
    res = fixSymbols(line, '.')
    assert res == line


def test_fix_Symbols_missing_symbol():
    line = '#01: foobar[{:s}foo.{:s} +0xfc258]\n'
    result = '#01: foo.{:s} + 0xfc258\n'
    if platform.system() == "Windows":
        line = line.format('C:\\application\\firefox\\', 'dll')
        result = result.format('dll')
    else:
        line = line.format('/home/firerox/', 'so', '')
        result = result.format('so')

    output = fixSymbols(line, '.')
    assert output == result


symfile_data = '''FILE 0 source.c
FUNC fc256 2 0 func
FUNC fc258 4 0 func_line
fc258 2 1 0
fc25a 2 2 0
FUNC m fc25c 2 0 func_multiple
PUBLIC fc256 0 func_public
PUBLIC fc260 0 other_public'''

open_name = \
    ('builtins.%s' if sys.version_info >= (3,) else '__builtin__.%s') % 'open'


@patch(open_name, spec=open)
def test_parse_SymbolFile(mock_open):
    m = MagicMock()
    m.__enter__.return_value.__iter__.return_value = (symfile_data.split('\n'))
    m.__exit__.return_value = False
    mock_open.side_effect = [m]
    symfile = SymbolFile('foo.sym')
    assert symfile is not None
    assert symfile.addrToSymbol(int('fc255', 16)) == ''
    assert symfile.addrToSymbol(int('fc256', 16)) == 'func'
    assert symfile.addrToSymbol(int('fc258', 16)) == 'func_line [source.c:1]'
    assert symfile.addrToSymbol(int('fc25a', 16)) == 'func_line [source.c:2]'
    assert symfile.addrToSymbol(int('fc25b', 16)) == 'func_line [source.c:2]'
    assert symfile.addrToSymbol(int('fc260', 16)) == 'other_public'
    assert symfile.addrToSymbol(int('fc261', 16)) == 'other_public'


if __name__ == '__main__':
    mozunit.main()
