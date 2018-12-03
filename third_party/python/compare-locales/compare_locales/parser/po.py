# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Gettext PO(T) parser

Parses gettext po and pot files.
"""

from __future__ import absolute_import
from __future__ import unicode_literals

import re

from .base import (
    CAN_SKIP,
    Entity,
    BadEntity,
    Parser
)


class PoEntityMixin(object):

    @property
    def val(self):
        return self.stringlist_val

    @property
    def key(self):
        return self.stringlist_key

    @property
    def localized(self):
        # gettext denotes a non-localized string by an empty value
        return bool(self.val)

    def __repr__(self):
        return self.key[0]


class PoEntity(PoEntityMixin, Entity):
    pass


# Unescape and concat a string list
def eval_stringlist(lines):
    return ''.join(
        (
            l
            .replace(r'\\', '\\')
            .replace(r'\t', '\t')
            .replace(r'\r', '\r')
            .replace(r'\n', '\n')
            .replace(r'\"', '"')
        )
        for l in lines
    )


class PoParser(Parser):
    # gettext l10n fallback at runtime, don't merge en-US strings
    capabilities = CAN_SKIP

    reKey = re.compile('msgctxt|msgid')
    reValue = re.compile('(?P<white>[ \t\r\n]*)(?P<cmd>msgstr)')
    reComment = re.compile(r'(?:#.*?\n)+')
    # string list item:
    # leading whitespace
    # `"`
    # escaped quotes etc, not quote, newline, backslash
    # `"`
    reListItem = re.compile(r'[ \t\r\n]*"((?:\\[\\trn"]|[^"\n\\])*)"')

    def __init__(self):
        super(PoParser, self).__init__()

    def createEntity(self, ctx, m, current_comment, white_space):
        start = cursor = m.start()
        id_start = cursor
        try:
            msgctxt, cursor = self._parse_string_list(ctx, cursor, 'msgctxt')
            m = self.reWhitespace.match(ctx.contents, cursor)
            if m:
                cursor = m.end()
        except BadEntity:
            # no msgctxt is OK
            msgctxt = None
        if id_start is None:
            id_start = cursor
        msgid, cursor = self._parse_string_list(ctx, cursor, 'msgid')
        id_end = cursor
        m = self.reWhitespace.match(ctx.contents, cursor)
        if m:
            cursor = m.end()
        val_start = cursor
        msgstr, cursor = self._parse_string_list(ctx, cursor, 'msgstr')
        e = PoEntity(
            ctx,
            current_comment,
            white_space,
            (start, cursor),
            (id_start, id_end),
            (val_start, cursor)
        )
        e.stringlist_key = (msgid, msgctxt)
        e.stringlist_val = msgstr
        return e

    def _parse_string_list(self, ctx, cursor, key):
        if not ctx.contents.startswith(key, cursor):
            raise BadEntity
        cursor += len(key)
        frags = []
        while True:
            m = self.reListItem.match(ctx.contents, cursor)
            if not m:
                break
            frags.append(m.group(1))
            cursor = m.end()
        if not frags:
            raise BadEntity
        return eval_stringlist(frags), cursor
