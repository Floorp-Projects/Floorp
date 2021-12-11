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

PY3 = sys.version_info >= (3, 0)

if PY3:
    # Python 3:
    basestring = str
    long = int
    xrange = range
    unicode = str
    uchr = chr

    def uord(ch):
        return ord(ch[0])

else:
    basestring = basestring
    long = long
    xrange = xrange
    unicode = unicode

    try:
        # Python 2 UCS4:
        unichr(0x10000)
        uchr = unichr

        def uord(ch):
            return ord(ch[0])

    except ValueError:
        # Python 2 UCS2:
        def uchr(code):
            # UTF-16 Encoding
            if code <= 0xFFFF:
                return unichr(code)
            cu1 = ((code - 0x10000) >> 10) + 0xD800
            cu2 = ((code - 0x10000) & 1023) + 0xDC00
            return unichr(cu1) + unichr(cu2)

        def uord(ch):
            cp = ord(ch[0])
            if cp >= 0xD800 and cp <= 0xDBFF:
                second = ord(ch[1])
                if second >= 0xDC00 and second <= 0xDFFF:
                    first = cp
                    cp = (first - 0xD800) * 0x400 + second - 0xDC00 + 0x10000
            return cp
