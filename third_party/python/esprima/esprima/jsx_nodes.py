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

from .nodes import Node
from .jsx_syntax import JSXSyntax


class JSXClosingElement(Node):
    def __init__(self, name):
        self.type = JSXSyntax.JSXClosingElement
        self.name = name


class JSXElement(Node):
    def __init__(self, openingElement, children, closingElement):
        self.type = JSXSyntax.JSXElement
        self.openingElement = openingElement
        self.children = children
        self.closingElement = closingElement


class JSXEmptyExpression(Node):
    def __init__(self):
        self.type = JSXSyntax.JSXEmptyExpression


class JSXExpressionContainer(Node):
    def __init__(self, expression):
        self.type = JSXSyntax.JSXExpressionContainer
        self.expression = expression


class JSXIdentifier(Node):
    def __init__(self, name):
        self.type = JSXSyntax.JSXIdentifier
        self.name = name


class JSXMemberExpression(Node):
    def __init__(self, object, property):
        self.type = JSXSyntax.JSXMemberExpression
        self.object = object
        self.property = property


class JSXAttribute(Node):
    def __init__(self, name, value):
        self.type = JSXSyntax.JSXAttribute
        self.name = name
        self.value = value


class JSXNamespacedName(Node):
    def __init__(self, namespace, name):
        self.type = JSXSyntax.JSXNamespacedName
        self.namespace = namespace
        self.name = name


class JSXOpeningElement(Node):
    def __init__(self, name, selfClosing, attributes):
        self.type = JSXSyntax.JSXOpeningElement
        self.name = name
        self.selfClosing = selfClosing
        self.attributes = attributes


class JSXSpreadAttribute(Node):
    def __init__(self, argument):
        self.type = JSXSyntax.JSXSpreadAttribute
        self.argument = argument


class JSXText(Node):
    def __init__(self, value, raw):
        self.type = JSXSyntax.JSXText
        self.value = value
        self.raw = raw
