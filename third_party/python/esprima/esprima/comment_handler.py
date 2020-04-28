# -*- coding: utf-8 -*-
# Copyright JS Foundation and other contributors, https://js.foundation/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, self.list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, self.list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# self.SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES
# LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# self.SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import absolute_import, unicode_literals

from .objects import Object
from .nodes import Node
from .syntax import Syntax


class Comment(Node):
    def __init__(self, type, value, range=None, loc=None):
        self.type = type
        self.value = value
        self.range = range
        self.loc = loc


class Entry(Object):
    def __init__(self, comment, start):
        self.comment = comment
        self.start = start


class NodeInfo(Object):
    def __init__(self, node, start):
        self.node = node
        self.start = start


class CommentHandler(object):
    def __init__(self):
        self.attach = False
        self.comments = []
        self.stack = []
        self.leading = []
        self.trailing = []

    def insertInnerComments(self, node, metadata):
        #  innnerComments for properties empty block
        #  `function a(:/** comments **\/}`
        if node.type is Syntax.BlockStatement and not node.body:
            innerComments = []
            for i, entry in enumerate(self.leading):
                if metadata.end.offset >= entry.start:
                    innerComments.append(entry.comment)
                    self.leading[i] = None
                    self.trailing[i] = None
            if innerComments:
                node.innerComments = innerComments
                self.leading = [v for v in self.leading if v is not None]
                self.trailing = [v for v in self.trailing if v is not None]

    def findTrailingComments(self, metadata):
        trailingComments = []

        if self.trailing:
            for i, entry in enumerate(self.trailing):
                if entry.start >= metadata.end.offset:
                    trailingComments.append(entry.comment)
            if trailingComments:
                self.trailing = []
            return trailingComments

        last = self.stack and self.stack[-1]
        if last and last.node.trailingComments:
            firstComment = last.node.trailingComments[0]
            if firstComment and firstComment.range[0] >= metadata.end.offset:
                trailingComments = last.node.trailingComments
                del last.node.trailingComments
        return trailingComments

    def findLeadingComments(self, metadata):
        leadingComments = []

        target = None
        while self.stack:
            entry = self.stack and self.stack[-1]
            if entry and entry.start >= metadata.start.offset:
                target = entry.node
                self.stack.pop()
            else:
                break

        if target:
            if target.leadingComments:
                for i, comment in enumerate(target.leadingComments):
                    if comment.range[1] <= metadata.start.offset:
                        leadingComments.append(comment)
                        target.leadingComments[i] = None
            if leadingComments:
                target.leadingComments = [v for v in target.leadingComments if v is not None]
                if not target.leadingComments:
                    del target.leadingComments
            return leadingComments

        for i, entry in enumerate(self.leading):
            if entry.start <= metadata.start.offset:
                leadingComments.append(entry.comment)
                self.leading[i] = None
        if leadingComments:
            self.leading = [v for v in self.leading if v is not None]

        return leadingComments

    def visitNode(self, node, metadata):
        if node.type is Syntax.Program and node.body:
            return

        self.insertInnerComments(node, metadata)
        trailingComments = self.findTrailingComments(metadata)
        leadingComments = self.findLeadingComments(metadata)
        if leadingComments:
            node.leadingComments = leadingComments
        if trailingComments:
            node.trailingComments = trailingComments

        self.stack.append(NodeInfo(
            node=node,
            start=metadata.start.offset
        ))

    def visitComment(self, node, metadata):
        type = 'Line' if node.type[0] == 'L' else 'Block'
        comment = Comment(
            type=type,
            value=node.value
        )
        if node.range:
            comment.range = node.range
        if node.loc:
            comment.loc = node.loc
        self.comments.append(comment)

        if self.attach:
            entry = Entry(
                comment=Comment(
                    type=type,
                    value=node.value,
                    range=[metadata.start.offset, metadata.end.offset]
                ),
                start=metadata.start.offset
            )
            if node.loc:
                entry.comment.loc = node.loc
            node.type = type
            self.leading.append(entry)
            self.trailing.append(entry)

    def visit(self, node, metadata):
        if node.type == 'LineComment':
            self.visitComment(node, metadata)
        elif node.type == 'BlockComment':
            self.visitComment(node, metadata)
        elif self.attach:
            self.visitNode(node, metadata)
