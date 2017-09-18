# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import bisect
import codecs
from collections import Counter
import logging

from fluent.syntax import FluentParser as FTLParser
from fluent.syntax import ast as ftl

__constructors = []


# The allowed capabilities for the Parsers.  They define the exact strategy
# used by ContentComparer.merge.

# Don't perform any merging
CAN_NONE = 0
# Copy the entire reference file
CAN_COPY = 1
# Remove broken entities from localization
CAN_SKIP = 2
# Add missing and broken entities from the reference to localization
CAN_MERGE = 4


class EntityBase(object):
    '''
    Abstraction layer for a localizable entity.
    Currently supported are grammars of the form:

    1: pre white space
    2: entity definition
    3: entity key (name)
    4: entity value
    5: post white space
                                                 <--[1]
    <!ENTITY key "value">

    <-------[2]--------->
    '''
    def __init__(self, ctx, pre_comment,
                 span, pre_ws_span, def_span,
                 key_span, val_span, post_span):
        self.ctx = ctx
        self.span = span
        self.pre_ws_span = pre_ws_span
        self.def_span = def_span
        self.key_span = key_span
        self.val_span = val_span
        self.post_span = post_span
        self.pre_comment = pre_comment
        pass

    def position(self, offset=0):
        """Get the 1-based line and column of the character
        with given offset into the Entity.

        If offset is negative, return the end of the Entity.
        """
        if offset < 0:
            pos = self.span[1]
        else:
            pos = self.span[0] + offset
        return self.ctx.lines(pos)[0]

    def value_position(self, offset=0):
        """Get the 1-based line and column of the character
        with given offset into the value.

        If offset is negative, return the end of the value.
        """
        if offset < 0:
            pos = self.val_span[1]
        else:
            pos = self.val_span[0] + offset
        return self.ctx.lines(pos)[0]

    # getter helpers

    def get_all(self):
        return self.ctx.contents[self.span[0]:self.span[1]]

    def get_pre_ws(self):
        return self.ctx.contents[self.pre_ws_span[0]:self.pre_ws_span[1]]

    def get_def(self):
        return self.ctx.contents[self.def_span[0]:self.def_span[1]]

    def get_key(self):
        return self.ctx.contents[self.key_span[0]:self.key_span[1]]

    def get_raw_val(self):
        return self.ctx.contents[self.val_span[0]:self.val_span[1]]

    def get_post(self):
        return self.ctx.contents[self.post_span[0]:self.post_span[1]]

    # getters

    all = property(get_all)
    pre_ws = property(get_pre_ws)
    definition = property(get_def)
    key = property(get_key)
    val = property(get_raw_val)
    raw_val = property(get_raw_val)
    post = property(get_post)

    def __repr__(self):
        return self.key

    re_br = re.compile('<br\s*/?>', re.U)
    re_sgml = re.compile('</?\w+.*?>', re.U | re.M)

    def count_words(self):
        """Count the words in an English string.
        Replace a couple of xml markup to make that safer, too.
        """
        value = self.re_br.sub(u'\n', self.val)
        value = self.re_sgml.sub(u'', value)
        return len(value.split())

    def equals(self, other):
        return self.key == other.key and self.val == other.val


class Entity(EntityBase):
    pass


class Comment(EntityBase):
    def __init__(self, ctx, span, pre_ws_span, def_span,
                 post_span):
        self.ctx = ctx
        self.span = span
        self.pre_ws_span = pre_ws_span
        self.def_span = def_span
        self.post_span = post_span

    @property
    def key(self):
        return None

    @property
    def val(self):
        return None

    def __repr__(self):
        return self.all


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
        self.pre_ws = self.definition = self.post = ''
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
        return self.ctx.lines(pos)[0]

    # getter helpers
    def get_all(self):
        return self.ctx.contents[self.span[0]:self.span[1]]

    # getters
    all = property(get_all)
    val = property(get_all)

    def __repr__(self):
        return self.key


class Whitespace(EntityBase):
    '''Entity-like object representing an empty file with whitespace,
    if allowed
    '''
    def __init__(self, ctx, span):
        self.ctx = ctx
        self.key_span = self.val_span = self.span = span
        self.def_span = self.pre_ws_span = (span[0], span[0])
        self.post_span = (span[1], span[1])

    def __repr__(self):
        return self.raw_val


