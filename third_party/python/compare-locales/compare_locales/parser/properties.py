# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re

from .base import (
    Entity, OffsetComment, Whitespace,
    Parser
)
from six import unichr


class PropertiesEntityMixin(object):
    escape = re.compile(r'\\((?P<uni>u[0-9a-fA-F]{1,4})|'
                        '(?P<nl>\n[ \t]*)|(?P<single>.))', re.M)
    known_escapes = {'n': '\n', 'r': '\r', 't': '\t', '\\': '\\'}

    @property
    def val(self):
        def unescape(m):
            found = m.groupdict()
            if found['uni']:
                return unichr(int(found['uni'][1:], 16))
            if found['nl']:
                return ''
            return self.known_escapes.get(found['single'], found['single'])

        return self.escape.sub(unescape, self.raw_val)


class PropertiesEntity(PropertiesEntityMixin, Entity):
    pass


class PropertiesParser(Parser):

    Comment = OffsetComment

    def __init__(self):
        self.reKey = re.compile(
            '(?P<key>[^#! \t\r\n][^=:\n]*?)[ \t]*[:=][ \t]*', re.M)
        self.reComment = re.compile('(?:[#!][^\n]*\n)*(?:[#!][^\n]*)', re.M)
        self._escapedEnd = re.compile(r'\\+$')
        self._trailingWS = re.compile(r'[ \t\r\n]*(?:\n|\Z)', re.M)
        Parser.__init__(self)

    def getNext(self, ctx, offset):
        junk_offset = offset
        # overwritten to parse values line by line
        contents = ctx.contents

        m = self.reComment.match(contents, offset)
        if m:
            current_comment = self.Comment(ctx, m.span())
            if offset == 0 and 'License' in current_comment.val:
                # Heuristic. A early comment with "License" is probably
                # a license header, and should be standalone.
                return current_comment
            offset = m.end()
        else:
            current_comment = None

        m = self.reWhitespace.match(contents, offset)
        if m:
            white_space = Whitespace(ctx, m.span())
            offset = m.end()
            if (
                current_comment is not None
                and white_space.raw_val.count('\n') > 1
            ):
                # standalone comment
                return current_comment
            if current_comment is None:
                return white_space
        else:
            white_space = None

        m = self.reKey.match(contents, offset)
        if m:
            startline = offset = m.end()
            while True:
                endval = nextline = contents.find('\n', offset)
                if nextline == -1:
                    endval = offset = len(contents)
                    break
                # is newline escaped?
                _e = self._escapedEnd.search(contents, offset, nextline)
                offset = nextline + 1
                if _e is None:
                    break
                # backslashes at end of line, if 2*n, not escaped
                if len(_e.group()) % 2 == 0:
                    break
                startline = offset

            # strip trailing whitespace
            ws = self._trailingWS.search(contents, startline)
            if ws:
                endval = ws.start()

            entity = PropertiesEntity(
                ctx, current_comment, white_space,
                (m.start(), endval),   # full span
                m.span('key'),
                (m.end(), endval))   # value span
            return entity

        if current_comment is not None:
            return current_comment
        if white_space is not None:
            return white_space

        return self.getJunk(ctx, junk_offset, self.reKey, self.reComment)
