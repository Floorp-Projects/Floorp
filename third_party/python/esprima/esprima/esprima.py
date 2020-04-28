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

from .comment_handler import CommentHandler
from .error_handler import Error
from .jsx_parser import JSXParser
from .jsx_syntax import JSXSyntax
from .objects import Array, toDict
from .parser import Parser
from .syntax import Syntax
from .tokenizer import Tokenizer
from .visitor import NodeVisitor
from . import nodes
from . import jsx_nodes


__all__ = ['Syntax', 'JSXSyntax', 'Error', 'NodeVisitor', 'nodes', 'jsx_nodes',
           'parse', 'parseModule', 'parseScript', 'tokenize', 'toDict']


def parse(code, options=None, delegate=None, **kwargs):
    options = {} if options is None else options.copy()
    options.update(kwargs)

    # ESNext presset:
    if options.get('esnext', False):
        options['jsx'] = True
        options['classProperties'] = True

    commentHandler = None

    def proxyDelegate(node, metadata):
        if delegate:
            new_node = delegate(node, metadata)
            if new_node is not None:
                node = new_node
        if commentHandler:
            commentHandler.visit(node, metadata)
        return node

    parserDelegate = None if delegate is None else proxyDelegate
    collectComment = options.get('comment', False)
    attachComment = options.get('attachComment', False)
    if collectComment or attachComment:
        commentHandler = CommentHandler()
        commentHandler.attach = attachComment
        options['comment'] = True
        parserDelegate = proxyDelegate

    isModule = options.get('sourceType', 'script') == 'module'

    if options.get('jsx', False):
        parser = JSXParser(code, options=options, delegate=parserDelegate)
    else:
        parser = Parser(code, options=options, delegate=parserDelegate)

    ast = parser.parseModule() if isModule else parser.parseScript()

    if collectComment and commentHandler:
        ast.comments = commentHandler.comments

    if parser.config.tokens:
        ast.tokens = parser.tokens

    if parser.config.tolerant:
        ast.errors = parser.errorHandler.errors

    return ast


def parseModule(code, options=None, delegate=None, **kwargs):
    kwargs['sourceType'] = 'module'
    return parse(code, options, delegate, **kwargs)


def parseScript(code, options=None, delegate=None, **kwargs):
    kwargs['sourceType'] = 'script'
    return parse(code, options, delegate, **kwargs)


def tokenize(code, options=None, delegate=None, **kwargs):
    options = {} if options is None else options.copy()
    options.update(kwargs)

    tokenizer = Tokenizer(code, options)

    tokens = Array()

    try:
        while True:
            token = tokenizer.getNextToken()
            if not token:
                break
            if delegate:
                token = delegate(token)
            tokens.append(token)
    except Error as e:
        tokenizer.errorHandler.tolerate(e)

    if tokenizer.errorHandler.tolerant:
        tokens.errors = tokenizer.errors()

    return tokens
