# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re

try:
    from html import unescape as html_unescape
except ImportError:
    from HTMLParser import HTMLParser
    html_parser = HTMLParser()
    html_unescape = html_parser.unescape

from .base import (
    Entity, Comment, Junk,
    Parser
)


class DTDEntityMixin(object):
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
            line, col = super(DTDEntityMixin, self).value_position()
            if line_pos == 1:
                col = col + col_pos
            else:
                col = col_pos
                line += line_pos - 1
            return line, col
        else:
            return super(DTDEntityMixin, self).value_position(offset)


class DTDEntity(DTDEntityMixin, Entity):
    pass


class DTDParser(Parser):
    # http://www.w3.org/TR/2006/REC-xml11-20060816/#NT-NameStartChar
    # ":" | [A-Z] | "_" | [a-z] |
    # [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] | [#x370-#x37D] | [#x37F-#x1FFF]
    # | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] |
    # [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] |
    # [#x10000-#xEFFFF]
    CharMinusDash = '\x09\x0A\x0D\u0020-\u002C\u002E-\uD7FF\uE000-\uFFFD'
    XmlComment = '<!--(?:-?[%s])*?-->' % CharMinusDash
    NameStartChar = ':A-Z_a-z\xC0-\xD6\xD8-\xF6\xF8-\u02FF' + \
        '\u0370-\u037D\u037F-\u1FFF\u200C-\u200D\u2070-\u218F' + \
        '\u2C00-\u2FEF\u3001-\uD7FF\uF900-\uFDCF\uFDF0-\uFFFD'
    # + \U00010000-\U000EFFFF seems to be unsupported in python

    # NameChar ::= NameStartChar | "-" | "." | [0-9] | #xB7 |
    #     [#x0300-#x036F] | [#x203F-#x2040]
    NameChar = NameStartChar + r'\-\.0-9' + '\xB7\u0300-\u036F\u203F-\u2040'
    Name = '[' + NameStartChar + '][' + NameChar + ']*'
    reKey = re.compile('<!ENTITY[ \t\r\n]+(?P<key>' + Name + ')[ \t\r\n]+'
                       '(?P<val>\"[^\"]*\"|\'[^\']*\'?)[ \t\r\n]*>',
                       re.DOTALL | re.M)
    # add BOM to DTDs, details in bug 435002
    reHeader = re.compile('^\ufeff')
    reComment = re.compile('<!--(?P<val>-?[%s])*?-->' % CharMinusDash,
                           re.S)
    rePE = re.compile('<!ENTITY[ \t\r\n]+%[ \t\r\n]+(?P<key>' + Name + ')'
                      '[ \t\r\n]+SYSTEM[ \t\r\n]+'
                      '(?P<val>\"[^\"]*\"|\'[^\']*\')[ \t\r\n]*>[ \t\r\n]*'
                      '%' + Name + ';'
                      '(?:[ \t]*(?:' + XmlComment + u'[ \t\r\n]*)*\n?)?')

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
        if offset == 0 and self.reHeader.match(ctx.contents):
            offset += 1
        entity = Parser.getNext(self, ctx, offset)
        if (entity and isinstance(entity, Junk)) or entity is None:
            m = self.rePE.match(ctx.contents, offset)
            if m:
                entity = DTDEntity(
                    ctx, None, None, m.span(), m.span('key'), m.span('val'))
        return entity

    def createEntity(self, ctx, m, current_comment, white_space):
        valspan = m.span('val')
        valspan = (valspan[0]+1, valspan[1]-1)
        return DTDEntity(ctx, current_comment, white_space,
                         m.span(), m.span('key'), valspan)
