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

from xml.dom import minidom
from xml.dom.minidom import Node

from .base import (
    CAN_SKIP,
    EntityBase, Entity, Comment, Junk, Whitespace,
    Parser
)


class AndroidEntity(Entity):
    def __init__(self, ctx, pre_comment, node, all, key, raw_val, val):
        # fill out superclass as good as we can right now
        # most span can get modified at endElement
        super(AndroidEntity, self).__init__(
            ctx, pre_comment,
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
        return self._all_literal

    @property
    def key(self):
        return self._key_literal

    @property
    def raw_val(self):
        return self._raw_val_literal

    def value_position(self, offset=0):
        return (0, offset)


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


class DocumentWrapper(NodeMixin, EntityBase):
    def __init__(self, all):
        self._all_literal = all
        self._val_literal = all


class XMLJunk(Junk):
    def __init__(self, all):
        super(XMLJunk, self).__init__(None, (0, 0))
        self._all_literal = all

    @property
    def all(self):
        return self._all_literal


def textContent(node):
    if (
            node.nodeType == minidom.Node.TEXT_NODE or
            node.nodeType == minidom.Node.CDATA_SECTION_NODE
    ):
        return node.nodeValue
    return ''.join(textContent(child) for child in node.childNodes)


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
        if doc.documentElement.nodeName != 'resources':
            yield XMLJunk(doc.toxml())
            return
        root_children = doc.documentElement.childNodes
        if not only_localizable:
            yield DocumentWrapper(
                '<?xml version="1.0" encoding="utf-8"?>\n<resources>'
            )
        for node in root_children:
            if node.nodeType == Node.ELEMENT_NODE:
                yield self.handleElement(node)
                self.last_comment = None
            if node.nodeType in (Node.TEXT_NODE, Node.CDATA_SECTION_NODE):
                if not only_localizable:
                    yield XMLWhitespace(node.toxml(), node.nodeValue)
            if node.nodeType == Node.COMMENT_NODE:
                self.last_comment = XMLComment(node.toxml(), node.nodeValue)
                if not only_localizable:
                    yield self.last_comment
        if not only_localizable:
            yield DocumentWrapper('</resources>\n')

    def handleElement(self, element):
        if element.nodeName == 'string' and element.hasAttribute('name'):
            return AndroidEntity(
                self.ctx,
                self.last_comment,
                element,
                element.toxml(),
                element.getAttribute('name'),
                textContent(element),
                ''.join(c.toxml() for c in element.childNodes)
            )
        else:
            return XMLJunk(element.toxml())
