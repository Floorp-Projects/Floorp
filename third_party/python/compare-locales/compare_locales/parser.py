# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import bisect
import codecs
from collections import Counter
import logging

try:
    from html import unescape as html_unescape
except ImportError:
    from HTMLParser import HTMLParser
    html_parser = HTMLParser()
    html_unescape = html_parser.unescape

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

    # getter helpers

    def get_all(self):
        return self.ctx.contents[self.span[0]:self.span[1]]

    def get_key(self):
        return self.ctx.contents[self.key_span[0]:self.key_span[1]]

    def get_raw_val(self):
        if self.val_span is None:
            return None
        return self.ctx.contents[self.val_span[0]:self.val_span[1]]

    # getters

    all = property(get_all)
    key = property(get_key)
    val = property(get_raw_val)
    raw_val = property(get_raw_val)

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

    # getter helpers
    def get_all(self):
        return self.ctx.contents[self.span[0]:self.span[1]]

    # getters
    all = property(get_all)
    raw_val = property(get_all)
    val = property(get_all)

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
    reWhitespace = re.compile('\s+', re.M)
    Comment = Comment

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


def getParser(path):
    for item in __constructors:
        if re.search(item[0], path):
            return item[1]
    raise UserWarning("Cannot find Parser")


class DTDEntity(Entity):
    @property
    def val(self):
        '''Unescape HTML entities into corresponding Unicode characters.

        Named (&amp;), decimal (&#38;), and hex (&#x26; and &#x0026;) formats
        are supported. Unknown entities are left intact.

        As of Python 2.7 and Python 3.6 the following 252 named entities are
        recognized and unescaped:

            https://github.com/python/cpython/blob/2.7/Lib/htmlentitydefs.py
            https://github.com/python/cpython/blob/3.6/Lib/html/entities.py
        '''
        return html_unescape(self.raw_val)

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
    reKey = re.compile('<!ENTITY\s+(?P<key>' + Name + ')\s+'
                       '(?P<val>\"[^\"]*\"|\'[^\']*\'?)\s*>',
                       re.DOTALL | re.M)
    # add BOM to DTDs, details in bug 435002
    reHeader = re.compile(u'^\ufeff')
    reComment = re.compile('<!--(?P<val>-?[%s])*?-->' % CharMinusDash,
                           re.S)
    rePE = re.compile(u'<!ENTITY\s+%\s+(?P<key>' + Name + ')\s+'
                      u'SYSTEM\s+(?P<val>\"[^\"]*\"|\'[^\']*\')\s*>\s*'
                      u'%' + Name + ';'
                      u'(?:[ \t]*(?:' + XmlComment + u'\s*)*\n?)?')

    class Comment(Comment):
        @property
        def val(self):
            if self._val_cache is None:
                # Strip "<!--" and "-->" to comment contents
                self._val_cache = self.all[4:-3]
            return self._val_cache

    def getNext(self, ctx, offset):
        '''
        Overload Parser.getNext to special-case ParsedEntities.
        Just check for a parsed entity if that method claims junk.

        <!ENTITY % foo SYSTEM "url">
        %foo;
        '''
        if offset is 0 and self.reHeader.match(ctx.contents):
            offset += 1
        entity = Parser.getNext(self, ctx, offset)
        if (entity and isinstance(entity, Junk)) or entity is None:
            m = self.rePE.match(ctx.contents, offset)
            if m:
                self.last_comment = None
                entity = DTDEntity(
                    ctx, '', m.span(), m.span('key'), m.span('val'))
        return entity

    def createEntity(self, ctx, m):
        valspan = m.span('val')
        valspan = (valspan[0]+1, valspan[1]-1)
        pre_comment = self.last_comment
        self.last_comment = None
        return DTDEntity(ctx, pre_comment,
                         m.span(), m.span('key'), valspan)


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

    Comment = OffsetComment

    def __init__(self):
        self.reKey = re.compile(
            '(?P<key>[^#!\s\n][^=:\n]*?)\s*[:=][ \t]*', re.M)
        self.reComment = re.compile('(?:[#!][^\n]*\n)*(?:[#!][^\n]*)', re.M)
        self._escapedEnd = re.compile(r'\\+$')
        self._trailingWS = re.compile(r'\s*(?:\n|\Z)', re.M)
        Parser.__init__(self)

    def getNext(self, ctx, offset):
        # overwritten to parse values line by line
        contents = ctx.contents

        m = self.reWhitespace.match(contents, offset)
        if m:
            return Whitespace(ctx, m.span())

        m = self.reComment.match(contents, offset)
        if m:
            self.last_comment = self.Comment(ctx, m.span())
            return self.last_comment

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

            pre_comment = self.last_comment
            self.last_comment = None
            entity = PropertiesEntity(
                ctx, pre_comment,
                (m.start(), endval),   # full span
                m.span('key'),
                (m.end(), endval))   # value span
            return entity

        return self.getJunk(ctx, offset, self.reKey, self.reComment)


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
            '#define[ \t]+(?P<key>\w+)(?:[ \t](?P<val>[^\n]*))?', re.M)
        self.rePI = re.compile('#(?P<val>\w+[ \t]+[^\n]+)', re.M)
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


