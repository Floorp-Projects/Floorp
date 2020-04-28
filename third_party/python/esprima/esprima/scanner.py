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

from __future__ import absolute_import, unicode_literals

import re

from .objects import Object
from .compat import xrange, unicode, uchr, uord
from .character import Character, HEX_CONV, OCTAL_CONV
from .messages import Messages
from .token import Token


def hexValue(ch):
    return HEX_CONV[ch]


def octalValue(ch):
    return OCTAL_CONV[ch]


class RegExp(Object):
    def __init__(self, pattern=None, flags=None):
        self.pattern = pattern
        self.flags = flags


class Position(Object):
    def __init__(self, line=None, column=None, offset=None):
        self.line = line
        self.column = column
        self.offset = offset


class SourceLocation(Object):
    def __init__(self, start=None, end=None, source=None):
        self.start = start
        self.end = end
        self.source = source


class Comment(Object):
    def __init__(self, multiLine=None, slice=None, range=None, loc=None):
        self.multiLine = multiLine
        self.slice = slice
        self.range = range
        self.loc = loc


class RawToken(Object):
    def __init__(self, type=None, value=None, pattern=None, flags=None, regex=None, octal=None, cooked=None, head=None, tail=None, lineNumber=None, lineStart=None, start=None, end=None):
        self.type = type
        self.value = value
        self.pattern = pattern
        self.flags = flags
        self.regex = regex
        self.octal = octal
        self.cooked = cooked
        self.head = head
        self.tail = tail
        self.lineNumber = lineNumber
        self.lineStart = lineStart
        self.start = start
        self.end = end


class ScannerState(Object):
    def __init__(self, index=None, lineNumber=None, lineStart=None):
        self.index = index
        self.lineNumber = lineNumber
        self.lineStart = lineStart


class Octal(object):
    def __init__(self, octal, code):
        self.octal = octal
        self.code = code


