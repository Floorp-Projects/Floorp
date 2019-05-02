# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Android strings.xml parser

Parses strings.xml files per
https://developer.android.com/guide/topics/resources/localization.
As we're using a built-in XML parser underneath, errors on that level
break the full parsing, and result in a single Junk entry.
"""

from __future__ import absolute_import
from __future__ import unicode_literals

import re
from xml.dom import minidom
from xml.dom.minidom import Node

from .base import (
    CAN_SKIP,
    Entity, Comment, Junk, Whitespace,
    StickyEntry, LiteralEntity,
    Parser
)


class AndroidEntity(Entity):
    def __init__(
        self, ctx, pre_comment, white_space, node, all, key, raw_val, val
    ):
        # fill out superclass as good as we can right now
        # most span can get modified at endElement
        super(AndroidEntity, self).__init__(
            ctx, pre_comment, white_space,
            (None, None),
            (None, None),
            (None, None)
        )
        self.node = node
        self._all_literal = all
        self._key_literal = key
        self._raw_val_literal = raw_val
        self._val_literal = val

    @property
    def all(self):
        chunks = []
        if self.pre_comment is not None:
            chunks.append(self.pre_comment.all)
        if self.inner_white is not None:
            chunks.append(self.inner_white.all)
        chunks.append(self._all_literal)
        return ''.join(chunks)

    @property
    def key(self):
        return self._key_literal

    @property
    def raw_val(self):
        return self._raw_val_literal

    def position(self, offset=0):
        return (0, offset)

    def value_position(self, offset=0):
        return (0, offset)

    def wrap(self, raw_val):
        clone = self.node.cloneNode(True)
        if clone.childNodes.length == 1:
            child = clone.childNodes[0]
        else:
            for child in clone.childNodes:
                if child.nodeType == Node.CDATA_SECTION_NODE:
                    break
        child.data = raw_val
        all = []
        if self.pre_comment is not None:
            all.append(self.pre_comment.all)
        if self.inner_white is not None:
            all.append(self.inner_white.all)
        all.append(clone.toxml())
        return LiteralEntity(self.key, raw_val, ''.join(all))


class NodeMixin(object):
    def __init__(self, all, value):
        self._all_literal = all
        self._val_literal = value

    @property
    def all(self):
        return self._all_literal

    @property
    def key(self):
        return self._all_literal

    @property
    def raw_val(self):
        return self._val_literal


class XMLWhitespace(NodeMixin, Whitespace):
    pass


class XMLComment(NodeMixin, Comment):
    @property
    def val(self):
        return self._val_literal

    @property
    def key(self):
        return None


# DocumentWrapper is sticky in serialization.
# Always keep the one from the reference document.
class DocumentWrapper(NodeMixin, StickyEntry):
    def __init__(self, key, all):
        self._all_literal = all
        self._val_literal = all
        self._key_literal = key

    @property
    def key(self):
        return self._key_literal


class XMLJunk(Junk):
    def __init__(self, all):
        super(XMLJunk, self).__init__(None, (0, 0))
        self._all_literal = all

    @property
    def all(self):
        return self._all_literal


def textContent(node):
    if node.childNodes.length == 0:
        return ''
    for child in node.childNodes:
        if child.nodeType == minidom.Node.CDATA_SECTION_NODE:
            return child.data
    if (
            node.childNodes.length != 1 or
            node.childNodes[0].nodeType != minidom.Node.TEXT_NODE
    ):
        # Return something, we'll fail in checks on this
        return node.toxml()
    return node.childNodes[0].data


NEWLINE = re.compile(r'[ \t]*\n[ \t]*')


def normalize(val):
    return NEWLINE.sub('\n', val.strip(' \t'))


class AndroidParser(Parser):
    # Android does l10n fallback at runtime, don't merge en-US strings
    capabilities = CAN_SKIP

    def __init__(self):
        super(AndroidParser, self).__init__()
        self.last_comment = None

    def walk(self, only_localizable=False):
        if not self.ctx:
            # loading file failed, or we just didn't load anything
            return
        ctx = self.ctx
        contents = ctx.contents
        try:
            doc = minidom.parseString(contents.encode('utf-8'))
        except Exception:
            yield XMLJunk(contents)
            return
        docElement = doc.documentElement
        if docElement.nodeName != 'resources':
            yield XMLJunk(doc.toxml())
            return
        root_children = docElement.childNodes
        if not only_localizable:
            yield DocumentWrapper(
                '<?xml?><resources>',
                '<?xml version="1.0" encoding="utf-8"?>\n<resources'
            )
            for attr_name, attr_value in docElement.attributes.items():
                yield DocumentWrapper(
                    attr_name,
                    ' {}="{}"'.format(attr_name, attr_value)
                )
            yield DocumentWrapper('>', '>')
        child_num = 0
        while child_num < len(root_children):
            node = root_children[child_num]
            if node.nodeType == Node.COMMENT_NODE:
                current_comment, child_num = self.handleComment(
                    node, root_children, child_num
                )
                if child_num < len(root_children):
                    node = root_children[child_num]
                else:
                    if not only_localizable:
                        yield current_comment
                    break
            else:
                current_comment = None
            if node.nodeType in (Node.TEXT_NODE, Node.CDATA_SECTION_NODE):
                white_space = XMLWhitespace(node.toxml(), node.nodeValue)
                child_num += 1
                if current_comment is None:
                    if not only_localizable:
                        yield white_space
                    continue
                if node.nodeValue.count('\n') > 1:
                    if not only_localizable:
                        if current_comment is not None:
                            yield current_comment
                        yield white_space
                    continue
                if child_num < len(root_children):
                    node = root_children[child_num]
                else:
                    if not only_localizable:
                        if current_comment is not None:
                            yield current_comment
                        yield white_space
                    break
            else:
                white_space = None
            if node.nodeType == Node.ELEMENT_NODE:
                yield self.handleElement(node, current_comment, white_space)
            else:
                if not only_localizable:
                    if current_comment:
                        yield current_comment
                    if white_space:
                        yield white_space
            child_num += 1
        if not only_localizable:
            yield DocumentWrapper('</resources>', '</resources>\n')

    def handleElement(self, element, current_comment, white_space):
        if element.nodeName == 'string' and element.hasAttribute('name'):
            return AndroidEntity(
                self.ctx,
                current_comment,
                white_space,
                element,
                element.toxml(),
                element.getAttribute('name'),
                textContent(element),
                ''.join(c.toxml() for c in element.childNodes)
            )
        else:
            return XMLJunk(element.toxml())

    def handleComment(self, node, root_children, child_num):
        all = node.toxml()
        val = normalize(node.nodeValue)
        while True:
            child_num += 1
            if child_num >= len(root_children):
                break
            node = root_children[child_num]
            if node.nodeType == Node.TEXT_NODE:
                if node.nodeValue.count('\n') > 1:
                    break
                white = node
                child_num += 1
                if child_num >= len(root_children):
                    break
                node = root_children[child_num]
            else:
                white = None
            if node.nodeType != Node.COMMENT_NODE:
                if white is not None:
                    # do not consume this node
                    child_num -= 1
                break
            if white:
                all += white.toxml()
                val += normalize(white.nodeValue)
            all += node.toxml()
            val += normalize(node.nodeValue)
        return XMLComment(all, val), child_num
