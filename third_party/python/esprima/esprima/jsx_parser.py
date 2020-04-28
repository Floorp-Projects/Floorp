# -*- coding: utf-8 -*-
# Copyright JS Foundation and other contributors, https://js.foundation/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import absolute_import, unicode_literals

from .compat import uchr
from .character import Character
from . import jsx_nodes as JSXNode
from .jsx_syntax import JSXSyntax
from . import nodes as Node
from .parser import Marker, Parser
from .token import Token, TokenName
from .xhtml_entities import XHTMLEntities


class MetaJSXElement(object):
    def __init__(self, node=None, opening=None, closing=None, children=None):
        self.node = node
        self.opening = opening
        self.closing = closing
        self.children = children


class JSXToken(object):
    Identifier = 100
    Text = 101


class RawJSXToken(object):
    def __init__(self, type=None, value=None, lineNumber=None, lineStart=None, start=None, end=None):
        self.type = type
        self.value = value
        self.lineNumber = lineNumber
        self.lineStart = lineStart
        self.start = start
        self.end = end


TokenName[JSXToken.Identifier] = "JSXIdentifier"
TokenName[JSXToken.Text] = "JSXText"


# Fully qualified element name, e.g. <svg:path> returns "svg:path"
def getQualifiedElementName(elementName):
    typ = elementName.type
    if typ is JSXSyntax.JSXIdentifier:
        id = elementName
        qualifiedName = id.name
    elif typ is JSXSyntax.JSXNamespacedName:
        ns = elementName
        qualifiedName = getQualifiedElementName(ns.namespace) + ':' + getQualifiedElementName(ns.name)
    elif typ is JSXSyntax.JSXMemberExpression:
        expr = elementName
        qualifiedName = getQualifiedElementName(expr.object) + '.' + getQualifiedElementName(expr.property)

    return qualifiedName


