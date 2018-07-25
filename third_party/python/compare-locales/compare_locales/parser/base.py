# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re
import bisect
import codecs
from collections import Counter
import logging

import six

__constructors = []


# The allowed capabilities for the Parsers.  They define the exact strategy
# used by ContentComparer.merge.

# Don't perform any merging
CAN_NONE = 0
# Copy the entire reference file
CAN_COPY = 1
# Remove broken entities from localization
# Without CAN_MERGE, en-US is not good to use for localization.
CAN_SKIP = 2
# Add missing and broken entities from the reference to localization
# This effectively means that en-US is good to use for localized files.
CAN_MERGE = 4


class EntityBase(object):
    '''
    Abstraction layer for a localizable entity.
    Currently supported are grammars of the form:

    1: entity definition
    2: entity key (name)
    3: entity value

    <!ENTITY key "value">

    <--- definition ---->
    '''
    def __init__(self, ctx, pre_comment, span, key_span, val_span):
        self.ctx = ctx
        self.span = span
        self.key_span = key_span
        self.val_span = val_span
        self.pre_comment = pre_comment

    def position(self, offset=0):
        """Get the 1-based line and column of the character
        with given offset into the Entity.

        If offset is negative, return the end of the Entity.
        """
        if offset < 0:
            pos = self.span[1]
        else:
            pos = self.span[0] + offset
        return self.ctx.linecol(pos)

    def value_position(self, offset=0):
        """Get the 1-based line and column of the character
        with given offset into the value.

        If offset is negative, return the end of the value.
        """
        assert self.val_span is not None
        if offset < 0:
            pos = self.val_span[1]
        else:
            pos = self.val_span[0] + offset
        return self.ctx.linecol(pos)

    @property
    def all(self):
        return self.ctx.contents[self.span[0]:self.span[1]]

    @property
    def key(self):
        return self.ctx.contents[self.key_span[0]:self.key_span[1]]

    @property
    def raw_val(self):
        if self.val_span is None:
            return None
        return self.ctx.contents[self.val_span[0]:self.val_span[1]]

    @property
    def val(self):
        return self.raw_val

    def __repr__(self):
        return self.key

    re_br = re.compile('<br[ \t\r\n]*/?>', re.U)
    re_sgml = re.compile(r'</?\w+.*?>', re.U | re.M)

    def count_words(self):
        """Count the words in an English string.
        Replace a couple of xml markup to make that safer, too.
        """
        value = self.re_br.sub('\n', self.val)
        value = self.re_sgml.sub('', value)
        return len(value.split())

    def equals(self, other):
        return self.key == other.key and self.val == other.val


class Entity(EntityBase):
    pass


class Comment(EntityBase):
    def __init__(self, ctx, span):
        self.ctx = ctx
        self.span = span
        self.val_span = None
        self._val_cache = None

    @property
    def key(self):
        return None

    @property
    def val(self):
        if self._val_cache is None:
            self._val_cache = self.all
        return self._val_cache

    def __repr__(self):
        return self.all


class OffsetComment(Comment):
    '''Helper for file formats that have a constant number of leading
    chars to strip from comments.
    Offset defaults to 1
    '''
    comment_offset = 1

    @property
    def val(self):
        if self._val_cache is None:
            self._val_cache = ''.join((
                l[self.comment_offset:] for l in self.all.splitlines(True)
            ))
        return self._val_cache


class Junk(object):
    '''
    An almost-Entity, representing junk data that we didn't parse.
    This way, we can signal bad content as stuff we don't understand.
    And the either fix that, or report real bugs in localizations.
    '''
    junkid = 0

    def __init__(self, ctx, span):
        self.ctx = ctx
        self.span = span
        self.__class__.junkid += 1
        self.key = '_junk_%d_%d-%d' % (self.__class__.junkid, span[0], span[1])

    def position(self, offset=0):
        """Get the 1-based line and column of the character
        with given offset into the Entity.

        If offset is negative, return the end of the Entity.
        """
        if offset < 0:
            pos = self.span[1]
        else:
            pos = self.span[0] + offset
        return self.ctx.linecol(pos)

    @property
    def all(self):
        return self.ctx.contents[self.span[0]:self.span[1]]

    @property
    def raw_val(self):
        return self.all

    @property
    def val(self):
        return self.all

    def __repr__(self):
        return self.key


