# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re

from .base import (
    CAN_COPY,
    Entry, OffsetComment, Junk, Whitespace,
    Parser
)


class DefinesInstruction(Entry):
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

    class Comment(OffsetComment):
        comment_offset = 2

    class Context(Parser.Context):
        def __init__(self, contents):
            super(DefinesParser.Context, self).__init__(contents)
            self.filter_empty_lines = False

    def __init__(self):
        self.reComment = re.compile('(?:^# .*?\n)*(?:^# [^\n]*)', re.M)
        # corresponds to
        # https://hg.mozilla.org/mozilla-central/file/72ee4800d4156931c89b58bd807af4a3083702bb/python/mozbuild/mozbuild/preprocessor.py#l561  # noqa
        self.reKey = re.compile(
            r'#define[ \t]+(?P<key>\w+)(?:[ \t](?P<val>[^\n]*))?', re.M)
        self.rePI = re.compile(r'#(?P<val>\w+[ \t]+[^\n]+)', re.M)
        Parser.__init__(self)

    def getNext(self, ctx, offset):
        junk_offset = offset
        contents = ctx.contents

        m = self.reComment.match(ctx.contents, offset)
        if m:
            current_comment = self.Comment(ctx, m.span())
            offset = m.end()
        else:
            current_comment = None

        m = self.reWhitespace.match(contents, offset)
        if m:
            # blank lines outside of filter_empty_lines or
            # leading whitespace are bad
            if (
                offset == 0 or
                not (len(m.group()) == 1 or ctx.filter_empty_lines)
            ):
                if current_comment:
                    return current_comment
                return Junk(ctx, m.span())
            white_space = Whitespace(ctx, m.span())
            offset = m.end()
            if (
                current_comment is not None
                and white_space.raw_val.count('\n') > 1
            ):
                # standalone comment
                # return the comment, and reparse the whitespace next time
                return current_comment
            if current_comment is None:
                return white_space
        else:
            white_space = None

        m = self.reKey.match(contents, offset)
        if m:
            return self.createEntity(ctx, m, current_comment, white_space)
        # defines instructions don't have comments
        # Any pending commment is standalone
        if current_comment:
            return current_comment
        if white_space:
            return white_space
        m = self.rePI.match(contents, offset)
        if m:
            instr = DefinesInstruction(ctx, m.span(), m.span('val'))
            if instr.val == 'filter emptyLines':
                ctx.filter_empty_lines = True
            if instr.val == 'unfilter emptyLines':
                ctx.filter_empty_lines = False
            return instr
        return self.getJunk(
            ctx, junk_offset, self.reComment, self.reKey, self.rePI)