class Parser(object):
    capabilities = CAN_SKIP | CAN_MERGE
    tail = re.compile('\s+\Z')

    class Context(object):
        "Fixture for content and line numbers"
        def __init__(self, contents):
            self.contents = contents
            self._lines = None

        def lines(self, *positions):
            # return line and column tuples, 1-based
            if self._lines is None:
                nl = re.compile('\n', re.M)
                self._lines = [m.end()
                               for m in nl.finditer(self.contents)]
            line_nrs = [bisect.bisect(self._lines, p) for p in positions]
            # compute columns
            pos_ = [
                (1 + line, 1 + p - (self._lines[line-1] if line else 0))
                for line, p in zip(line_nrs, positions)]
            return pos_

    def __init__(self):
        if not hasattr(self, 'encoding'):
            self.encoding = 'utf-8'
        self.ctx = None
        self.last_comment = None

    def readFile(self, file):
        with open(file, 'rU') as f:
            try:
                self.readContents(f.read())
            except UnicodeDecodeError, e:
                (logging.getLogger('locales')
                        .error("Can't read file: " + file + '; ' + str(e)))

    def readContents(self, contents):
        '''Read contents and create parsing context.

        contents are in native encoding, but with normalized line endings.
        '''
        (contents, length) = codecs.getdecoder(self.encoding)(contents)
        self.ctx = Parser.Context(contents)

    def parse(self):
        l = []
        m = {}
        for e in self:
            m[e.key] = len(l)
            l.append(e)
        return (l, m)

    def __iter__(self):
        return self.walk(onlyEntities=True)

    def walk(self, onlyEntities=False):
        if not self.ctx:
            # loading file failed, or we just didn't load anything
            return
        ctx = self.ctx
        contents = ctx.contents
        offset = 0
        entity, offset = self.getEntity(ctx, offset)
        while entity:
            if (not onlyEntities or
                    isinstance(entity, Entity) or
                    type(entity) is Junk):
                yield entity
            entity, offset = self.getEntity(ctx, offset)
        if len(contents) > offset:
            yield Junk(ctx, (offset, len(contents)))

    def getEntity(self, ctx, offset):
        m = self.reKey.match(ctx.contents, offset)
        if m:
            offset = m.end()
            entity = self.createEntity(ctx, m)
            return (entity, offset)
        m = self.reComment.match(ctx.contents, offset)
        if m:
            offset = m.end()
            self.last_comment = Comment(ctx, *[m.span(i) for i in xrange(4)])
            return (self.last_comment, offset)
        return self.getTrailing(ctx, offset, self.reKey, self.reComment)

    def getTrailing(self, ctx, offset, *expressions):
        junkend = None
        for exp in expressions:
            m = exp.search(ctx.contents, offset)
            if m:
                junkend = min(junkend, m.start()) if junkend else m.start()
        if junkend is None:
            if self.tail.match(ctx.contents, offset):
                white_end = len(ctx.contents)
                return (Whitespace(ctx, (offset, white_end)), white_end)
            else:
                return (None, offset)
        return (Junk(ctx, (offset, junkend)), junkend)

    def createEntity(self, ctx, m):
        pre_comment = self.last_comment
        self.last_comment = None
        return Entity(ctx, pre_comment,
                      *[m.span(i) for i in xrange(6)])

    @classmethod
    def findDuplicates(cls, entities):
        found = Counter(entity.key for entity in entities)
        for entity_id, cnt in found.items():
            if cnt > 1:
                yield '{} occurs {} times'.format(entity_id, cnt)


def getParser(path):
    for item in __constructors:
        if re.search(item[0], path):
            return item[1]
    raise UserWarning("Cannot find Parser")


# Subgroups of the match will:
# 1: pre white space
# 2: pre comments
# 3: entity definition
# 4: entity key (name)
# 5: entity value
# 6: post comment (and white space) in the same line (dtd only)
#                                            <--[1]
# <!-- pre comments -->                      <--[2]
# <!ENTITY key "value"> <!-- comment -->
#
# <-------[3]---------><------[6]------>