class Whitespace(EntityBase):
    '''Entity-like object representing an empty file with whitespace,
    if allowed
    '''
    def __init__(self, ctx, span):
        self.ctx = ctx
        self.span = self.key_span = self.val_span = span

    def __repr__(self):
        return self.raw_val


class Parser(object):
    capabilities = CAN_SKIP | CAN_MERGE
    reWhitespace = re.compile('[ \t\r\n]+', re.M)
    Comment = Comment
    # NotImplementedError would be great, but also tedious
    reKey = reComment = None

    class Context(object):
        "Fixture for content and line numbers"
        def __init__(self, contents):
            self.contents = contents
            # Subclasses may use bitmasks to keep state.
            self.state = 0
            self._lines = None

        def linecol(self, position):
            "Returns 1-based line and column numbers."
            if self._lines is None:
                nl = re.compile('\n', re.M)
                self._lines = [m.end()
                               for m in nl.finditer(self.contents)]

            line_offset = bisect.bisect(self._lines, position)
            line_start = self._lines[line_offset - 1] if line_offset else 0
            col_offset = position - line_start

            return line_offset + 1, col_offset + 1

    def __init__(self):
        if not hasattr(self, 'encoding'):
            self.encoding = 'utf-8'
        self.ctx = None
        self.last_comment = None

    def readFile(self, file):
        '''Read contents from disk, with universal_newlines'''
        # python 2 has binary input with universal newlines,
        # python 3 doesn't. Let's split code paths
        if six.PY2:
            with open(file, 'rbU') as f:
                try:
                    self.readContents(f.read())
                except UnicodeDecodeError as e:
                    (logging.getLogger('locales')
                            .error("Can't read file: " + file + '; ' + str(e)))
        else:
            with open(file, 'r', encoding=self.encoding, newline=None) as f:
                try:
                    self.readUnicode(f.read())
                except UnicodeDecodeError as e:
                    (logging.getLogger('locales')
                            .error("Can't read file: " + file + '; ' + str(e)))

    def readContents(self, contents):
        '''Read contents and create parsing context.

        contents are in native encoding, but with normalized line endings.
        '''
        (contents, _) = codecs.getdecoder(self.encoding)(contents)
        self.readUnicode(contents)

    def readUnicode(self, contents):
        self.ctx = Parser.Context(contents)

    def parse(self):
        list_ = list(self)
        map_ = dict((e.key, i) for i, e in enumerate(list_))
        return (list_, map_)

    def __iter__(self):
        return self.walk(only_localizable=True)

    def walk(self, only_localizable=False):
        if not self.ctx:
            # loading file failed, or we just didn't load anything
            return
        ctx = self.ctx
        contents = ctx.contents

        next_offset = 0
        while next_offset < len(contents):
            entity = self.getNext(ctx, next_offset)

            if isinstance(entity, (Entity, Junk)):
                yield entity
            elif not only_localizable:
                yield entity

            next_offset = entity.span[1]

    def getNext(self, ctx, offset):
        m = self.reWhitespace.match(ctx.contents, offset)
        if m:
            return Whitespace(ctx, m.span())
        m = self.reKey.match(ctx.contents, offset)
        if m:
            return self.createEntity(ctx, m)
        m = self.reComment.match(ctx.contents, offset)
        if m:
            self.last_comment = self.Comment(ctx, m.span())
            return self.last_comment
        return self.getJunk(ctx, offset, self.reKey, self.reComment)

    def getJunk(self, ctx, offset, *expressions):
        junkend = None
        for exp in expressions:
            m = exp.search(ctx.contents, offset)
            if m:
                junkend = min(junkend, m.start()) if junkend else m.start()
        return Junk(ctx, (offset, junkend or len(ctx.contents)))

    def createEntity(self, ctx, m):
        pre_comment = self.last_comment
        self.last_comment = None
        return Entity(ctx, pre_comment, m.span(), m.span('key'), m.span('val'))

    @classmethod
    def findDuplicates(cls, entities):
        found = Counter(entity.key for entity in entities)
        for entity_id, cnt in found.items():
            if cnt > 1:
                yield '{} occurs {} times'.format(entity_id, cnt)
