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
from compare_locales.keyedtuple import KeyedTuple
from compare_locales.paths import File

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


class Entry(object):
    '''
    Abstraction layer for a localizable entity.
    Currently supported are grammars of the form:

    1: entity definition
    2: entity key (name)
    3: entity value

    <!ENTITY key "value">

    <--- definition ---->
    '''
    def __init__(
        self, ctx, pre_comment, inner_white, span, key_span, val_span
    ):
        self.ctx = ctx
        self.span = span
        self.key_span = key_span
        self.val_span = val_span
        self.pre_comment = pre_comment
        self.inner_white = inner_white

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

    def _span_start(self):
        start = self.span[0]
        if hasattr(self, 'pre_comment') and self.pre_comment is not None:
            start = self.pre_comment.span[0]
        return start

    @property
    def all(self):
        start = self._span_start()
        end = self.span[1]
        return self.ctx.contents[start:end]

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


class StickyEntry(Entry):
    """Subclass of Entry to use in for syntax fragments
    which should always be overwritten in the serializer.
    """
    pass


class Entity(Entry):
    @property
    def localized(self):
        '''Is this entity localized.

        Always true for monolingual files.
        In bilingual files, this is a dynamic property.
        '''
        return True

    def unwrap(self):
        """Return the literal value to be used by tools.
        """
        return self.raw_val

    def wrap(self, raw_val):
        """Create literal entity based on reference and raw value.

        This is used by the serialization logic.
        """
        start = self._span_start()
        all = (
            self.ctx.contents[start:self.val_span[0]] +
            raw_val +
            self.ctx.contents[self.val_span[1]:self.span[1]]
        )
        return LiteralEntity(self.key, raw_val, all)


class LiteralEntity(Entity):
    """Subclass of Entity to represent entities without context slices.

    It's storing string literals for key, raw_val and all instead of spans.
    """
    def __init__(self, key, val, all):
        super(LiteralEntity, self).__init__(None, None, None, None, None, None)
        self._key = key
        self._raw_val = val
        self._all = all

    @property
    def key(self):
        return self._key

    @property
    def raw_val(self):
        return self._raw_val

    @property
    def all(self):
        return self._all


class PlaceholderEntity(LiteralEntity):
    """Subclass of Entity to be removed in merges.
    """
    def __init__(self, key):
        super(PlaceholderEntity, self).__init__(key, "", "\nplaceholder\n")


class Comment(Entry):
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

    def error_message(self):
        params = (self.val,) + self.position() + self.position(-1)
        return (
            'Unparsed content "%s" from line %d column %d'
            ' to line %d column %d' % params
        )

    def __repr__(self):
        return self.key


class Whitespace(Entry):
    '''Entity-like object representing an empty file with whitespace,
    if allowed
    '''
    def __init__(self, ctx, span):
        self.ctx = ctx
        self.span = self.key_span = self.val_span = span

    def __repr__(self):
        return self.raw_val


class BadEntity(ValueError):
    '''Raised when the parser can't create an Entity for a found match.
    '''
    pass


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
            # cache split lines
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

    def readFile(self, file):
        '''Read contents from disk, with universal_newlines'''
        if isinstance(file, File):
            file = file.fullpath
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
        self.ctx = self.Context(contents)

    def parse(self):
        return KeyedTuple(self)

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
        '''Parse the next fragment.

        Parse comments first, then white-space.
        If an entity follows, create that entity with such pre_comment and
        inner white-space. If not, emit comment or white-space as standlone.
        It's OK that this might parse whitespace more than once.
        Comments are associated with entities if they're not separated by
        blank lines. Multiple consecutive comments are joined.
        '''
        junk_offset = offset
        m = self.reComment.match(ctx.contents, offset)
        if m:
            current_comment = self.Comment(ctx, m.span())
            if offset < 2 and 'License' in current_comment.val:
                # Heuristic. A early comment with "License" is probably
                # a license header, and should be standalone.
                # Not glueing ourselves to offset == 0 as we might have
                # skipped a BOM.
                return current_comment
            offset = m.end()
        else:
            current_comment = None
        m = self.reWhitespace.match(ctx.contents, offset)
        if m:
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
        m = self.reKey.match(ctx.contents, offset)
        if m:
            try:
                return self.createEntity(ctx, m, current_comment, white_space)
            except BadEntity:
                # fall through to Junk, probably
                pass
        if current_comment is not None:
            return current_comment
        if white_space is not None:
            return white_space
        return self.getJunk(ctx, junk_offset, self.reKey, self.reComment)

    def getJunk(self, ctx, offset, *expressions):
        junkend = None
        for exp in expressions:
            m = exp.search(ctx.contents, offset)
            if m:
                junkend = min(junkend, m.start()) if junkend else m.start()
        return Junk(ctx, (offset, junkend or len(ctx.contents)))

    def createEntity(self, ctx, m, current_comment, white_space):
        return Entity(
            ctx, current_comment, white_space,
            m.span(), m.span('key'), m.span('val')
        )

    @classmethod
    def findDuplicates(cls, entities):
        found = Counter(entity.key for entity in entities)
        for entity_id, cnt in found.items():
            if cnt > 1:
                yield '{} occurs {} times'.format(entity_id, cnt)