class DTDEntity(Entity):
    def value_position(self, offset=0):
        # DTDChecker already returns tuples of (line, col) positions
        if isinstance(offset, tuple):
            line_pos, col_pos = offset
            line, col = super(DTDEntity, self).value_position()
            if line_pos == 1:
                col = col + col_pos
            else:
                col = col_pos
                line += line_pos - 1
            return line, col
        else:
            return super(DTDEntity, self).value_position(offset)


class DTDParser(Parser):
    # http://www.w3.org/TR/2006/REC-xml11-20060816/#NT-NameStartChar
    # ":" | [A-Z] | "_" | [a-z] |
    # [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] | [#x370-#x37D] | [#x37F-#x1FFF]
    # | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] |
    # [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] |
    # [#x10000-#xEFFFF]
    CharMinusDash = u'\x09\x0A\x0D\u0020-\u002C\u002E-\uD7FF\uE000-\uFFFD'
    XmlComment = '<!--(?:-?[%s])*?-->' % CharMinusDash
    NameStartChar = u':A-Z_a-z\xC0-\xD6\xD8-\xF6\xF8-\u02FF' + \
        u'\u0370-\u037D\u037F-\u1FFF\u200C-\u200D\u2070-\u218F' + \
        u'\u2C00-\u2FEF\u3001-\uD7FF\uF900-\uFDCF\uFDF0-\uFFFD'
    # + \U00010000-\U000EFFFF seems to be unsupported in python

    # NameChar ::= NameStartChar | "-" | "." | [0-9] | #xB7 |
    #     [#x0300-#x036F] | [#x203F-#x2040]
    NameChar = NameStartChar + ur'\-\.0-9' + u'\xB7\u0300-\u036F\u203F-\u2040'
    Name = '[' + NameStartChar + '][' + NameChar + ']*'
    reKey = re.compile('(?:(?P<pre>\s*)(?P<entity><!ENTITY\s+(?P<key>' + Name +
                       ')\s+(?P<val>\"[^\"]*\"|\'[^\']*\'?)\s*>)'
                       '(?P<post>\s+)?)',
                       re.DOTALL | re.M)
    # add BOM to DTDs, details in bug 435002
    reHeader = re.compile(u'^\ufeff')
    reComment = re.compile('(\s*)(<!--(-?[%s])*?-->)(\s*)' % CharMinusDash,
                           re.S)
    rePE = re.compile(u'(?:(\s*)'
                      u'(<!ENTITY\s+%\s+(' + Name +
                      u')\s+SYSTEM\s+(\"[^\"]*\"|\'[^\']*\')\s*>\s*%' + Name +
                      u';)([ \t]*(?:' + XmlComment + u'\s*)*\n?)?)')

    def getEntity(self, ctx, offset):
        '''
        Overload Parser.getEntity to special-case ParsedEntities.
        Just check for a parsed entity if that method claims junk.

        <!ENTITY % foo SYSTEM "url">
        %foo;
        '''
        if offset is 0 and self.reHeader.match(ctx.contents):
            offset += 1
        entity, inneroffset = Parser.getEntity(self, ctx, offset)
        if (entity and isinstance(entity, Junk)) or entity is None:
            m = self.rePE.match(ctx.contents, offset)
            if m:
                inneroffset = m.end()
                self.last_comment = None
                entity = DTDEntity(ctx, '', *[m.span(i) for i in xrange(6)])
        return (entity, inneroffset)

    def createEntity(self, ctx, m):
        valspan = m.span('val')
        valspan = (valspan[0]+1, valspan[1]-1)
        pre_comment = self.last_comment
        self.last_comment = None
        return DTDEntity(ctx, pre_comment,
                         m.span(),
                         m.span('pre'),
                         m.span('entity'), m.span('key'), valspan,
                         m.span('post'))


class PropertiesEntity(Entity):
    escape = re.compile(r'\\((?P<uni>u[0-9a-fA-F]{1,4})|'
                        '(?P<nl>\n\s*)|(?P<single>.))', re.M)
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


