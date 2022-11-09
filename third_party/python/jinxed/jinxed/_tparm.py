# -*- coding: utf-8 -*-
# Copyright 2019 Avram Lubkin, All Rights Reserved

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
An pure Python implementation of tparm
Based on documentation in man(5) terminfo and comparing behavior of curses.tparm
"""

from collections import deque
import operator
import re


OPERATORS = {b'+': operator.add,
             b'-': operator.sub,
             b'*': operator.mul,
             b'/': operator.floordiv,
             b'm': operator.mod,
             b'&': operator.and_,
             b'|': operator.or_,
             b'^': operator.xor,
             b'=': operator.eq,
             b'>': operator.gt,
             b'<': operator.lt,
             b'~': operator.inv,
             b'!': operator.not_,
             b'A': lambda x, y: bool(x and y),
             b'O': lambda x, y: bool(x or y)}

FILTERS = (('_literal_percent', br'%%'),
           ('_pop_c', br'%c'),
           ('_increment_one_two', br'%i'),
           ('_binary_op', br'%([\+\-\*/m\&\|\^=><AO])'),
           ('_unary_op', br'%([\~!])'),
           ('_push_param', br'%p([1-9]\d*)'),
           ('_set_dynamic', br'%P([a-z])'),
           ('_get_dynamic', br'%g([a-z])'),
           ('_set_static', br'%P([A-Z])'),
           ('_get_static', br'%g([A-Z])'),
           ('_char_constant', br"%'(.)'"),
           ('_int_constant', br'%{(\d+)}'),
           ('_push_len', br'%l'),
           ('_cond_if', br'%\?(.+?)(?=%t)'),
           ('_cond_then_else', br'%t(.+?)(?=%e)'),
           ('_cond_then_fi', br'%t(.+?)(?=%;)'),
           ('_cond_else', br'%e(.+?)(?=%;|$)'),
           ('_cond_fi', br'%;'),
           ('_printf', br'%:?[^%]*?[doxXs]'),
           ('_unmatched', br'%.'),
           ('_literal', br'[^%]+'))

PATTERNS = tuple((re.compile(pattern), filter_) for filter_, pattern in FILTERS)
NULL = type('Null', (int,), {})(0)


class TParm(object):  # pylint: disable=useless-object-inheritance
    """
    Class to hold tparm methods and persist variables between calls
    """

    def __init__(self, *params, **kwargs):

        self.rtn = b''
        self.stack = deque()

        # The spec for tparm allows c string parameters, but most implementations don't
        # The reference code makes a best effort to determine which parameters require strings
        # We'll allow them without trying to predict
        for param in params:
            if not isinstance(param, (int, bytes)):
                raise TypeError('Parameters must be integers or bytes, not %s' %
                                type(param).__name__)
        self.params = list(params)

        static = kwargs.get('static', None)
        self.static = {} if static is None else static
        dynamic = kwargs.get('static', None)
        self.dynamic = {} if dynamic is None else dynamic

    def __call__(self, string, *params):
        return self.child(*params).parse(string)

    def _literal_percent(self, group):  # pylint: disable=unused-argument
        """
        Literal percent sign
        """
        self.rtn += b'%'

    def _pop_c(self, group):  # pylint: disable=unused-argument
        """
        Return pop() like %c in printf
        """

        try:
            value = self.stack.pop()
        except IndexError:
            value = NULL

        # Treat null as 0x80
        if value is NULL:
            value = 0x80

        self.rtn += b'%c' % value

    def _increment_one_two(self, group):  # pylint: disable=unused-argument
        """
        Add 1 to first two parameters
        Missing parameters are treated as 0's
        """
        for index in (0, 1):
            try:
                self.params[index] += 1
            except IndexError:
                self.params.append(1)

    def _binary_op(self, group):
        """
        Perform a binary operation on the last two items on the stack
        The order of evaluation is the order the items were placed on the stack
        """
        second_val = self.stack.pop()
        self.stack.append(OPERATORS[group](self.stack.pop(), second_val))

    def _unary_op(self, group):
        """
        Perform a unary operation on the last item on the stack
        """
        self.stack.append(OPERATORS[group](self.stack.pop()))

    def _push_param(self, group):
        """
        Push a parameter onto the stack
        If the parameter is missing, push Null
        """
        try:
            self.stack.append(self.params[int(group) - 1])
        except IndexError:
            self.stack.append(NULL)

    def _set_dynamic(self, group):
        """
        Set the a dynamic variable to pop()
        """
        self.dynamic[group] = self.stack.pop()

    def _get_dynamic(self, group):
        """
        Push the value of a dynamic variable onto the stack
        """
        self.stack.append(self.dynamic.get(group, NULL))

    def _set_static(self, group):
        """
        Set the a static variable to pop()
        """
        self.static[group] = self.stack.pop()

    def _get_static(self, group):
        """
        Push the value of a static variable onto the stack
        """
        self.stack.append(self.static.get(group, NULL))

    def _char_constant(self, group):
        """
        Push an character constant onto the stack
        """
        self.stack.append(ord(group))

    def _int_constant(self, group):
        """
        Push an integer constant onto the stack
        """
        self.stack.append(int(group))

    def _push_len(self, group):  # pylint: disable=unused-argument
        """
        Replace the last item on the stack with its length
        """
        self.stack.append(len(self.stack.pop()))

    def _cond_if(self, group):
        """
        Recursively evaluate the body of the if statement
        """
        self.parse(group)

    def _cond_then_else(self, group):
        """
        If the last item on the stack is True,
        recursively evaluate then statement

        Do not consume last item on stack
        """
        if self.stack[-1]:
            self.parse(group)

    def _cond_then_fi(self, group):
        """
        If the last item on the stack is True,
        recursively evaluate then statement

        Always consume last item on stack
        """
        if self.stack.pop():
            self.parse(group)

    def _cond_else(self, group):
        """
        If the last item on the stack is False,
        recursively evaluate the both of the else statement

        Always consume last item on stack
        """
        if not self.stack.pop():
            self.parse(group)

    def _cond_fi(self, group):  # pylint: disable=unused-argument
        """
        End if statement
        """

    def _printf(self, group):
        """
        Subset of printf-like formatting
        """

        # : is an escape to prevent flags from being treated as % operators, ignore
        # Python 2 returns as ':', Python 3 returns as 58
        if group[1] in (b':', 58):
            group = b'%' + group[2:]

        try:
            value = self.stack.pop()
        except IndexError:
            value = NULL

        # Treat null as empty string when string formatting
        # Python 2 returns as 's', Python 3 returns as 115
        if value is NULL and group[-1] in (b's', 115):
            value = b''

        self.rtn += group % value

    def _unmatched(self, group):  # pylint: disable=unused-argument
        """
        Escape pattern with no spec is skipped
        """

    def _literal(self, group):
        """
        Anything not prefaced with a known pattern spec is treated literally
        """
        self.rtn += group

    def parse(self, string):
        """
        Parsing loop
        Evaluate regex patterns in order until a pattern is matched
        """

        if not isinstance(string, bytes):
            raise TypeError("A bytes-like object is required, not '%s'" % type(string).__name__)

        index = 0
        length = len(string)

        while index < length:
            for filt, meth in PATTERNS:  # pragma: no branch
                match = re.match(filt, string[index:])
                if match:
                    group = match.groups()[-1] if match.groups() else match.group(0)
                    getattr(self, meth)(group)
                    index += match.end()
                    break

        return self.rtn

    def child(self, *params):
        """
        Return a new instance with the same variables, but different parameters
        """
        return self.__class__(*params, static=self.static, dynamic=self.dynamic)


tparm = TParm()  # pylint: disable=invalid-name
"""Reimplementation of :py:func:`curses.tparm`"""
