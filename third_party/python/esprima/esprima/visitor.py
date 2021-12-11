# -*- coding: utf-8 -*-
# Copyright JS Foundation and other contributors, https://js.foundation/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import unicode_literals

import json
import types
from collections import deque

from .objects import Object
from .compat import PY3, unicode


class VisitRecursionError(Exception):
    pass


class Visited(object):
    def __init__(self, result):
        if isinstance(result, Visited):
            result = result.result
        self.result = result


class Visitor(object):
    """
    An Object visitor base class that walks the abstract syntax tree and calls a
    visitor function for every Object found.  This function may return a value
    which is forwarded by the `visit` method.

    This class is meant to be subclassed, with the subclass adding visitor
    methods.

    Per default the visitor functions for the nodes are ``'visit_'`` +
    class name of the Object.  So a `Module` Object visit function would
    be `visit_Module`.  This behavior can be changed by overriding
    the `visit` method.  If no visitor function exists for a Object
    (return value `None`) the `generic_visit` visitor is used instead.
    """

    def __call__(self, obj, metadata):
        return self.transform(obj, metadata)

    def transform(self, obj, metadata):
        """Transform an Object."""
        if isinstance(obj, Object):
            method = 'transform_' + obj.__class__.__name__
            transformer = getattr(self, method, self.transform_Object)
            new_obj = transformer(obj, metadata)
            if new_obj is not None and obj is not new_obj:
                obj = new_obj
        return obj

    def transform_Object(self, obj, metadata):
        """Called if no explicit transform function exists for an Object."""
        return obj

    def generic_visit(self, obj):
        return self.visit(self.visit_Object(obj))

    def visit(self, obj):
        """Visit a Object."""
        if not hasattr(self, 'visitors'):
            self._visit_context = {}
            self._visit_count = 0
        try:
            self._visit_count += 1
            stack = deque()
            stack.append((obj, None))
            last_result = None
            while stack:
                try:
                    last, visited = stack[-1]
                    if isinstance(last, types.GeneratorType):
                        stack.append((last.send(last_result), None))
                        last_result = None
                    elif isinstance(last, Visited):
                        stack.pop()
                        last_result = last.result
                    elif isinstance(last, Object):
                        if last in self._visit_context:
                            if self._visit_context[last] == self.visit_Object:
                                visitor = self.visit_RecursionError
                            else:
                                visitor = self.visit_Object
                        else:
                            method = 'visit_' + last.__class__.__name__
                            visitor = getattr(self, method, self.visit_Object)
                        self._visit_context[last] = visitor
                        stack.pop()
                        stack.append((visitor(last), last))
                    else:
                        method = 'visit_' + last.__class__.__name__
                        visitor = getattr(self, method, self.visit_Generic)
                        stack.pop()
                        stack.append((visitor(last), None))
                except StopIteration:
                    stack.pop()
                    if visited and visited in self._visit_context:
                        del self._visit_context[visited]
            return last_result
        finally:
            self._visit_count -= 1
            if self._visit_count <= 0:
                self._visit_context = {}

    def visit_RecursionError(self, obj):
        raise VisitRecursionError

    def visit_Object(self, obj):
        """Called if no explicit visitor function exists for an Object."""
        yield obj.__dict__
        yield Visited(obj)

    def visit_Generic(self, obj):
        """Called if no explicit visitor function exists for an object."""
        yield Visited(obj)

    def visit_list(self, obj):
        for item in obj:
            yield item
        yield Visited(obj)

    visit_Array = visit_list

    def visit_dict(self, obj):
        for field, value in list(obj.items()):
            if not field.startswith('_'):
                yield value
        yield Visited(obj)


class NodeVisitor(Visitor):
    pass


class ReprVisitor(Visitor):
    def visit(self, obj, indent=4, nl="\n", sp="", skip=()):
        self.level = 0
        if isinstance(indent, int):
            indent = " " * indent
        self.indent = indent
        self.nl = nl
        self.sp = sp
        self.skip = skip
        return super(ReprVisitor, self).visit(obj)

    def visit_RecursionError(self, obj):
        yield Visited("...")

    def visit_Object(self, obj):
        value_repr = yield obj.__dict__
        yield Visited(value_repr)

    def visit_Generic(self, obj):
        yield Visited(repr(obj))

    def visit_list(self, obj):
        indent1 = self.indent * self.level
        indent2 = indent1 + self.indent
        self.level += 1
        try:
            items = []
            for item in obj:
                v = yield item
                items.append(v)
            if items:
                value_repr = "[%s%s%s%s%s%s%s]" % (
                    self.sp,
                    self.nl,
                    indent2,
                    (",%s%s%s" % (self.nl, self.sp, indent2)).join(items),
                    self.nl,
                    indent1,
                    self.sp,
                )
            else:
                value_repr = "[]"
        finally:
            self.level -= 1

        yield Visited(value_repr)

    visit_Array = visit_list

    def visit_dict(self, obj):
        indent1 = self.indent * self.level
        indent2 = indent1 + self.indent
        self.level += 1
        try:
            items = []
            for k, item in obj.items():
                if item is not None and not k.startswith('_') and k not in self.skip:
                    v = yield item
                    items.append("%s: %s" % (k, v))
            if items:
                value_repr = "{%s%s%s%s%s%s%s}" % (
                    self.sp,
                    self.nl,
                    indent2,
                    (",%s%s%s" % (self.nl, self.sp, indent2)).join(items),
                    self.nl,
                    indent1,
                    self.sp,
                )
            else:
                value_repr = "{}"
        finally:
            self.level -= 1

        yield Visited(value_repr)

    if PY3:
        def visit_str(self, obj):
            value_repr = json.dumps(obj)
            yield Visited(value_repr)
    else:
        def visit_unicode(self, obj):
            value_repr = json.dumps(obj)
            yield Visited(value_repr)

    def visit_SourceLocation(self, obj):
        old_indent, self.indent = self.indent, ""
        old_nl, self.nl = self.nl, ""
        old_sp, self.sp = self.sp, ""
        try:
            yield obj
        finally:
            self.indent = old_indent
            self.nl = old_nl
            self.sp = old_sp


class ToDictVisitor(Visitor):
    map = {
        'isAsync': 'async',
        'allowAwait': 'await',
    }

    def visit_RecursionError(self, obj):
        yield Visited({
            'error': "Infinite recursion detected...",
        })

    def visit_Object(self, obj):
        obj = yield obj.__dict__
        yield Visited(obj)

    def visit_list(self, obj):
        items = []
        for item in obj:
            v = yield item
            items.append(v)
        yield Visited(items)

    visit_Array = visit_list

    def visit_dict(self, obj):
        items = []
        for k, item in obj.items():
            if item is not None and not k.startswith('_'):
                v = yield item
                k = unicode(k)
                items.append((self.map.get(k, k), v))
        yield Visited(dict(items))

    def visit_SRE_Pattern(self, obj):
        yield Visited({})