class PropertiesParser(Parser):
    def __init__(self):
        self.reKey = re.compile('^(\s*)'
                                '([^#!\s\n][^=:\n]*?)\s*[:=][ \t]*', re.M)
        self.reComment = re.compile('(\s*)(((?:[#!][^\n]*\n?)+))', re.M)
        self._escapedEnd = re.compile(r'\\+$')
        self._trailingWS = re.compile(r'\s*(?:\n|\Z)', re.M)
        Parser.__init__(self)

    def getEntity(self, ctx, offset):
        # overwritten to parse values line by line
        contents = ctx.contents
        m = self.reComment.match(contents, offset)
        if m:
            spans = [m.span(i) for i in xrange(3)]
            start_trailing = offset = m.end()
            while offset < len(contents):
                m = self._trailingWS.match(contents, offset)
                if not m:
                    break
                offset = m.end()
            spans.append((start_trailing, offset))
            self.last_comment = Comment(ctx, *spans)
            return (self.last_comment, offset)
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
                offset = ws.end()
            pre_comment = self.last_comment
            self.last_comment = None
            entity = PropertiesEntity(
                ctx, pre_comment,
                (m.start(), offset),   # full span
                m.span(1),  # leading whitespan
                (m.start(2), offset),   # entity def span
                m.span(2),   # key span
                (m.end(), endval),   # value span
                (offset, offset))  # post comment span, empty
            return (entity, offset)
        return self.getTrailing(ctx, offset, self.reKey, self.reComment)


class DefinesInstruction(EntityBase):
    '''Entity-like object representing processing instructions in inc files
    '''
    def __init__(self, ctx, span, pre_ws_span, def_span, val_span, post_span):
        self.ctx = ctx
        self.span = span
        self.pre_ws_span = pre_ws_span
        self.def_span = def_span
        self.key_span = self.val_span = val_span
        self.post_span = post_span

    def __repr__(self):
        return self.raw_val


class DefinesParser(Parser):
    # can't merge, #unfilter needs to be the last item, which we don't support
    capabilities = CAN_COPY
    tail = re.compile(r'(?!)')  # never match

    def __init__(self):
        self.reComment = re.compile(
            '((?:[ \t]*\n)*)'
            '((?:^# .*?(?:\n|\Z))+)'
            '((?:[ \t]*(?:\n|\Z))*)', re.M)
        self.reKey = re.compile('((?:[ \t]*\n)*)'
                                '(#define[ \t]+(\w+)(?:[ \t](.*?))?(?:\n|\Z))'
                                '((?:[ \t]*(?:\n|\Z))*)',
                                re.M)
        self.rePI = re.compile('((?:[ \t]*\n)*)'
                               '(#(\w+)[ \t]+(.*?)(?:\n|\Z))'
                               '((?:[ \t]*(?:\n|\Z))*)',
                               re.M)
        Parser.__init__(self)

    def getEntity(self, ctx, offset):
        contents = ctx.contents
        m = self.reComment.match(contents, offset)
        if m:
            offset = m.end()
            self.last_comment = Comment(ctx, *[m.span(i) for i in xrange(4)])
            return (self.last_comment, offset)
        m = self.reKey.match(contents, offset)
        if m:
            offset = m.end()
            return (self.createEntity(ctx, m), offset)
        m = self.rePI.match(contents, offset)
        if m:
            offset = m.end()
            return (DefinesInstruction(ctx, *[m.span(i) for i in xrange(5)]),
                    offset)
        return self.getTrailing(ctx, offset,
                                self.reComment, self.reKey, self.rePI)


class IniSection(EntityBase):
    '''Entity-like object representing sections in ini files
    '''
    def __init__(self, ctx, span, pre_ws_span, def_span, val_span, post_span):
        self.ctx = ctx
        self.span = span
        self.pre_ws_span = pre_ws_span
        self.def_span = def_span
        self.key_span = self.val_span = val_span
        self.post_span = post_span

    def __repr__(self):
        return self.raw_val