class Scanner(object):
    def __init__(self, code, handler):
        self.source = unicode(code) + '\x00'
        self.errorHandler = handler
        self.trackComment = False
        self.isModule = False

        self.length = len(code)
        self.index = 0
        self.lineNumber = 1 if self.length > 0 else 0
        self.lineStart = 0
        self.curlyStack = []

    def saveState(self):
        return ScannerState(
            index=self.index,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart
        )

    def restoreState(self, state):
        self.index = state.index
        self.lineNumber = state.lineNumber
        self.lineStart = state.lineStart

    def eof(self):
        return self.index >= self.length

    def throwUnexpectedToken(self, message=Messages.UnexpectedTokenIllegal):
        return self.errorHandler.throwError(self.index, self.lineNumber,
            self.index - self.lineStart + 1, message)

    def tolerateUnexpectedToken(self, message=Messages.UnexpectedTokenIllegal):
        self.errorHandler.tolerateError(self.index, self.lineNumber,
            self.index - self.lineStart + 1, message)

    # https://tc39.github.io/ecma262/#sec-comments

    def skipSingleLineComment(self, offset):
        comments = []

        if self.trackComment:
            start = self.index - offset
            loc = SourceLocation(
                start=Position(
                    line=self.lineNumber,
                    column=self.index - self.lineStart - offset
                ),
                end=Position()
            )

        while not self.eof():
            ch = self.source[self.index]
            self.index += 1
            if Character.isLineTerminator(ch):
                if self.trackComment:
                    loc.end = Position(
                        line=self.lineNumber,
                        column=self.index - self.lineStart - 1
                    )
                    entry = Comment(
                        multiLine=False,
                        slice=[start + offset, self.index - 1],
                        range=[start, self.index - 1],
                        loc=loc
                    )
                    comments.append(entry)

                if ch == '\r' and self.source[self.index] == '\n':
                    self.index += 1

                self.lineNumber += 1
                self.lineStart = self.index
                return comments

        if self.trackComment:
            loc.end = Position(
                line=self.lineNumber,
                column=self.index - self.lineStart
            )
            entry = Comment(
                multiLine=False,
                slice=[start + offset, self.index],
                range=[start, self.index],
                loc=loc
            )
            comments.append(entry)

        return comments

    def skipMultiLineComment(self):
        comments = []

        if self.trackComment:
            comments = []
            start = self.index - 2
            loc = SourceLocation(
                start=Position(
                    line=self.lineNumber,
                    column=self.index - self.lineStart - 2
                ),
                end=Position()
            )

        while not self.eof():
            ch = self.source[self.index]
            if Character.isLineTerminator(ch):
                if ch == '\r' and self.source[self.index + 1] == '\n':
                    self.index += 1

                self.lineNumber += 1
                self.index += 1
                self.lineStart = self.index
            elif ch == '*':
                # Block comment ends with '*/'.
                if self.source[self.index + 1] == '/':
                    self.index += 2
                    if self.trackComment:
                        loc.end = Position(
                            line=self.lineNumber,
                            column=self.index - self.lineStart
                        )
                        entry = Comment(
                            multiLine=True,
                            slice=[start + 2, self.index - 2],
                            range=[start, self.index],
                            loc=loc
                        )
                        comments.append(entry)

                    return comments

                self.index += 1
            else:
                self.index += 1

        # Ran off the end of the file - the whole thing is a comment
        if self.trackComment:
            loc.end = Position(
                line=self.lineNumber,
                column=self.index - self.lineStart
            )
            entry = Comment(
                multiLine=True,
                slice=[start + 2, self.index],
                range=[start, self.index],
                loc=loc
            )
            comments.append(entry)

        self.tolerateUnexpectedToken()
        return comments

    def scanComments(self):
        comments = []

        start = self.index == 0
        while not self.eof():
            ch = self.source[self.index]

            if Character.isWhiteSpace(ch):
                self.index += 1
            elif Character.isLineTerminator(ch):
                self.index += 1
                if ch == '\r' and self.source[self.index] == '\n':
                    self.index += 1

                self.lineNumber += 1
                self.lineStart = self.index
                start = True
            elif ch == '/':  # U+002F is '/'
                ch = self.source[self.index + 1]
                if ch == '/':
                    self.index += 2
                    comment = self.skipSingleLineComment(2)
                    if self.trackComment:
                        comments.extend(comment)

                    start = True
                elif ch == '*':  # U+002A is '*'
                    self.index += 2
                    comment = self.skipMultiLineComment()
                    if self.trackComment:
                        comments.extend(comment)

                else:
                    break

            elif start and ch == '-':  # U+002D is '-'
                # U+003E is '>'
                if self.source[self.index + 1:self.index + 3] == '->':
                    # '-->' is a single-line comment
                    self.index += 3
                    comment = self.skipSingleLineComment(3)
                    if self.trackComment:
                        comments.extend(comment)

                else:
                    break

            elif ch == '<' and not self.isModule:  # U+003C is '<'
                if self.source[self.index + 1:self.index + 4] == '!--':
                    self.index += 4  # `<!--`
                    comment = self.skipSingleLineComment(4)
                    if self.trackComment:
                        comments.extend(comment)

                else:
                    break

            else:
                break

        return comments

    # https://tc39.github.io/ecma262/#sec-future-reserved-words

    def isFutureReservedWord(self, id):
        return id in self.isFutureReservedWord.set
    isFutureReservedWord.set = set((
        'enum',
        'export',
        'import',
        'super',
    ))

    def isStrictModeReservedWord(self, id):
        return id in self.isStrictModeReservedWord.set
    isStrictModeReservedWord.set = set((
        'implements',
        'interface',
        'package',
        'private',
        'protected',
        'public',
        'static',
        'yield',
        'let',
    ))

    def isRestrictedWord(self, id):
        return id in self.isRestrictedWord.set
    isRestrictedWord.set = set((
        'eval', 'arguments',
    ))

    # https://tc39.github.io/ecma262/#sec-keywords

    def isKeyword(self, id):
        return id in self.isKeyword.set
    isKeyword.set = set((
        'if', 'in', 'do',

        'var', 'for', 'new',
        'try', 'let',

        'this', 'else', 'case',
        'void', 'with', 'enum',

        'while', 'break', 'catch',
        'throw', 'const', 'yield',
        'class', 'super',

        'return', 'typeof', 'delete',
        'switch', 'export', 'import',

        'default', 'finally', 'extends',

        'function', 'continue', 'debugger',

        'instanceof',
    ))

    def codePointAt(self, i):
        return uord(self.source[i:i + 2])

    def scanHexEscape(self, prefix):
        length = 4 if prefix == 'u' else 2
        code = 0

        for i in xrange(length):
            if not self.eof() and Character.isHexDigit(self.source[self.index]):
                ch = self.source[self.index]
                self.index += 1
                code = code * 16 + hexValue(ch)
            else:
                return None

        return uchr(code)

    def scanUnicodeCodePointEscape(self):
        ch = self.source[self.index]
        code = 0

        # At least, one hex digit is required.
        if ch == '}':
            self.throwUnexpectedToken()

        while not self.eof():
            ch = self.source[self.index]
            self.index += 1
            if not Character.isHexDigit(ch):
                break

            code = code * 16 + hexValue(ch)

        if code > 0x10FFFF or ch != '}':
            self.throwUnexpectedToken()

        return Character.fromCodePoint(code)

    def getIdentifier(self):
        start = self.index
        self.index += 1
        while not self.eof():
            ch = self.source[self.index]
            if ch == '\\':
                # Blackslash (U+005C) marks Unicode escape sequence.
                self.index = start
                return self.getComplexIdentifier()
            else:
                cp = ord(ch)
                if cp >= 0xD800 and cp < 0xDFFF:
                    # Need to handle surrogate pairs.
                    self.index = start
                    return self.getComplexIdentifier()

            if Character.isIdentifierPart(ch):
                self.index += 1
            else:
                break

        return self.source[start:self.index]

    def getComplexIdentifier(self):
        cp = self.codePointAt(self.index)
        id = Character.fromCodePoint(cp)
        self.index += len(id)

        # '\u' (U+005C, U+0075) denotes an escaped character.
        if cp == 0x5C:
            if self.source[self.index] != 'u':
                self.throwUnexpectedToken()

            self.index += 1
            if self.source[self.index] == '{':
                self.index += 1
                ch = self.scanUnicodeCodePointEscape()
            else:
                ch = self.scanHexEscape('u')
                if not ch or ch == '\\' or not Character.isIdentifierStart(ch[0]):
                    self.throwUnexpectedToken()

            id = ch

        while not self.eof():
            cp = self.codePointAt(self.index)
            ch = Character.fromCodePoint(cp)
            if not Character.isIdentifierPart(ch):
                break

            id += ch
            self.index += len(ch)

            # '\u' (U+005C, U+0075) denotes an escaped character.
            if cp == 0x5C:
                id = id[:-1]
                if self.source[self.index] != 'u':
                    self.throwUnexpectedToken()

                self.index += 1
                if self.source[self.index] == '{':
                    self.index += 1
                    ch = self.scanUnicodeCodePointEscape()
                else:
                    ch = self.scanHexEscape('u')
                    if not ch or ch == '\\' or not Character.isIdentifierPart(ch[0]):
                        self.throwUnexpectedToken()

                id += ch

        return id

    def octalToDecimal(self, ch):
        # \0 is not octal escape sequence
        octal = ch != '0'
        code = octalValue(ch)

        if not self.eof() and Character.isOctalDigit(self.source[self.index]):
            octal = True
            code = code * 8 + octalValue(self.source[self.index])
            self.index += 1

            # 3 digits are only allowed when string starts
            # with 0, 1, 2, 3
            if ch in '0123' and not self.eof() and Character.isOctalDigit(self.source[self.index]):
                code = code * 8 + octalValue(self.source[self.index])
                self.index += 1

        return Octal(octal, code)

    # https://tc39.github.io/ecma262/#sec-names-and-keywords

    def scanIdentifier(self):
        start = self.index

        # Backslash (U+005C) starts an escaped character.
        id = self.getComplexIdentifier() if self.source[start] == '\\' else self.getIdentifier()

        # There is no keyword or literal with only one character.
        # Thus, it must be an identifier.
        if len(id) == 1:
            type = Token.Identifier
        elif self.isKeyword(id):
            type = Token.Keyword
        elif id == 'null':
            type = Token.NullLiteral
        elif id == 'true' or id == 'false':
            type = Token.BooleanLiteral
        else:
            type = Token.Identifier

        if type is not Token.Identifier and start + len(id) != self.index:
            restore = self.index
            self.index = start
            self.tolerateUnexpectedToken(Messages.InvalidEscapedReservedWord)
            self.index = restore

        return RawToken(
            type=type,
            value=id,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    # https://tc39.github.io/ecma262/#sec-punctuators

    def scanPunctuator(self):
        start = self.index

        # Check for most common single-character punctuators.
        str = self.source[self.index]
        if str in (
            '(',
            '{',
        ):
            if str == '{':
                self.curlyStack.append('{')

            self.index += 1

        elif str == '.':
            self.index += 1
            if self.source[self.index] == '.' and self.source[self.index + 1] == '.':
                # Spread operator: ...
                self.index += 2
                str = '...'

        elif str == '}':
            self.index += 1
            if self.curlyStack:
                self.curlyStack.pop()

        elif str in (
            ')',
            ';',
            ',',
            '[',
            ']',
            ':',
            '?',
            '~',
        ):
            self.index += 1

        else:
            # 4-character punctuator.
            str = self.source[self.index:self.index + 4]
            if str == '>>>=':
                self.index += 4
            else:

                # 3-character punctuators.
                str = str[:3]
                if str in (
                    '===', '!==', '>>>',
                    '<<=', '>>=', '**='
                ):
                    self.index += 3
                else:

                    # 2-character punctuators.
                    str = str[:2]
                    if str in (
                        '&&', '||', '==', '!=',
                        '+=', '-=', '*=', '/=',
                        '++', '--', '<<', '>>',
                        '&=', '|=', '^=', '%=',
                        '<=', '>=', '=>', '**',
                    ):
                        self.index += 2
                    else:

                        # 1-character punctuators.
                        str = self.source[self.index]
                        if str in '<>=!+-*%&|^/':
                            self.index += 1

        if self.index == start:
            self.throwUnexpectedToken()

        return RawToken(
            type=Token.Punctuator,
            value=str,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    # https://tc39.github.io/ecma262/#sec-literals-numeric-literals

    def scanHexLiteral(self, start):
        num = ''

        while not self.eof():
            if not Character.isHexDigit(self.source[self.index]):
                break

            num += self.source[self.index]
            self.index += 1

        if len(num) == 0:
            self.throwUnexpectedToken()

        if Character.isIdentifierStart(self.source[self.index]):
            self.throwUnexpectedToken()

        return RawToken(
            type=Token.NumericLiteral,
            value=int(num, 16),
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    def scanBinaryLiteral(self, start):
        num = ''

        while not self.eof():
            ch = self.source[self.index]
            if ch != '0' and ch != '1':
                break

            num += self.source[self.index]
            self.index += 1

        if len(num) == 0:
            # only 0b or 0B
            self.throwUnexpectedToken()

        if not self.eof():
            ch = self.source[self.index]
            if Character.isIdentifierStart(ch) or Character.isDecimalDigit(ch):
                self.throwUnexpectedToken()

        return RawToken(
            type=Token.NumericLiteral,
            value=int(num, 2),
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    def scanOctalLiteral(self, prefix, start):
        num = ''
        octal = False

        if Character.isOctalDigit(prefix[0]):
            octal = True
            num = '0' + self.source[self.index]
        self.index += 1

        while not self.eof():
            if not Character.isOctalDigit(self.source[self.index]):
                break

            num += self.source[self.index]
            self.index += 1

        if not octal and len(num) == 0:
            # only 0o or 0O
            self.throwUnexpectedToken()

        if Character.isIdentifierStart(self.source[self.index]) or Character.isDecimalDigit(self.source[self.index]):
            self.throwUnexpectedToken()

        return RawToken(
            type=Token.NumericLiteral,
            value=int(num, 8),
            octal=octal,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    def isImplicitOctalLiteral(self):
        # Implicit octal, unless there is a non-octal digit.
        # (Annex B.1.1 on Numeric Literals)
        for i in xrange(self.index + 1, self.length):
            ch = self.source[i]
            if ch in '89':
                return False
            if not Character.isOctalDigit(ch):
                return True
        return True

    def scanNumericLiteral(self):
        start = self.index
        ch = self.source[start]
        assert Character.isDecimalDigit(ch) or ch == '.', 'Numeric literal must start with a decimal digit or a decimal point'

        num = ''
        if ch != '.':
            num = self.source[self.index]
            self.index += 1
            ch = self.source[self.index]

            # Hex number starts with '0x'.
            # Octal number starts with '0'.
            # Octal number in ES6 starts with '0o'.
            # Binary number in ES6 starts with '0b'.
            if num == '0':
                if ch in ('x', 'X'):
                    self.index += 1
                    return self.scanHexLiteral(start)

                if ch in ('b', 'B'):
                    self.index += 1
                    return self.scanBinaryLiteral(start)

                if ch in ('o', 'O'):
                    return self.scanOctalLiteral(ch, start)

                if ch and Character.isOctalDigit(ch):
                    if self.isImplicitOctalLiteral():
                        return self.scanOctalLiteral(ch, start)

            while Character.isDecimalDigit(self.source[self.index]):
                num += self.source[self.index]
                self.index += 1

            ch = self.source[self.index]

        if ch == '.':
            num += self.source[self.index]
            self.index += 1
            while Character.isDecimalDigit(self.source[self.index]):
                num += self.source[self.index]
                self.index += 1

            ch = self.source[self.index]

        if ch in ('e', 'E'):
            num += self.source[self.index]
            self.index += 1

            ch = self.source[self.index]
            if ch in ('+', '-'):
                num += self.source[self.index]
                self.index += 1

            if Character.isDecimalDigit(self.source[self.index]):
                while Character.isDecimalDigit(self.source[self.index]):
                    num += self.source[self.index]
                    self.index += 1

            else:
                self.throwUnexpectedToken()

        if Character.isIdentifierStart(self.source[self.index]):
            self.throwUnexpectedToken()

        value = float(num)
        return RawToken(
            type=Token.NumericLiteral,
            value=int(value) if value.is_integer() else value,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    # https://tc39.github.io/ecma262/#sec-literals-string-literals

    def scanStringLiteral(self):
        start = self.index
        quote = self.source[start]
        assert quote in ('\'', '"'), 'String literal must starts with a quote'

        self.index += 1
        octal = False
        str = ''

        while not self.eof():
            ch = self.source[self.index]
            self.index += 1

            if ch == quote:
                quote = ''
                break
            elif ch == '\\':
                ch = self.source[self.index]
                self.index += 1
                if not ch or not Character.isLineTerminator(ch):
                    if ch == 'u':
                        if self.source[self.index] == '{':
                            self.index += 1
                            str += self.scanUnicodeCodePointEscape()
                        else:
                            unescapedChar = self.scanHexEscape(ch)
                            if not unescapedChar:
                                self.throwUnexpectedToken()

                            str += unescapedChar

                    elif ch == 'x':
                        unescaped = self.scanHexEscape(ch)
                        if not unescaped:
                            self.throwUnexpectedToken(Messages.InvalidHexEscapeSequence)

                        str += unescaped
                    elif ch == 'n':
                        str += '\n'
                    elif ch == 'r':
                        str += '\r'
                    elif ch == 't':
                        str += '\t'
                    elif ch == 'b':
                        str += '\b'
                    elif ch == 'f':
                        str += '\f'
                    elif ch == 'v':
                        str += '\x0B'
                    elif ch in (
                        '8',
                        '9',
                    ):
                        str += ch
                        self.tolerateUnexpectedToken()

                    else:
                        if ch and Character.isOctalDigit(ch):
                            octToDec = self.octalToDecimal(ch)

                            octal = octToDec.octal or octal
                            str += uchr(octToDec.code)
                        else:
                            str += ch

                else:
                    self.lineNumber += 1
                    if ch == '\r' and self.source[self.index] == '\n':
                        self.index += 1

                    self.lineStart = self.index

            elif Character.isLineTerminator(ch):
                break
            else:
                str += ch

        if quote != '':
            self.index = start
            self.throwUnexpectedToken()

        return RawToken(
            type=Token.StringLiteral,
            value=str,
            octal=octal,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    # https://tc39.github.io/ecma262/#sec-template-literal-lexical-components

    def scanTemplate(self):
        cooked = ''
        terminated = False
        start = self.index

        head = self.source[start] == '`'
        tail = False
        rawOffset = 2

        self.index += 1

        while not self.eof():
            ch = self.source[self.index]
            self.index += 1
            if ch == '`':
                rawOffset = 1
                tail = True
                terminated = True
                break
            elif ch == '$':
                if self.source[self.index] == '{':
                    self.curlyStack.append('${')
                    self.index += 1
                    terminated = True
                    break

                cooked += ch
            elif ch == '\\':
                ch = self.source[self.index]
                self.index += 1
                if not Character.isLineTerminator(ch):
                    if ch == 'n':
                        cooked += '\n'
                    elif ch == 'r':
                        cooked += '\r'
                    elif ch == 't':
                        cooked += '\t'
                    elif ch == 'u':
                        if self.source[self.index] == '{':
                            self.index += 1
                            cooked += self.scanUnicodeCodePointEscape()
                        else:
                            restore = self.index
                            unescapedChar = self.scanHexEscape(ch)
                            if unescapedChar:
                                cooked += unescapedChar
                            else:
                                self.index = restore
                                cooked += ch

                    elif ch == 'x':
                        unescaped = self.scanHexEscape(ch)
                        if not unescaped:
                            self.throwUnexpectedToken(Messages.InvalidHexEscapeSequence)

                        cooked += unescaped
                    elif ch == 'b':
                        cooked += '\b'
                    elif ch == 'f':
                        cooked += '\f'
                    elif ch == 'v':
                        cooked += '\v'

                    else:
                        if ch == '0':
                            if Character.isDecimalDigit(self.source[self.index]):
                                # Illegal: \01 \02 and so on
                                self.throwUnexpectedToken(Messages.TemplateOctalLiteral)

                            cooked += '\0'
                        elif Character.isOctalDigit(ch):
                            # Illegal: \1 \2
                            self.throwUnexpectedToken(Messages.TemplateOctalLiteral)
                        else:
                            cooked += ch

                else:
                    self.lineNumber += 1
                    if ch == '\r' and self.source[self.index] == '\n':
                        self.index += 1

                    self.lineStart = self.index

            elif Character.isLineTerminator(ch):
                self.lineNumber += 1
                if ch == '\r' and self.source[self.index] == '\n':
                    self.index += 1

                self.lineStart = self.index
                cooked += '\n'
            else:
                cooked += ch

        if not terminated:
            self.throwUnexpectedToken()

        if not head:
            if self.curlyStack:
                self.curlyStack.pop()

        return RawToken(
            type=Token.Template,
            value=self.source[start + 1:self.index - rawOffset],
            cooked=cooked,
            head=head,
            tail=tail,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    # https://tc39.github.io/ecma262/#sec-literals-regular-expression-literals

    def testRegExp(self, pattern, flags):
        # The BMP character to use as a replacement for astral symbols when
        # translating an ES6 "u"-flagged pattern to an ES5-compatible
        # approximation.
        # Note: replacing with '\uFFFF' enables false positives in unlikely
        # scenarios. For example, `[\u{1044f}-\u{10440}]` is an invalid
        # pattern that would not be detected by this substitution.
        astralSubstitute = '\uFFFF'

        # Replace every Unicode escape sequence with the equivalent
        # BMP character or a constant ASCII code point in the case of
        # astral symbols. (See the above note on `astralSubstitute`
        # for more information.)
        def astralSub(m):
            codePoint = int(m.group(1) or m.group(2), 16)
            if codePoint > 0x10FFFF:
                self.tolerateUnexpectedToken(Messages.InvalidRegExp)
            elif codePoint <= 0xFFFF:
                return uchr(codePoint)
            return astralSubstitute
        pattern = re.sub(r'\\u\{([0-9a-fA-F]+)\}|\\u([a-fA-F0-9]{4})', astralSub, pattern)

        # Replace each paired surrogate with a single ASCII symbol to
        # avoid throwing on regular expressions that are only valid in
        # combination with the "u" flag.
        pattern = re.sub(r'[\uD800-\uDBFF][\uDC00-\uDFFF]', astralSubstitute, pattern)

        # Return a regular expression object for this pattern-flag pair, or
        # `null` in case the current environment doesn't support the flags it
        # uses.
        pyflags = 0 | re.M if 'm' in flags else 0 | re.I if 'i' in flags else 0
        try:
            return re.compile(pattern, pyflags)
        except Exception:
            self.tolerateUnexpectedToken(Messages.InvalidRegExp)

    def scanRegExpBody(self):
        ch = self.source[self.index]
        assert ch == '/', 'Regular expression literal must start with a slash'

        str = self.source[self.index]
        self.index += 1
        classMarker = False
        terminated = False

        while not self.eof():
            ch = self.source[self.index]
            self.index += 1
            str += ch
            if ch == '\\':
                ch = self.source[self.index]
                self.index += 1
                # https://tc39.github.io/ecma262/#sec-literals-regular-expression-literals
                if Character.isLineTerminator(ch):
                    self.throwUnexpectedToken(Messages.UnterminatedRegExp)

                str += ch
            elif Character.isLineTerminator(ch):
                self.throwUnexpectedToken(Messages.UnterminatedRegExp)
            elif classMarker:
                if ch == ']':
                    classMarker = False

            else:
                if ch == '/':
                    terminated = True
                    break
                elif ch == '[':
                    classMarker = True

        if not terminated:
            self.throwUnexpectedToken(Messages.UnterminatedRegExp)

        # Exclude leading and trailing slash.
        return str[1:-1]

    def scanRegExpFlags(self):
        str = ''
        flags = ''
        while not self.eof():
            ch = self.source[self.index]
            if not Character.isIdentifierPart(ch):
                break

            self.index += 1
            if ch == '\\' and not self.eof():
                ch = self.source[self.index]
                if ch == 'u':
                    self.index += 1
                    restore = self.index
                    char = self.scanHexEscape('u')
                    if char:
                        flags += char
                        str += '\\u'
                        while restore < self.index:
                            str += self.source[restore]
                            restore += 1

                    else:
                        self.index = restore
                        flags += 'u'
                        str += '\\u'

                    self.tolerateUnexpectedToken()
                else:
                    str += '\\'
                    self.tolerateUnexpectedToken()

            else:
                flags += ch
                str += ch

        return flags

    def scanRegExp(self):
        start = self.index

        pattern = self.scanRegExpBody()
        flags = self.scanRegExpFlags()
        value = self.testRegExp(pattern, flags)

        return RawToken(
            type=Token.RegularExpression,
            value='',
            pattern=pattern,
            flags=flags,
            regex=value,
            lineNumber=self.lineNumber,
            lineStart=self.lineStart,
            start=start,
            end=self.index
        )

    def lex(self):
        if self.eof():
            return RawToken(
                type=Token.EOF,
                value='',
                lineNumber=self.lineNumber,
                lineStart=self.lineStart,
                start=self.index,
                end=self.index
            )

        ch = self.source[self.index]

        if Character.isIdentifierStart(ch):
            return self.scanIdentifier()

        # Very common: ( and ) and ;
        if ch in ('(', ')', ';'):
            return self.scanPunctuator()

        # String literal starts with single quote (U+0027) or double quote (U+0022).
        if ch in ('\'', '"'):
            return self.scanStringLiteral()

        # Dot (.) U+002E can also start a floating-point number, hence the need
        # to check the next character.
        if ch == '.':
            if Character.isDecimalDigit(self.source[self.index + 1]):
                return self.scanNumericLiteral()

            return self.scanPunctuator()

        if Character.isDecimalDigit(ch):
            return self.scanNumericLiteral()

        # Template literals start with ` (U+0060) for template head
        # or } (U+007D) for template middle or template tail.
        if ch == '`' or (ch == '}' and self.curlyStack and self.curlyStack[-1] == '${'):
            return self.scanTemplate()

        # Possible identifier start in a surrogate pair.
        cp = ord(ch)
        if cp >= 0xD800 and cp < 0xDFFF:
            cp = self.codePointAt(self.index)
            ch = Character.fromCodePoint(cp)
            if Character.isIdentifierStart(ch):
                return self.scanIdentifier()

        return self.scanPunctuator()
