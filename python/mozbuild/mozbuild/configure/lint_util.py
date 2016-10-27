# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import dis
import inspect


# dis.dis only outputs to stdout. This is a modified version that
# returns an iterator.
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
    while i < n:
        c = code[i]
        op = ord(c)
        opname = dis.opname[op]
        i += 1;
        if op >= dis.HAVE_ARGUMENT:
            arg = ord(code[i]) + ord(code[i + 1]) * 256 + extended_arg
            extended_arg = 0
            i += 2
            if op == dis.EXTENDED_ARG:
                extended_arg = arg * 65536L
                continue
            if op in dis.hasconst:
                yield opname, co.co_consts[arg]
            elif op in dis.hasname:
                yield opname, co.co_names[arg]
            elif op in dis.hasjrel:
                yield opname, i + arg
            elif op in dis.haslocal:
                yield opname, co.co_varnames[arg]
            elif op in dis.hascompare:
                yield opname, dis.cmp_op[arg]
            elif op in dis.hasfree:
                if free is None:
                    free = co.co_cellvars + co.co_freevars
                yield opname, free[arg]
            else:
                yield opname, None
        else:
            yield opname, None