class IniParser(Parser):
    '''
    Parse files of the form:
    # initial comment
    [cat]
    whitespace*
    #comment
    string=value
    ...
    '''
    def __init__(self):
        self.reComment = re.compile(
            '((?:[ \t]*\n)*)'
            '((?:^[;#].*?(?:\n|\Z))+)'
            '((?:[ \t]*(?:\n|\Z))*)', re.M)
        self.reSection = re.compile(
            '((?:[ \t]*\n)*)'
            '(\[(.*?)\])'
            '((?:[ \t]*(?:\n|\Z))*)', re.M)
        self.reKey = re.compile(
            '((?:[ \t]*\n)*)'
            '((.+?)=(.*))'
            '((?:[ \t]*(?:\n|\Z))*)', re.M)
        Parser.__init__(self)

    def getEntity(self, ctx, offset):
        contents = ctx.contents
        m = self.reComment.match(contents, offset)
        if m:
            offset = m.end()
            self.last_comment = Comment(ctx, *[m.span(i) for i in xrange(4)])
            return (self.last_comment, offset)
        m = self.reSection.match(contents, offset)
        if m:
            offset = m.end()
            return (IniSection(ctx, *[m.span(i) for i in xrange(5)]), offset)
        m = self.reKey.match(contents, offset)
        if m:
            offset = m.end()
            return (self.createEntity(ctx, m), offset)
        return self.getTrailing(ctx, offset,
                                self.reComment, self.reSection, self.reKey)


class FluentAttribute(EntityBase):
    ignored_fields = ['span']

    def __init__(self, entity, attr_node):
        self.ctx = entity.ctx
        self.attr = attr_node
        self.key_span = (attr_node.id.span.start, attr_node.id.span.end)
        self.val_span = (attr_node.value.span.start, attr_node.value.span.end)

    def equals(self, other):
        if not isinstance(other, FluentAttribute):
            return False
        return self.attr.equals(
            other.attr, ignored_fields=self.ignored_fields)


class FluentEntity(Entity):
    # Fields ignored when comparing two entities.
    ignored_fields = ['comment', 'span', 'tags']

    def __init__(self, ctx, entry):
        start = entry.span.start
        end = entry.span.end

        self.ctx = ctx
        self.span = (start, end)

        self.key_span = (entry.id.span.start, entry.id.span.end)

        if entry.value is not None:
            self.val_span = (entry.value.span.start, entry.value.span.end)
        else:
            self.val_span = (0, 0)

        self.entry = entry

    _word_count = None

    def count_words(self):
        if self._word_count is None:
            self._word_count = 0

            def count_words(node):
                if isinstance(node, ftl.TextElement):
                    self._word_count += len(node.value.split())
                return node

            self.entry.traverse(count_words)

        return self._word_count

    def equals(self, other):
        return self.entry.equals(
            other.entry, ignored_fields=self.ignored_fields)

    # Positions yielded by FluentChecker.check are absolute offsets from the
    # beginning of the file.  This is different from the base Checker behavior
    # which yields offsets from the beginning of the current entity's value.
    def position(self, pos=None):
        if pos is None:
            pos = self.entry.span.start
        return self.ctx.lines(pos)[0]

    # FluentEntities don't differentiate between entity and value positions
    # because all positions are absolute from the beginning of the file.
    def value_position(self, pos=None):
        return self.position(pos)

    @property
    def attributes(self):
        for attr_node in self.entry.attributes:
            yield FluentAttribute(self, attr_node)


class FluentParser(Parser):
    capabilities = CAN_SKIP

    def __init__(self):
        super(FluentParser, self).__init__()
        self.ftl_parser = FTLParser()

    def walk(self, onlyEntities=False):
        if not self.ctx:
            # loading file failed, or we just didn't load anything
            return
        resource = self.ftl_parser.parse(self.ctx.contents)
        for entry in resource.body:
            if isinstance(entry, ftl.Message):
                yield FluentEntity(self.ctx, entry)
            elif isinstance(entry, ftl.Junk):
                start = entry.span.start
                end = entry.span.end
                # strip leading whitespace
                start += re.match('\s*', entry.content).end()
                # strip trailing whitespace
                ws, we = re.search('\s*$', entry.content).span()
                end -= we - ws
                yield Junk(self.ctx, (start, end))


__constructors = [('\\.dtd$', DTDParser()),
                  ('\\.properties$', PropertiesParser()),
                  ('\\.ini$', IniParser()),
                  ('\\.inc$', DefinesParser()),
                  ('\\.ftl$', FluentParser())]