class JSXParser(Parser):
    def __init__(self, code, options, delegate):
        super(JSXParser, self).__init__(code, options, delegate)

    def parsePrimaryExpression(self):
        return self.parseJSXRoot() if self.match('<') else super(JSXParser, self).parsePrimaryExpression()

    def startJSX(self):
        # Unwind the scanner before the lookahead token.
        self.scanner.index = self.startMarker.index
        self.scanner.lineNumber = self.startMarker.line
        self.scanner.lineStart = self.startMarker.index - self.startMarker.column

    def finishJSX(self):
        # Prime the next lookahead.
        self.nextToken()

    def reenterJSX(self):
        self.startJSX()
        self.expectJSX('}')

        # Pop the closing '}' added from the lookahead.
        if self.config.tokens:
            self.tokens.pop()

    def createJSXNode(self):
        self.collectComments()
        return Marker(
            index=self.scanner.index,
            line=self.scanner.lineNumber,
            column=self.scanner.index - self.scanner.lineStart
        )

    def createJSXChildNode(self):
        return Marker(
            index=self.scanner.index,
            line=self.scanner.lineNumber,
            column=self.scanner.index - self.scanner.lineStart
        )

    def scanXHTMLEntity(self, quote):
        result = '&'

        valid = True
        terminated = False
        numeric = False
        hex = False

        while not self.scanner.eof() and valid and not terminated:
            ch = self.scanner.source[self.scanner.index]
            if ch == quote:
                break

            terminated = (ch == ';')
            result += ch
            self.scanner.index += 1
            if not terminated:
                length = len(result)
                if length == 2:
                    # e.g. '&#123;'
                    numeric = (ch == '#')
                elif length == 3:
                    if numeric:
                        # e.g. '&#x41;'
                        hex = ch == 'x'
                        valid = hex or Character.isDecimalDigit(ch)
                        numeric = numeric and not hex
                else:
                    valid = valid and not (numeric and not Character.isDecimalDigit(ch))
                    valid = valid and not (hex and not Character.isHexDigit(ch))

        if valid and terminated and len(result) > 2:
            # e.g. '&#x41;' becomes just '#x41'
            st = result[1:-1]
            if numeric and len(st) > 1:
                result = uchr(int(st[1:], 10))
            elif hex and len(st) > 2:
                result = uchr(int(st[2:], 16))
            elif not numeric and not hex and st in XHTMLEntities:
                result = XHTMLEntities[st]

        return result

    # Scan the next JSX token. This replaces Scanner#lex when in JSX mode.

    def lexJSX(self):
        ch = self.scanner.source[self.scanner.index]

        # < > / : = { }
        if ch in ('<', '>', '/', ':', '=', '{', '}'):
            value = self.scanner.source[self.scanner.index]
            self.scanner.index += 1
            return RawJSXToken(
                type=Token.Punctuator,
                value=value,
                lineNumber=self.scanner.lineNumber,
                lineStart=self.scanner.lineStart,
                start=self.scanner.index - 1,
                end=self.scanner.index
            )

        # " '
        if ch in ('\'', '"'):
            start = self.scanner.index
            quote = self.scanner.source[self.scanner.index]
            self.scanner.index += 1
            str = ''
            while not self.scanner.eof():
                ch = self.scanner.source[self.scanner.index]
                self.scanner.index += 1
                if ch == quote:
                    break
                elif ch == '&':
                    str += self.scanXHTMLEntity(quote)
                else:
                    str += ch

            return RawJSXToken(
                type=Token.StringLiteral,
                value=str,
                lineNumber=self.scanner.lineNumber,
                lineStart=self.scanner.lineStart,
                start=start,
                end=self.scanner.index
            )

        # ... or .
        if ch == '.':
            start = self.scanner.index
            if self.scanner.source[start + 1:start + 3] == '..':
                value = '...'
                self.scanner.index += 3
            else:
                value = '.'
                self.scanner.index += 1
            return RawJSXToken(
                type=Token.Punctuator,
                value=value,
                lineNumber=self.scanner.lineNumber,
                lineStart=self.scanner.lineStart,
                start=start,
                end=self.scanner.index
            )

        # `
        if ch == '`':
            # Only placeholder, since it will be rescanned as a real assignment expression.
            return RawJSXToken(
                type=Token.Template,
                value='',
                lineNumber=self.scanner.lineNumber,
                lineStart=self.scanner.lineStart,
                start=self.scanner.index,
                end=self.scanner.index
            )

        # Identifer can not contain backslash (char code 92).
        if Character.isIdentifierStart(ch) and ch != '\\':
            start = self.scanner.index
            self.scanner.index += 1
            while not self.scanner.eof():
                ch = self.scanner.source[self.scanner.index]
                if Character.isIdentifierPart(ch) and ch != '\\':
                    self.scanner.index += 1
                elif ch == '-':
                    # Hyphen (char code 45) can be part of an identifier.
                    self.scanner.index += 1
                else:
                    break

            id = self.scanner.source[start:self.scanner.index]
            return RawJSXToken(
                type=JSXToken.Identifier,
                value=id,
                lineNumber=self.scanner.lineNumber,
                lineStart=self.scanner.lineStart,
                start=start,
                end=self.scanner.index
            )

        return self.scanner.lex()

    def nextJSXToken(self):
        self.collectComments()

        self.startMarker.index = self.scanner.index
        self.startMarker.line = self.scanner.lineNumber
        self.startMarker.column = self.scanner.index - self.scanner.lineStart
        token = self.lexJSX()
        self.lastMarker.index = self.scanner.index
        self.lastMarker.line = self.scanner.lineNumber
        self.lastMarker.column = self.scanner.index - self.scanner.lineStart

        if self.config.tokens:
            self.tokens.append(self.convertToken(token))

        return token

    def nextJSXText(self):
        self.startMarker.index = self.scanner.index
        self.startMarker.line = self.scanner.lineNumber
        self.startMarker.column = self.scanner.index - self.scanner.lineStart

        start = self.scanner.index

        text = ''
        while not self.scanner.eof():
            ch = self.scanner.source[self.scanner.index]
            if ch in ('{', '<'):
                break

            self.scanner.index += 1
            text += ch
            if Character.isLineTerminator(ch):
                self.scanner.lineNumber += 1
                if ch == '\r' and self.scanner.source[self.scanner.index] == '\n':
                    self.scanner.index += 1

                self.scanner.lineStart = self.scanner.index

        self.lastMarker.index = self.scanner.index
        self.lastMarker.line = self.scanner.lineNumber
        self.lastMarker.column = self.scanner.index - self.scanner.lineStart

        token = RawJSXToken(
            type=JSXToken.Text,
            value=text,
            lineNumber=self.scanner.lineNumber,
            lineStart=self.scanner.lineStart,
            start=start,
            end=self.scanner.index
        )

        if text and self.config.tokens:
            self.tokens.append(self.convertToken(token))

        return token

    def peekJSXToken(self):
        state = self.scanner.saveState()
        self.scanner.scanComments()
        next = self.lexJSX()
        self.scanner.restoreState(state)

        return next

    # Expect the next JSX token to match the specified punctuator.
    # If not, an exception will be thrown.

    def expectJSX(self, value):
        token = self.nextJSXToken()
        if token.type is not Token.Punctuator or token.value != value:
            self.throwUnexpectedToken(token)

    # Return True if the next JSX token matches the specified punctuator.

    def matchJSX(self, *value):
        next = self.peekJSXToken()
        return next.type is Token.Punctuator and next.value in value

    def parseJSXIdentifier(self):
        node = self.createJSXNode()
        token = self.nextJSXToken()
        if token.type is not JSXToken.Identifier:
            self.throwUnexpectedToken(token)

        return self.finalize(node, JSXNode.JSXIdentifier(token.value))

    def parseJSXElementName(self):
        node = self.createJSXNode()
        elementName = self.parseJSXIdentifier()

        if self.matchJSX(':'):
            namespace = elementName
            self.expectJSX(':')
            name = self.parseJSXIdentifier()
            elementName = self.finalize(node, JSXNode.JSXNamespacedName(namespace, name))
        elif self.matchJSX('.'):
            while self.matchJSX('.'):
                object = elementName
                self.expectJSX('.')
                property = self.parseJSXIdentifier()
                elementName = self.finalize(node, JSXNode.JSXMemberExpression(object, property))

        return elementName

    def parseJSXAttributeName(self):
        node = self.createJSXNode()

        identifier = self.parseJSXIdentifier()
        if self.matchJSX(':'):
            namespace = identifier
            self.expectJSX(':')
            name = self.parseJSXIdentifier()
            attributeName = self.finalize(node, JSXNode.JSXNamespacedName(namespace, name))
        else:
            attributeName = identifier

        return attributeName

    def parseJSXStringLiteralAttribute(self):
        node = self.createJSXNode()
        token = self.nextJSXToken()
        if token.type is not Token.StringLiteral:
            self.throwUnexpectedToken(token)

        raw = self.getTokenRaw(token)
        return self.finalize(node, Node.Literal(token.value, raw))

    def parseJSXExpressionAttribute(self):
        node = self.createJSXNode()

        self.expectJSX('{')
        self.finishJSX()

        if self.match('}'):
            self.tolerateError('JSX attributes must only be assigned a non-empty expression')

        expression = self.parseAssignmentExpression()
        self.reenterJSX()

        return self.finalize(node, JSXNode.JSXExpressionContainer(expression))

    def parseJSXAttributeValue(self):
        if self.matchJSX('{'):
            return self.parseJSXExpressionAttribute()
        if self.matchJSX('<'):
            return self.parseJSXElement()

        return self.parseJSXStringLiteralAttribute()

    def parseJSXNameValueAttribute(self):
        node = self.createJSXNode()
        name = self.parseJSXAttributeName()
        value = None
        if self.matchJSX('='):
            self.expectJSX('=')
            value = self.parseJSXAttributeValue()

        return self.finalize(node, JSXNode.JSXAttribute(name, value))

    def parseJSXSpreadAttribute(self):
        node = self.createJSXNode()
        self.expectJSX('{')
        self.expectJSX('...')

        self.finishJSX()
        argument = self.parseAssignmentExpression()
        self.reenterJSX()

        return self.finalize(node, JSXNode.JSXSpreadAttribute(argument))

    def parseJSXAttributes(self):
        attributes = []

        while not self.matchJSX('/', '>'):
            attribute = self.parseJSXSpreadAttribute() if self.matchJSX('{') else self.parseJSXNameValueAttribute()
            attributes.append(attribute)

        return attributes

    def parseJSXOpeningElement(self):
        node = self.createJSXNode()

        self.expectJSX('<')
        name = self.parseJSXElementName()
        attributes = self.parseJSXAttributes()
        selfClosing = self.matchJSX('/')
        if selfClosing:
            self.expectJSX('/')

        self.expectJSX('>')

        return self.finalize(node, JSXNode.JSXOpeningElement(name, selfClosing, attributes))

    def parseJSXBoundaryElement(self):
        node = self.createJSXNode()

        self.expectJSX('<')
        if self.matchJSX('/'):
            self.expectJSX('/')
            elementName = self.parseJSXElementName()
            self.expectJSX('>')
            return self.finalize(node, JSXNode.JSXClosingElement(elementName))

        name = self.parseJSXElementName()
        attributes = self.parseJSXAttributes()
        selfClosing = self.matchJSX('/')
        if selfClosing:
            self.expectJSX('/')

        self.expectJSX('>')

        return self.finalize(node, JSXNode.JSXOpeningElement(name, selfClosing, attributes))

    def parseJSXEmptyExpression(self):
        node = self.createJSXChildNode()
        self.collectComments()
        self.lastMarker.index = self.scanner.index
        self.lastMarker.line = self.scanner.lineNumber
        self.lastMarker.column = self.scanner.index - self.scanner.lineStart
        return self.finalize(node, JSXNode.JSXEmptyExpression())

    def parseJSXExpressionContainer(self):
        node = self.createJSXNode()
        self.expectJSX('{')

        if self.matchJSX('}'):
            expression = self.parseJSXEmptyExpression()
            self.expectJSX('}')
        else:
            self.finishJSX()
            expression = self.parseAssignmentExpression()
            self.reenterJSX()

        return self.finalize(node, JSXNode.JSXExpressionContainer(expression))

    def parseJSXChildren(self):
        children = []

        while not self.scanner.eof():
            node = self.createJSXChildNode()
            token = self.nextJSXText()
            if token.start < token.end:
                raw = self.getTokenRaw(token)
                child = self.finalize(node, JSXNode.JSXText(token.value, raw))
                children.append(child)

            if self.scanner.source[self.scanner.index] == '{':
                container = self.parseJSXExpressionContainer()
                children.append(container)
            else:
                break

        return children

    def parseComplexJSXElement(self, el):
        stack = []

        while not self.scanner.eof():
            el.children.extend(self.parseJSXChildren())
            node = self.createJSXChildNode()
            element = self.parseJSXBoundaryElement()
            if element.type is JSXSyntax.JSXOpeningElement:
                opening = element
                if opening.selfClosing:
                    child = self.finalize(node, JSXNode.JSXElement(opening, [], None))
                    el.children.append(child)
                else:
                    stack.append(el)
                    el = MetaJSXElement(
                        node=node,
                        opening=opening,
                        closing=None,
                        children=[],
                    )

            if element.type is JSXSyntax.JSXClosingElement:
                el.closing = element
                open = getQualifiedElementName(el.opening.name)
                close = getQualifiedElementName(el.closing.name)
                if open != close:
                    self.tolerateError('Expected corresponding JSX closing tag for %0', open)

                if stack:
                    child = self.finalize(el.node, JSXNode.JSXElement(el.opening, el.children, el.closing))
                    el = stack[-1]
                    el.children.append(child)
                    stack.pop()
                else:
                    break

        return el

    def parseJSXElement(self):
        node = self.createJSXNode()

        opening = self.parseJSXOpeningElement()
        children = []
        closing = None

        if not opening.selfClosing:
            el = self.parseComplexJSXElement(MetaJSXElement(
                node=node,
                opening=opening,
                closing=closing,
                children=children
            ))
            children = el.children
            closing = el.closing

        return self.finalize(node, JSXNode.JSXElement(opening, children, closing))

    def parseJSXRoot(self):
        # Pop the opening '<' added from the lookahead.
        if self.config.tokens:
            self.tokens.pop()

        self.startJSX()
        element = self.parseJSXElement()
        self.finishJSX()

        return element

    def isStartOfExpression(self):
        return super(JSXParser, self).isStartOfExpression() or self.match('<')
