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

import sys

import unicodedata
from collections import defaultdict

from .compat import uchr, xrange

# http://stackoverflow.com/questions/14245893/efficiently-list-all-characters-in-a-given-unicode-category
U_CATEGORIES = defaultdict(list)
for c in map(uchr, xrange(sys.maxunicode + 1)):
    U_CATEGORIES[unicodedata.category(c)].append(c)
UNICODE_LETTER = set(
    U_CATEGORIES['Lu'] + U_CATEGORIES['Ll'] +
    U_CATEGORIES['Lt'] + U_CATEGORIES['Lm'] +
    U_CATEGORIES['Lo'] + U_CATEGORIES['Nl']
)
UNICODE_OTHER_ID_START = set((
    # Other_ID_Start
    '\u1885', '\u1886', '\u2118', '\u212E', '\u309B', '\u309C',
    # New in Unicode 8.0
    '\u08B3', '\u0AF9', '\u13F8', '\u9FCD', '\uAB60', '\U00010CC0', '\U000108E0', '\U0002B820',
    # New in Unicode 9.0
    '\u1C80', '\U000104DB', '\U0001E922',
    '\U0001EE00', '\U0001EE06', '\U0001EE0A',
))
UNICODE_OTHER_ID_CONTINUE = set((
    # Other_ID_Continue
    '\xB7', '\u0387', '\u1369', '\u136A', '\u136B', '\u136C',
    '\u136D', '\u136E', '\u136F', '\u1370', '\u1371', '\u19DA',
    # New in Unicode 8.0
    '\u08E3', '\uA69E', '\U00011730',
    # New in Unicode 9.0
    '\u08D4', '\u1DFB', '\uA8C5', '\U00011450',
    '\U0001EE03', '\U0001EE0B',
))
UNICODE_COMBINING_MARK = set(U_CATEGORIES['Mn'] + U_CATEGORIES['Mc'])
UNICODE_DIGIT = set(U_CATEGORIES['Nd'])
UNICODE_CONNECTOR_PUNCTUATION = set(U_CATEGORIES['Pc'])
IDENTIFIER_START = UNICODE_LETTER.union(UNICODE_OTHER_ID_START).union(set(('$', '_', '\\')))
IDENTIFIER_PART = IDENTIFIER_START.union(UNICODE_COMBINING_MARK).union(UNICODE_DIGIT).union(UNICODE_CONNECTOR_PUNCTUATION).union(set(('\u200D', '\u200C'))).union(UNICODE_OTHER_ID_CONTINUE)

WHITE_SPACE = set((
    '\x09', '\x0B', '\x0C', '\x20', '\xA0',
    '\u1680', '\u180E', '\u2000', '\u2001', '\u2002',
    '\u2003', '\u2004', '\u2005', '\u2006', '\u2007',
    '\u2008', '\u2009', '\u200A', '\u202F', '\u205F',
    '\u3000', '\uFEFF',
))
LINE_TERMINATOR = set(('\x0A', '\x0D', '\u2028', '\u2029'))

DECIMAL_CONV = dict((c, n) for n, c in enumerate('0123456789'))
OCTAL_CONV = dict((c, n) for n, c in enumerate('01234567'))
HEX_CONV = dict((c, n) for n, c in enumerate('0123456789abcdef'))
for n, c in enumerate('ABCDEF', 10):
    HEX_CONV[c] = n
DECIMAL_DIGIT = set(DECIMAL_CONV.keys())
OCTAL_DIGIT = set(OCTAL_CONV.keys())
HEX_DIGIT = set(HEX_CONV.keys())


class Character:
    @staticmethod
    def fromCodePoint(code):
        return uchr(code)

    # https://tc39.github.io/ecma262/#sec-white-space

    @staticmethod
    def isWhiteSpace(ch):
        return ch in WHITE_SPACE

    # https://tc39.github.io/ecma262/#sec-line-terminators

    @staticmethod
    def isLineTerminator(ch):
        return ch in LINE_TERMINATOR

    # https://tc39.github.io/ecma262/#sec-names-and-keywords

    @staticmethod
    def isIdentifierStart(ch):
        return ch in IDENTIFIER_START

    @staticmethod
    def isIdentifierPart(ch):
        return ch in IDENTIFIER_PART

    # https://tc39.github.io/ecma262/#sec-literals-numeric-literals

    @staticmethod
    def isDecimalDigit(ch):
        return ch in DECIMAL_DIGIT

    @staticmethod
    def isHexDigit(ch):
        return ch in HEX_DIGIT

    @staticmethod
    def isOctalDigit(ch):
        return ch in OCTAL_DIGIT
