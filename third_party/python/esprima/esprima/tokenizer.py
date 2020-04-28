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
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES
# LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import absolute_import, unicode_literals

from collections import deque

from .objects import Object
from .error_handler import ErrorHandler
from .scanner import Scanner, SourceLocation, Position, RegExp
from .token import Token, TokenName


class BufferEntry(Object):
    def __init__(self, type, value, regex=None, range=None, loc=None):
        self.type = type
        self.value = value
        self.regex = regex
        self.range = range
        self.loc = loc


class Reader(object):
    def __init__(self):
        self.values = []
        self.curly = self.paren = -1

    # A function following one of those tokens is an expression.
    def beforeFunctionExpression(self, t):
        return t in (
            '(', '{', '[', 'in', 'typeof', 'instanceof', 'new',
            'return', 'case', 'delete', 'throw', 'void',
            # assignment operators
            '=', '+=', '-=', '*=', '**=', '/=', '%=', '<<=', '>>=', '>>>=',
            '&=', '|=', '^=', ',',
            # binary/unary operators
            '+', '-', '*', '**', '/', '%', '++', '--', '<<', '>>', '>>>', '&',
            '|', '^', '!', '~', '&&', '||', '?', ':', '===', '==', '>=',
            '<=', '<', '>', '!=', '!=='
        )

    # Determine if forward slash (/) is an operator or part of a regular expression
    # https://github.com/mozilla/sweet.js/wiki/design
    def isRegexStart(self):
        if not self.values:
            return True

        previous = self.values[-1]
        regex = previous is not None

        if previous in (
            'this',
            ']',
        ):
            regex = False
        elif previous == ')':
            keyword = self.values[self.paren - 1]
            regex = keyword in ('if', 'while', 'for', 'with')

        elif previous == '}':
            # Dividing a function by anything makes little sense,
            # but we have to check for that.
            regex = True
            if len(self.values) >= 3 and self.values[self.curly - 3] == 'function':
                # Anonymous function, e.g. function(){} /42
                check = self.values[self.curly - 4]
                regex = not self.beforeFunctionExpression(check) if check else False
            elif len(self.values) >= 4 and self.values[self.curly - 4] == 'function':
                # Named function, e.g. function f(){} /42/
                check = self.values[self.curly - 5]
                regex = not self.beforeFunctionExpression(check) if check else True

        return regex

    def append(self, token):
        if token.type in (Token.Punctuator, Token.Keyword):
            if token.value == '{':
                self.curly = len(self.values)
            elif token.value == '(':
                self.paren = len(self.values)
            self.values.append(token.value)
        else:
            self.values.append(None)


class Config(Object):
    def __init__(self, tolerant=None, comment=None, range=None, loc=None, **options):
        self.tolerant = tolerant
        self.comment = comment
        self.range = range
        self.loc = loc
        for k, v in options.items():
            setattr(self, k, v)


class Tokenizer(object):
    def __init__(self, code, options):
        self.config = Config(**options)

        self.errorHandler = ErrorHandler()
        self.errorHandler.tolerant = self.config.tolerant
        self.scanner = Scanner(code, self.errorHandler)
        self.scanner.trackComment = self.config.comment

        self.trackRange = self.config.range
        self.trackLoc = self.config.loc
        self.buffer = deque()
        self.reader = Reader()

    def errors(self):
        return self.errorHandler.errors

    def getNextToken(self):
        if not self.buffer:

            comments = self.scanner.scanComments()
            if self.scanner.trackComment:
                for e in comments:
                    value = self.scanner.source[e.slice[0]:e.slice[1]]
                    comment = BufferEntry(
                        type='BlockComment' if e.multiLine else 'LineComment',
                        value=value
                    )
                    if self.trackRange:
                        comment.range = e.range
                    if self.trackLoc:
                        comment.loc = e.loc
                    self.buffer.append(comment)

            if not self.scanner.eof():
                if self.trackLoc:
                    loc = SourceLocation(
                        start=Position(
                            line=self.scanner.lineNumber,
                            column=self.scanner.index - self.scanner.lineStart
                        ),
                        end=Position(),
                    )

                maybeRegex = self.scanner.source[self.scanner.index] == '/' and self.reader.isRegexStart()
                if maybeRegex:
                    state = self.scanner.saveState()
                    try:
                        token = self.scanner.scanRegExp()
                    except Exception:
                        self.scanner.restoreState(state)
                        token = self.scanner.lex()
                else:
                    token = self.scanner.lex()

                self.reader.append(token)

                entry = BufferEntry(
                    type=TokenName[token.type],
                    value=self.scanner.source[token.start:token.end]
                )
                if self.trackRange:
                    entry.range = [token.start, token.end]
                if self.trackLoc:
                    loc.end = Position(
                        line=self.scanner.lineNumber,
                        column=self.scanner.index - self.scanner.lineStart
                    )
                    entry.loc = loc
                if token.type is Token.RegularExpression:
                    entry.regex = RegExp(
                        pattern=token.pattern,
                        flags=token.flags,
                    )

                self.buffer.append(entry)

        return self.buffer.popleft() if self.buffer else None