class IniSection(EntityBase):
    '''Entity-like object representing sections in ini files
    '''
    def __init__(self, ctx, span, val_span):
        self.ctx = ctx
        self.span = span
        self.key_span = self.val_span = val_span

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

    Comment = OffsetComment

    def __init__(self):
        self.reComment = re.compile('(?:^[;#][^\n]*\n)*(?:^[;#][^\n]*)', re.M)
        self.reSection = re.compile('\[(?P<val>.*?)\]', re.M)
        self.reKey = re.compile('(?P<key>.+?)=(?P<val>.*)', re.M)
        Parser.__init__(self)

    def getNext(self, ctx, offset):
        contents = ctx.contents
        m = self.reWhitespace.match(contents, offset)
        if m:
            return Whitespace(ctx, m.span())
        m = self.reComment.match(contents, offset)
        if m:
            self.last_comment = self.Comment(ctx, m.span())
            return self.last_comment
        m = self.reSection.match(contents, offset)
        if m:
            return IniSection(ctx, m.span(), m.span('val'))
        m = self.reKey.match(contents, offset)
        if m:
            return self.createEntity(ctx, m)
        return self.getJunk(
            ctx, offset, self.reComment, self.reSection, self.reKey)


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
    ignored_fields = ['comment', 'span']

    def __init__(self, ctx, entry):
        start = entry.span.start
        end = entry.span.end

        self.ctx = ctx
        self.span = (start, end)

        self.key_span = (entry.id.span.start, entry.id.span.end)

        if entry.value is not None:
            self.val_span = (entry.value.span.start, entry.value.span.end)
        else:
            self.val_span = None

        self.entry = entry

        # EntityBase instances are expected to have pre_comment. It's used by
        # other formats to associate a Comment with an Entity. FluentEntities
        # don't need it because message comments are part of the entry AST and
        # are not separate Comment instances.
        self.pre_comment = None

    @property
    def root_node(self):
        '''AST node at which to start traversal for count_words.

        By default we count words in the value and in all attributes.
        '''
        return self.entry

    _word_count = None

    def count_words(self):
        if self._word_count is None:
            self._word_count = 0

            def count_words(node):
                if isinstance(node, ftl.TextElement):
                    self._word_count += len(node.value.split())
                return node

            self.root_node.traverse(count_words)

        return self._word_count

    def equals(self, other):
        return self.entry.equals(
            other.entry, ignored_fields=self.ignored_fields)

    # In Fluent we treat entries as a whole.  FluentChecker reports errors at
    # offsets calculated from the beginning of the entry.
    def value_position(self, offset=0):
        return self.position(offset)

    @property
    def attributes(self):
        for attr_node in self.entry.attributes:
            yield FluentAttribute(self, attr_node)


class FluentMessage(FluentEntity):
    pass


class FluentTerm(FluentEntity):
    # Fields ignored when comparing two terms.
    ignored_fields = ['attributes', 'comment', 'span']

    @property
    def root_node(self):
        '''AST node at which to start traversal for count_words.

        In Fluent Terms we only count words in the value. Attributes are
        private and do not count towards the word total.
        '''
        return self.entry.value


class FluentComment(Comment):
    def __init__(self, ctx, span, entry):
        super(FluentComment, self).__init__(ctx, span)
        self._val_cache = entry.content


class FluentParser(Parser):
    capabilities = CAN_SKIP

    def __init__(self):
        super(FluentParser, self).__init__()
        self.ftl_parser = FTLParser()

    def walk(self, only_localizable=False):
        if not self.ctx:
            # loading file failed, or we just didn't load anything
            return

        resource = self.ftl_parser.parse(self.ctx.contents)

        last_span_end = 0

        for entry in resource.body:
            if not only_localizable:
                if entry.span.start > last_span_end:
                    yield Whitespace(
                        self.ctx, (last_span_end, entry.span.start))

            if isinstance(entry, ftl.Message):
                yield FluentMessage(self.ctx, entry)
            elif isinstance(entry, ftl.Term):
                yield FluentTerm(self.ctx, entry)
            elif isinstance(entry, ftl.Junk):
                start = entry.span.start
                end = entry.span.end
                # strip leading whitespace
                start += re.match('\s*', entry.content).end()
                # strip trailing whitespace
                ws, we = re.search('\s*$', entry.content).span()
                end -= we - ws
                yield Junk(self.ctx, (start, end))
            elif isinstance(entry, ftl.BaseComment) and not only_localizable:
                span = (entry.span.start, entry.span.end)
                yield FluentComment(self.ctx, span, entry)

            last_span_end = entry.span.end

        # Yield Whitespace at the EOF.
        if not only_localizable:
            eof_offset = len(self.ctx.contents)
            if eof_offset > last_span_end:
                yield Whitespace(self.ctx, (last_span_end, eof_offset))


__constructors = [('\\.dtd$', DTDParser()),
                  ('\\.properties$', PropertiesParser()),
                  ('\\.ini$', IniParser()),
                  ('\\.inc$', DefinesParser()),
                  ('\\.ftl$', FluentParser())]
