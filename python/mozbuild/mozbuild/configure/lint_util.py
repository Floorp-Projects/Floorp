# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import dis
import inspect
import itertools


# Like python 3.2's itertools.accumulate
def accumulate(iterable):
    t = 0
    for i in iterable:
        t += i
        yield t


# dis.dis only outputs to stdout. This is a modified version that
# returns an iterator, and includes line numbers.
def disassemble_as_iter(co):
    if inspect.ismethod(co):
        co = co.im_func
    if inspect.isfunction(co):
        co = co.func_code
    code = co.co_code
    n = len(code)
    i = 0
    extended_arg = 0
    free = None
    line = 0
    # co_lnotab is a string where each pair of consecutive character is
    # (chr(byte_increment), chr(line_increment)), mapping bytes in co_code
    # to line numbers relative to co_firstlineno.
    # We want to iterate over pairs of (accumulated_byte, accumulated_line).
    lnotab = itertools.chain(
        itertools.izip(accumulate(ord(c) for c in co.co_lnotab[0::2]),
                       accumulate(ord(c) for c in co.co_lnotab[1::2])),
        (None,))
    next_byte_line = lnotab.next()
    while i < n:
        while next_byte_line and i >= next_byte_line[0]:
            line = next_byte_line[1]
            next_byte_line = lnotab.next()
        c = code[i]
        op = ord(c)
        opname = dis.opname[op]
        i += 1
        if op >= dis.HAVE_ARGUMENT:
            arg = ord(code[i]) + ord(code[i + 1]) * 256 + extended_arg
            extended_arg = 0
            i += 2
            if op == dis.EXTENDED_ARG:
                extended_arg = arg * 65536
                continue
            if op in dis.hasconst:
                yield opname, co.co_consts[arg], line
            elif op in dis.hasname:
                yield opname, co.co_names[arg], line
            elif op in dis.hasjrel:
                yield opname, i + arg, line
            elif op in dis.haslocal:
                yield opname, co.co_varnames[arg], line
            elif op in dis.hascompare:
                yield opname, dis.cmp_op[arg], line
            elif op in dis.hasfree:
                if free is None:
                    free = co.co_cellvars + co.co_freevars
                yield opname, free[arg], line
            else:
                yield opname, None, line
        else:
            yield opname, None, line
