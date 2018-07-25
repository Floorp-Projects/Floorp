# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re

from .base import (
    CAN_COPY,
    EntityBase, OffsetComment, Junk, Whitespace,
    Parser
)


class DefinesInstruction(EntityBase):
    '''Entity-like object representing processing instructions in inc files
    '''
    def __init__(self, ctx, span, val_span):
        self.ctx = ctx
        self.span = span
        self.key_span = self.val_span = val_span

    def __repr__(self):
        return self.raw_val


class DefinesParser(Parser):
    # can't merge, #unfilter needs to be the last item, which we don't support
    capabilities = CAN_COPY
    reWhitespace = re.compile('\n+', re.M)

    EMPTY_LINES = 1 << 0
    PAST_FIRST_LINE = 1 << 1

    class Comment(OffsetComment):
        comment_offset = 2

    def __init__(self):
        self.reComment = re.compile('(?:^# .*?\n)*(?:^# [^\n]*)', re.M)
        # corresponds to
        # https://hg.mozilla.org/mozilla-central/file/72ee4800d4156931c89b58bd807af4a3083702bb/python/mozbuild/mozbuild/preprocessor.py#l561  # noqa
        self.reKey = re.compile(
            r'#define[ \t]+(?P<key>\w+)(?:[ \t](?P<val>[^\n]*))?', re.M)
        self.rePI = re.compile(r'#(?P<val>\w+[ \t]+[^\n]+)', re.M)
        Parser.__init__(self)

    def getNext(self, ctx, offset):
        contents = ctx.contents

        m = self.reWhitespace.match(contents, offset)
        if m:
            if ctx.state & self.EMPTY_LINES:
                return Whitespace(ctx, m.span())
            if ctx.state & self.PAST_FIRST_LINE and len(m.group()) == 1:
                return Whitespace(ctx, m.span())
            else:
                return Junk(ctx, m.span())

        # We're not in the first line anymore.
        ctx.state |= self.PAST_FIRST_LINE

        m = self.reComment.match(contents, offset)
        if m:
            self.last_comment = self.Comment(ctx, m.span())
            return self.last_comment
        m = self.reKey.match(contents, offset)
        if m:
            return self.createEntity(ctx, m)
        m = self.rePI.match(contents, offset)
        if m:
            instr = DefinesInstruction(ctx, m.span(), m.span('val'))
            if instr.val == 'filter emptyLines':
                ctx.state |= self.EMPTY_LINES
            if instr.val == 'unfilter emptyLines':
                ctx.state &= ~ self.EMPTY_LINES
            return instr
        return self.getJunk(
            ctx, offset, self.reComment, self.reKey, self.rePI)
