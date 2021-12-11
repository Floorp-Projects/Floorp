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

from .objects import Object
from .compat import basestring, unicode
from .utils import format
from .error_handler import ErrorHandler
from .messages import Messages
from .scanner import RawToken, Scanner, SourceLocation, Position, RegExp
from .token import Token, TokenName
from .syntax import Syntax
from . import nodes as Node


class Value(object):
    def __init__(self, value):
        self.value = value


class Params(object):
    def __init__(self, simple=None, message=None, stricted=None, firstRestricted=None, inFor=None, paramSet=None, params=None, get=None):
        self.simple = simple
        self.message = message
        self.stricted = stricted
        self.firstRestricted = firstRestricted
        self.inFor = inFor
        self.paramSet = paramSet
        self.params = params
        self.get = get


class Config(Object):
    def __init__(self, range=False, loc=False, source=None, tokens=False, comment=False, tolerant=False, **options):
        self.range = range
        self.loc = loc
        self.source = source
        self.tokens = tokens
        self.comment = comment
        self.tolerant = tolerant
        for k, v in options.items():
            setattr(self, k, v)


class Context(object):
    def __init__(self, isModule=False, allowAwait=False, allowIn=True, allowStrictDirective=True, allowYield=True, firstCoverInitializedNameError=None, isAssignmentTarget=False, isBindingElement=False, inFunctionBody=False, inIteration=False, inSwitch=False, labelSet=None, strict=False):
        self.isModule = isModule
        self.allowAwait = allowAwait
        self.allowIn = allowIn
        self.allowStrictDirective = allowStrictDirective
        self.allowYield = allowYield
        self.firstCoverInitializedNameError = firstCoverInitializedNameError
        self.isAssignmentTarget = isAssignmentTarget
        self.isBindingElement = isBindingElement
        self.inFunctionBody = inFunctionBody
        self.inIteration = inIteration
        self.inSwitch = inSwitch
        self.labelSet = {} if labelSet is None else labelSet
        self.strict = strict


class Marker(object):
    def __init__(self, index=None, line=None, column=None):
        self.index = index
        self.line = line
        self.column = column


class TokenEntry(Object):
    def __init__(self, type=None, value=None, regex=None, range=None, loc=None):
        self.type = type
        self.value = value
        self.regex = regex
        self.range = range
        self.loc = loc


class Parser(object):
    def __init__(self, code, options={}, delegate=None):
        self.config = Config(**options)

        self.delegate = delegate

        self.errorHandler = ErrorHandler()
        self.errorHandler.tolerant = self.config.tolerant
        self.scanner = Scanner(code, self.errorHandler)
        self.scanner.trackComment = self.config.comment

        self.operatorPrecedence = {
            '||': 1,
            '&&': 2,
            '|': 3,
            '^': 4,
            '&': 5,
            '==': 6,
            '!=': 6,
            '===': 6,
            '!==': 6,
            '<': 7,
            '>': 7,
            '<=': 7,
            '>=': 7,
            'instanceof': 7,
            'in': 7,
            '<<': 8,
            '>>': 8,
            '>>>': 8,
            '+': 9,
            '-': 9,
            '*': 11,
            '/': 11,
            '%': 11,
        }

        self.lookahead = RawToken(
            type=Token.EOF,
            value='',
            lineNumber=self.scanner.lineNumber,
            lineStart=0,
            start=0,
            end=0
        )
        self.hasLineTerminator = False

        self.context = Context(
            isModule=False,
            allowAwait=False,
            allowIn=True,
            allowStrictDirective=True,
            allowYield=True,
            firstCoverInitializedNameError=None,
            isAssignmentTarget=False,
            isBindingElement=False,
            inFunctionBody=False,
            inIteration=False,
            inSwitch=False,
            labelSet={},
            strict=False
        )
        self.tokens = []

        self.startMarker = Marker(
            index=0,
            line=self.scanner.lineNumber,
            column=0
        )
        self.lastMarker = Marker(
            index=0,
            line=self.scanner.lineNumber,
            column=0
        )
        self.nextToken()
        self.lastMarker = Marker(
            index=self.scanner.index,
            line=self.scanner.lineNumber,
            column=self.scanner.index - self.scanner.lineStart
        )

    def throwError(self, messageFormat, *args):
        msg = format(messageFormat, *args)
        index = self.lastMarker.index
        line = self.lastMarker.line
        column = self.lastMarker.column + 1
        raise self.errorHandler.createError(index, line, column, msg)

    def tolerateError(self, messageFormat, *args):
        msg = format(messageFormat, *args)
        index = self.lastMarker.index
        line = self.scanner.lineNumber
        column = self.lastMarker.column + 1
        self.errorHandler.tolerateError(index, line, column, msg)

    # Throw an exception because of the token.

    def unexpectedTokenError(self, token=None, message=None):
        msg = message or Messages.UnexpectedToken
        if token:
            if not message:
                typ = token.type
                if typ is Token.EOF:
                    msg = Messages.UnexpectedEOS
                elif typ is Token.Identifier:
                    msg = Messages.UnexpectedIdentifier
                elif typ is Token.NumericLiteral:
                    msg = Messages.UnexpectedNumber
                elif typ is Token.StringLiteral:
                    msg = Messages.UnexpectedString
                elif typ is Token.Template:
                    msg = Messages.UnexpectedTemplate
                elif typ is Token.Keyword:
                    if self.scanner.isFutureReservedWord(token.value):
                        msg = Messages.UnexpectedReserved
                    elif self.context.strict and self.scanner.isStrictModeReservedWord(token.value):
                        msg = Messages.StrictReservedWord
                else:
                    msg = Messages.UnexpectedToken
            value = token.value
        else:
            value = 'ILLEGAL'

        msg = msg.replace('%0', unicode(value), 1)

        if token and isinstance(token.lineNumber, int):
            index = token.start
            line = token.lineNumber
            lastMarkerLineStart = self.lastMarker.index - self.lastMarker.column
            column = token.start - lastMarkerLineStart + 1
            return self.errorHandler.createError(index, line, column, msg)
        else:
            index = self.lastMarker.index
            line = self.lastMarker.line
            column = self.lastMarker.column + 1
            return self.errorHandler.createError(index, line, column, msg)

    def throwUnexpectedToken(self, token=None, message=None):
        raise self.unexpectedTokenError(token, message)

    def tolerateUnexpectedToken(self, token=None, message=None):
        self.errorHandler.tolerate(self.unexpectedTokenError(token, message))

    def collectComments(self):
        if not self.config.comment:
            self.scanner.scanComments()
        else:
            comments = self.scanner.scanComments()
            if comments:
                for e in comments:
                    if e.multiLine:
                        node = Node.BlockComment(self.scanner.source[e.slice[0]:e.slice[1]])
                    else:
                        node = Node.LineComment(self.scanner.source[e.slice[0]:e.slice[1]])
                    if self.config.range:
                        node.range = e.range
                    if self.config.loc:
                        node.loc = e.loc
                    if self.delegate:
                        metadata = SourceLocation(
                            start=Position(
                                line=e.loc.start.line,
                                column=e.loc.start.column,
                                offset=e.range[0],
                            ),
                            end=Position(
                                line=e.loc.end.line,
                                column=e.loc.end.column,
                                offset=e.range[1],
                            )
                        )
                        new_node = self.delegate(node, metadata)
                        if new_node is not None:
                            node = new_node

    # From internal representation to an external structure

    def getTokenRaw(self, token):
        return self.scanner.source[token.start:token.end]

    def convertToken(self, token):
        t = TokenEntry(
            type=TokenName[token.type],
            value=self.getTokenRaw(token),
        )
        if self.config.range:
            t.range = [token.start, token.end]
        if self.config.loc:
            t.loc = SourceLocation(
                start=Position(
                    line=self.startMarker.line,
                    column=self.startMarker.column,
                ),
                end=Position(
                    line=self.scanner.lineNumber,
                    column=self.scanner.index - self.scanner.lineStart,
                ),
            )
        if token.type is Token.RegularExpression:
            t.regex = RegExp(
                pattern=token.pattern,
                flags=token.flags,
            )

        return t

    def nextToken(self):
        token = self.lookahead

        self.lastMarker.index = self.scanner.index
        self.lastMarker.line = self.scanner.lineNumber
        self.lastMarker.column = self.scanner.index - self.scanner.lineStart

        self.collectComments()

        if self.scanner.index != self.startMarker.index:
            self.startMarker.index = self.scanner.index
            self.startMarker.line = self.scanner.lineNumber
            self.startMarker.column = self.scanner.index - self.scanner.lineStart

        next = self.scanner.lex()
        self.hasLineTerminator = token.lineNumber != next.lineNumber

        if next and self.context.strict and next.type is Token.Identifier:
            if self.scanner.isStrictModeReservedWord(next.value):
                next.type = Token.Keyword
        self.lookahead = next

        if self.config.tokens and next.type is not Token.EOF:
            self.tokens.append(self.convertToken(next))

        return token

    def nextRegexToken(self):
        self.collectComments()

        token = self.scanner.scanRegExp()
        if self.config.tokens:
            # Pop the previous token, '/' or '/='
            # self is added from the lookahead token.
            self.tokens.pop()

            self.tokens.append(self.convertToken(token))

        # Prime the next lookahead.
        self.lookahead = token
        self.nextToken()

        return token

    def createNode(self):
        return Marker(
            index=self.startMarker.index,
            line=self.startMarker.line,
            column=self.startMarker.column,
        )

    def startNode(self, token, lastLineStart=0):
        column = token.start - token.lineStart
        line = token.lineNumber
        if column < 0:
            column += lastLineStart
            line -= 1

        return Marker(
            index=token.start,
            line=line,
            column=column,
        )

    def finalize(self, marker, node):
        if self.config.range:
            node.range = [marker.index, self.lastMarker.index]

        if self.config.loc:
            node.loc = SourceLocation(
                start=Position(
                    line=marker.line,
                    column=marker.column,
                ),
                end=Position(
                    line=self.lastMarker.line,
                    column=self.lastMarker.column,
                ),
            )
            if self.config.source:
                node.loc.source = self.config.source

        if self.delegate:
            metadata = SourceLocation(
                start=Position(
                    line=marker.line,
                    column=marker.column,
                    offset=marker.index,
                ),
                end=Position(
                    line=self.lastMarker.line,
                    column=self.lastMarker.column,
                    offset=self.lastMarker.index,
                )
            )
            new_node = self.delegate(node, metadata)
            if new_node is not None:
                node = new_node

        return node

    # Expect the next token to match the specified punctuator.
    # If not, an exception will be thrown.

    def expect(self, value):
        token = self.nextToken()
        if token.type is not Token.Punctuator or token.value != value:
            self.throwUnexpectedToken(token)

    # Quietly expect a comma when in tolerant mode, otherwise delegates to expect().

    def expectCommaSeparator(self):
        if self.config.tolerant:
            token = self.lookahead
            if token.type is Token.Punctuator and token.value == ',':
                self.nextToken()
            elif token.type is Token.Punctuator and token.value == ';':
                self.nextToken()
                self.tolerateUnexpectedToken(token)
            else:
                self.tolerateUnexpectedToken(token, Messages.UnexpectedToken)
        else:
            self.expect(',')

    # Expect the next token to match the specified keyword.
    # If not, an exception will be thrown.

    def expectKeyword(self, keyword):
        token = self.nextToken()
        if token.type is not Token.Keyword or token.value != keyword:
            self.throwUnexpectedToken(token)

    # Return true if the next token matches the specified punctuator.

    def match(self, *value):
        return self.lookahead.type is Token.Punctuator and self.lookahead.value in value

    # Return true if the next token matches the specified keyword

    def matchKeyword(self, *keyword):
        return self.lookahead.type is Token.Keyword and self.lookahead.value in keyword

    # Return true if the next token matches the specified contextual keyword
    # (where an identifier is sometimes a keyword depending on the context)

    def matchContextualKeyword(self, *keyword):
        return self.lookahead.type is Token.Identifier and self.lookahead.value in keyword

    # Return true if the next token is an assignment operator

    def matchAssign(self):
        if self.lookahead.type is not Token.Punctuator:
            return False

        op = self.lookahead.value
        return op in ('=', '*=', '**=', '/=', '%=', '+=', '-=', '<<=', '>>=', '>>>=', '&=', '^=', '|=')

    # Cover grammar support.
    #
    # When an assignment expression position starts with an left parenthesis, the determination of the type
    # of the syntax is to be deferred arbitrarily long until the end of the parentheses pair (plus a lookahead)
    # or the first comma. This situation also defers the determination of all the expressions nested in the pair.
    #
    # There are three productions that can be parsed in a parentheses pair that needs to be determined
    # after the outermost pair is closed. They are:
    #
    #   1. AssignmentExpression
    #   2. BindingElements
    #   3. AssignmentTargets
    #
    # In order to avoid exponential backtracking, we use two flags to denote if the production can be
    # binding element or assignment target.
    #
    # The three productions have the relationship:
    #
    #   BindingElements ⊆ AssignmentTargets ⊆ AssignmentExpression
    #
    # with a single exception that CoverInitializedName when used directly in an Expression, generates
    # an early error. Therefore, we need the third state, firstCoverInitializedNameError, to track the
    # first usage of CoverInitializedName and report it when we reached the end of the parentheses pair.
    #
    # isolateCoverGrammar function runs the given parser function with a new cover grammar context, and it does not
    # effect the current flags. This means the production the parser parses is only used as an expression. Therefore
    # the CoverInitializedName check is conducted.
    #
    # inheritCoverGrammar function runs the given parse function with a new cover grammar context, and it propagates
    # the flags outside of the parser. This means the production the parser parses is used as a part of a potential
    # pattern. The CoverInitializedName check is deferred.

    def isolateCoverGrammar(self, parseFunction):
        previousIsBindingElement = self.context.isBindingElement
        previousIsAssignmentTarget = self.context.isAssignmentTarget
        previousFirstCoverInitializedNameError = self.context.firstCoverInitializedNameError

        self.context.isBindingElement = True
        self.context.isAssignmentTarget = True
        self.context.firstCoverInitializedNameError = None

        result = parseFunction()
        if self.context.firstCoverInitializedNameError is not None:
            self.throwUnexpectedToken(self.context.firstCoverInitializedNameError)

        self.context.isBindingElement = previousIsBindingElement
        self.context.isAssignmentTarget = previousIsAssignmentTarget
        self.context.firstCoverInitializedNameError = previousFirstCoverInitializedNameError

        return result

    def inheritCoverGrammar(self, parseFunction):
        previousIsBindingElement = self.context.isBindingElement
        previousIsAssignmentTarget = self.context.isAssignmentTarget
        previousFirstCoverInitializedNameError = self.context.firstCoverInitializedNameError

        self.context.isBindingElement = True
        self.context.isAssignmentTarget = True
        self.context.firstCoverInitializedNameError = None

        result = parseFunction()

        self.context.isBindingElement = self.context.isBindingElement and previousIsBindingElement
        self.context.isAssignmentTarget = self.context.isAssignmentTarget and previousIsAssignmentTarget
        self.context.firstCoverInitializedNameError = previousFirstCoverInitializedNameError or self.context.firstCoverInitializedNameError

        return result

    def consumeSemicolon(self):
        if self.match(';'):
            self.nextToken()
        elif not self.hasLineTerminator:
            if self.lookahead.type is not Token.EOF and not self.match('}'):
                self.throwUnexpectedToken(self.lookahead)
            self.lastMarker.index = self.startMarker.index
            self.lastMarker.line = self.startMarker.line
            self.lastMarker.column = self.startMarker.column

    # https://tc39.github.io/ecma262/#sec-primary-expression

    def parsePrimaryExpression(self):
        node = self.createNode()

        typ = self.lookahead.type
        if typ is Token.Identifier:
            if (self.context.isModule or self.context.allowAwait) and self.lookahead.value == 'await':
                self.tolerateUnexpectedToken(self.lookahead)
            expr = self.parseFunctionExpression() if self.matchAsyncFunction() else self.finalize(node, Node.Identifier(self.nextToken().value))

        elif typ in (
            Token.NumericLiteral,
            Token.StringLiteral,
        ):
            if self.context.strict and self.lookahead.octal:
                self.tolerateUnexpectedToken(self.lookahead, Messages.StrictOctalLiteral)
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False
            token = self.nextToken()
            raw = self.getTokenRaw(token)
            expr = self.finalize(node, Node.Literal(token.value, raw))

        elif typ is Token.BooleanLiteral:
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False
            token = self.nextToken()
            raw = self.getTokenRaw(token)
            expr = self.finalize(node, Node.Literal(token.value == 'true', raw))

        elif typ is Token.NullLiteral:
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False
            token = self.nextToken()
            raw = self.getTokenRaw(token)
            expr = self.finalize(node, Node.Literal(None, raw))

        elif typ is Token.Template:
            expr = self.parseTemplateLiteral()

        elif typ is Token.Punctuator:
            value = self.lookahead.value
            if value == '(':
                self.context.isBindingElement = False
                expr = self.inheritCoverGrammar(self.parseGroupExpression)
            elif value == '[':
                expr = self.inheritCoverGrammar(self.parseArrayInitializer)
            elif value == '{':
                expr = self.inheritCoverGrammar(self.parseObjectInitializer)
            elif value in ('/', '/='):
                self.context.isAssignmentTarget = False
                self.context.isBindingElement = False
                self.scanner.index = self.startMarker.index
                token = self.nextRegexToken()
                raw = self.getTokenRaw(token)
                expr = self.finalize(node, Node.RegexLiteral(token.regex, raw, token.pattern, token.flags))
            else:
                expr = self.throwUnexpectedToken(self.nextToken())

        elif typ is Token.Keyword:
            if not self.context.strict and self.context.allowYield and self.matchKeyword('yield'):
                expr = self.parseIdentifierName()
            elif not self.context.strict and self.matchKeyword('let'):
                expr = self.finalize(node, Node.Identifier(self.nextToken().value))
            else:
                self.context.isAssignmentTarget = False
                self.context.isBindingElement = False
                if self.matchKeyword('function'):
                    expr = self.parseFunctionExpression()
                elif self.matchKeyword('this'):
                    self.nextToken()
                    expr = self.finalize(node, Node.ThisExpression())
                elif self.matchKeyword('class'):
                    expr = self.parseClassExpression()
                elif self.matchImportCall():
                    expr = self.parseImportCall()
                else:
                    expr = self.throwUnexpectedToken(self.nextToken())

        else:
            expr = self.throwUnexpectedToken(self.nextToken())

        return expr

    # https://tc39.github.io/ecma262/#sec-array-initializer

    def parseSpreadElement(self):
        node = self.createNode()
        self.expect('...')
        arg = self.inheritCoverGrammar(self.parseAssignmentExpression)
        return self.finalize(node, Node.SpreadElement(arg))

    def parseArrayInitializer(self):
        node = self.createNode()
        elements = []

        self.expect('[')
        while not self.match(']'):
            if self.match(','):
                self.nextToken()
                elements.append(None)
            elif self.match('...'):
                element = self.parseSpreadElement()
                if not self.match(']'):
                    self.context.isAssignmentTarget = False
                    self.context.isBindingElement = False
                    self.expect(',')
                elements.append(element)
            else:
                elements.append(self.inheritCoverGrammar(self.parseAssignmentExpression))
                if not self.match(']'):
                    self.expect(',')
        self.expect(']')

        return self.finalize(node, Node.ArrayExpression(elements))

    # https://tc39.github.io/ecma262/#sec-object-initializer

    def parsePropertyMethod(self, params):
        self.context.isAssignmentTarget = False
        self.context.isBindingElement = False

        previousStrict = self.context.strict
        previousAllowStrictDirective = self.context.allowStrictDirective
        self.context.allowStrictDirective = params.simple
        body = self.isolateCoverGrammar(self.parseFunctionSourceElements)
        if self.context.strict and params.firstRestricted:
            self.tolerateUnexpectedToken(params.firstRestricted, params.message)
        if self.context.strict and params.stricted:
            self.tolerateUnexpectedToken(params.stricted, params.message)
        self.context.strict = previousStrict
        self.context.allowStrictDirective = previousAllowStrictDirective

        return body

    def parsePropertyMethodFunction(self):
        isGenerator = False
        node = self.createNode()

        previousAllowYield = self.context.allowYield
        self.context.allowYield = True
        params = self.parseFormalParameters()
        method = self.parsePropertyMethod(params)
        self.context.allowYield = previousAllowYield

        return self.finalize(node, Node.FunctionExpression(None, params.params, method, isGenerator))

    def parsePropertyMethodAsyncFunction(self):
        node = self.createNode()

        previousAllowYield = self.context.allowYield
        previousAwait = self.context.allowAwait
        self.context.allowYield = False
        self.context.allowAwait = True
        params = self.parseFormalParameters()
        method = self.parsePropertyMethod(params)
        self.context.allowYield = previousAllowYield
        self.context.allowAwait = previousAwait

        return self.finalize(node, Node.AsyncFunctionExpression(None, params.params, method))

    def parseObjectPropertyKey(self):
        node = self.createNode()
        token = self.nextToken()

        typ = token.type
        if typ in (
            Token.StringLiteral,
            Token.NumericLiteral,
        ):
            if self.context.strict and token.octal:
                self.tolerateUnexpectedToken(token, Messages.StrictOctalLiteral)
            raw = self.getTokenRaw(token)
            key = self.finalize(node, Node.Literal(token.value, raw))

        elif typ in (
            Token.Identifier,
            Token.BooleanLiteral,
            Token.NullLiteral,
            Token.Keyword,
        ):
            key = self.finalize(node, Node.Identifier(token.value))

        elif typ is Token.Punctuator:
            if token.value == '[':
                key = self.isolateCoverGrammar(self.parseAssignmentExpression)
                self.expect(']')
            else:
                key = self.throwUnexpectedToken(token)

        else:
            key = self.throwUnexpectedToken(token)

        return key

    def isPropertyKey(self, key, value):
        return (
            (key.type is Syntax.Identifier and key.name == value) or
            (key.type is Syntax.Literal and key.value == value)
        )

    def parseObjectProperty(self, hasProto):
        node = self.createNode()
        token = self.lookahead

        key = None
        value = None

        computed = False
        method = False
        shorthand = False
        isAsync = False

        if token.type is Token.Identifier:
            id = token.value
            self.nextToken()
            computed = self.match('[')
            isAsync = not self.hasLineTerminator and (id == 'async') and not (self.match(':', '(', '*', ','))
            key = self.parseObjectPropertyKey() if isAsync else self.finalize(node, Node.Identifier(id))
        elif self.match('*'):
            self.nextToken()
        else:
            computed = self.match('[')
            key = self.parseObjectPropertyKey()

        lookaheadPropertyKey = self.qualifiedPropertyName(self.lookahead)
        if token.type is Token.Identifier and not isAsync and token.value == 'get' and lookaheadPropertyKey:
            kind = 'get'
            computed = self.match('[')
            key = self.parseObjectPropertyKey()
            self.context.allowYield = False
            value = self.parseGetterMethod()

        elif token.type is Token.Identifier and not isAsync and token.value == 'set' and lookaheadPropertyKey:
            kind = 'set'
            computed = self.match('[')
            key = self.parseObjectPropertyKey()
            value = self.parseSetterMethod()

        elif token.type is Token.Punctuator and token.value == '*' and lookaheadPropertyKey:
            kind = 'init'
            computed = self.match('[')
            key = self.parseObjectPropertyKey()
            value = self.parseGeneratorMethod()
            method = True

        else:
            if not key:
                self.throwUnexpectedToken(self.lookahead)

            kind = 'init'
            if self.match(':') and not isAsync:
                if not computed and self.isPropertyKey(key, '__proto__'):
                    if hasProto.value:
                        self.tolerateError(Messages.DuplicateProtoProperty)
                    hasProto.value = True
                self.nextToken()
                value = self.inheritCoverGrammar(self.parseAssignmentExpression)

            elif self.match('('):
                value = self.parsePropertyMethodAsyncFunction() if isAsync else self.parsePropertyMethodFunction()
                method = True

            elif token.type is Token.Identifier:
                id = self.finalize(node, Node.Identifier(token.value))
                if self.match('='):
                    self.context.firstCoverInitializedNameError = self.lookahead
                    self.nextToken()
                    shorthand = True
                    init = self.isolateCoverGrammar(self.parseAssignmentExpression)
                    value = self.finalize(node, Node.AssignmentPattern(id, init))
                else:
                    shorthand = True
                    value = id
            else:
                self.throwUnexpectedToken(self.nextToken())

        return self.finalize(node, Node.Property(kind, key, computed, value, method, shorthand))

    def parseObjectInitializer(self):
        node = self.createNode()

        self.expect('{')
        properties = []
        hasProto = Value(False)
        while not self.match('}'):
            properties.append(self.parseSpreadElement() if self.match('...') else self.parseObjectProperty(hasProto))
            if not self.match('}'):
                self.expectCommaSeparator()
        self.expect('}')

        return self.finalize(node, Node.ObjectExpression(properties))

    # https://tc39.github.io/ecma262/#sec-template-literals

    def parseTemplateHead(self):
        assert self.lookahead.head, 'Template literal must start with a template head'

        node = self.createNode()
        token = self.nextToken()
        raw = token.value
        cooked = token.cooked

        return self.finalize(node, Node.TemplateElement(raw, cooked, token.tail))

    def parseTemplateElement(self):
        if self.lookahead.type is not Token.Template:
            self.throwUnexpectedToken()

        node = self.createNode()
        token = self.nextToken()
        raw = token.value
        cooked = token.cooked

        return self.finalize(node, Node.TemplateElement(raw, cooked, token.tail))

    def parseTemplateLiteral(self):
        node = self.createNode()

        expressions = []
        quasis = []

        quasi = self.parseTemplateHead()
        quasis.append(quasi)
        while not quasi.tail:
            expressions.append(self.parseExpression())
            quasi = self.parseTemplateElement()
            quasis.append(quasi)

        return self.finalize(node, Node.TemplateLiteral(quasis, expressions))

    # https://tc39.github.io/ecma262/#sec-grouping-operator

    def reinterpretExpressionAsPattern(self, expr):
        typ = expr.type
        if typ in (
            Syntax.Identifier,
            Syntax.MemberExpression,
            Syntax.RestElement,
            Syntax.AssignmentPattern,
        ):
            pass
        elif typ is Syntax.SpreadElement:
            expr.type = Syntax.RestElement
            self.reinterpretExpressionAsPattern(expr.argument)
        elif typ is Syntax.ArrayExpression:
            expr.type = Syntax.ArrayPattern
            for elem in expr.elements:
                if elem is not None:
                    self.reinterpretExpressionAsPattern(elem)
        elif typ is Syntax.ObjectExpression:
            expr.type = Syntax.ObjectPattern
            for prop in expr.properties:
                self.reinterpretExpressionAsPattern(prop if prop.type is Syntax.SpreadElement else prop.value)
        elif typ is Syntax.AssignmentExpression:
            expr.type = Syntax.AssignmentPattern
            del expr.operator
            self.reinterpretExpressionAsPattern(expr.left)
        else:
            # Allow other node type for tolerant parsing.
            pass

    def parseGroupExpression(self):
        self.expect('(')
        if self.match(')'):
            self.nextToken()
            if not self.match('=>'):
                self.expect('=>')
            expr = Node.ArrowParameterPlaceHolder([])
        else:
            startToken = self.lookahead
            params = []
            if self.match('...'):
                expr = self.parseRestElement(params)
                self.expect(')')
                if not self.match('=>'):
                    self.expect('=>')
                expr = Node.ArrowParameterPlaceHolder([expr])
            else:
                arrow = False
                self.context.isBindingElement = True
                expr = self.inheritCoverGrammar(self.parseAssignmentExpression)

                if self.match(','):
                    expressions = []

                    self.context.isAssignmentTarget = False
                    expressions.append(expr)
                    while self.lookahead.type is not Token.EOF:
                        if not self.match(','):
                            break
                        self.nextToken()
                        if self.match(')'):
                            self.nextToken()
                            for expression in expressions:
                                self.reinterpretExpressionAsPattern(expression)
                            arrow = True
                            expr = Node.ArrowParameterPlaceHolder(expressions)
                        elif self.match('...'):
                            if not self.context.isBindingElement:
                                self.throwUnexpectedToken(self.lookahead)
                            expressions.append(self.parseRestElement(params))
                            self.expect(')')
                            if not self.match('=>'):
                                self.expect('=>')
                            self.context.isBindingElement = False
                            for expression in expressions:
                                self.reinterpretExpressionAsPattern(expression)
                            arrow = True
                            expr = Node.ArrowParameterPlaceHolder(expressions)
                        else:
                            expressions.append(self.inheritCoverGrammar(self.parseAssignmentExpression))
                        if arrow:
                            break
                    if not arrow:
                        expr = self.finalize(self.startNode(startToken), Node.SequenceExpression(expressions))

                if not arrow:
                    self.expect(')')
                    if self.match('=>'):
                        if expr.type is Syntax.Identifier and expr.name == 'yield':
                            arrow = True
                            expr = Node.ArrowParameterPlaceHolder([expr])
                        if not arrow:
                            if not self.context.isBindingElement:
                                self.throwUnexpectedToken(self.lookahead)

                            if expr.type is Syntax.SequenceExpression:
                                for expression in expr.expressions:
                                    self.reinterpretExpressionAsPattern(expression)
                            else:
                                self.reinterpretExpressionAsPattern(expr)

                            if expr.type is Syntax.SequenceExpression:
                                parameters = expr.expressions
                            else:
                                parameters = [expr]
                            expr = Node.ArrowParameterPlaceHolder(parameters)
                    self.context.isBindingElement = False

        return expr

    # https://tc39.github.io/ecma262/#sec-left-hand-side-expressions

    def parseArguments(self):
        self.expect('(')
        args = []
        if not self.match(')'):
            while True:
                if self.match('...'):
                    expr = self.parseSpreadElement()
                else:
                    expr = self.isolateCoverGrammar(self.parseAssignmentExpression)
                args.append(expr)
                if self.match(')'):
                    break
                self.expectCommaSeparator()
                if self.match(')'):
                    break
        self.expect(')')

        return args

    def isIdentifierName(self, token):
        return (
            token.type is Token.Identifier or
            token.type is Token.Keyword or
            token.type is Token.BooleanLiteral or
            token.type is Token.NullLiteral
        )

    def parseIdentifierName(self):
        node = self.createNode()
        token = self.nextToken()
        if not self.isIdentifierName(token):
            self.throwUnexpectedToken(token)
        return self.finalize(node, Node.Identifier(token.value))

    def parseNewExpression(self):
        node = self.createNode()

        id = self.parseIdentifierName()
        assert id.name == 'new', 'New expression must start with `new`'

        if self.match('.'):
            self.nextToken()
            if self.lookahead.type is Token.Identifier and self.context.inFunctionBody and self.lookahead.value == 'target':
                property = self.parseIdentifierName()
                expr = Node.MetaProperty(id, property)
            else:
                self.throwUnexpectedToken(self.lookahead)
        elif self.matchKeyword('import'):
            self.throwUnexpectedToken(self.lookahead)
        else:
            callee = self.isolateCoverGrammar(self.parseLeftHandSideExpression)
            args = self.parseArguments() if self.match('(') else []
            expr = Node.NewExpression(callee, args)
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False

        return self.finalize(node, expr)

    def parseAsyncArgument(self):
        arg = self.parseAssignmentExpression()
        self.context.firstCoverInitializedNameError = None
        return arg

    def parseAsyncArguments(self):
        self.expect('(')
        args = []
        if not self.match(')'):
            while True:
                if self.match('...'):
                    expr = self.parseSpreadElement()
                else:
                    expr = self.isolateCoverGrammar(self.parseAsyncArgument)
                args.append(expr)
                if self.match(')'):
                    break
                self.expectCommaSeparator()
                if self.match(')'):
                    break
        self.expect(')')

        return args

    def matchImportCall(self):
        match = self.matchKeyword('import')
        if match:
            state = self.scanner.saveState()
            self.scanner.scanComments()
            next = self.scanner.lex()
            self.scanner.restoreState(state)
            match = (next.type is Token.Punctuator) and (next.value == '(')

        return match

    def parseImportCall(self):
        node = self.createNode()
        self.expectKeyword('import')
        return self.finalize(node, Node.Import())

    def parseLeftHandSideExpressionAllowCall(self):
        startToken = self.lookahead
        maybeAsync = self.matchContextualKeyword('async')

        previousAllowIn = self.context.allowIn
        self.context.allowIn = True

        if self.matchKeyword('super') and self.context.inFunctionBody:
            expr = self.createNode()
            self.nextToken()
            expr = self.finalize(expr, Node.Super())
            if not self.match('(') and not self.match('.') and not self.match('['):
                self.throwUnexpectedToken(self.lookahead)
        else:
            expr = self.inheritCoverGrammar(self.parseNewExpression if self.matchKeyword('new') else self.parsePrimaryExpression)

        while True:
            if self.match('.'):
                self.context.isBindingElement = False
                self.context.isAssignmentTarget = True
                self.expect('.')
                property = self.parseIdentifierName()
                expr = self.finalize(self.startNode(startToken), Node.StaticMemberExpression(expr, property))

            elif self.match('('):
                asyncArrow = maybeAsync and (startToken.lineNumber == self.lookahead.lineNumber)
                self.context.isBindingElement = False
                self.context.isAssignmentTarget = False
                if asyncArrow:
                    args = self.parseAsyncArguments()
                else:
                    args = self.parseArguments()
                if expr.type is Syntax.Import and len(args) != 1:
                    self.tolerateError(Messages.BadImportCallArity)
                expr = self.finalize(self.startNode(startToken), Node.CallExpression(expr, args))
                if asyncArrow and self.match('=>'):
                    for arg in args:
                        self.reinterpretExpressionAsPattern(arg)
                    expr = Node.AsyncArrowParameterPlaceHolder(args)
            elif self.match('['):
                self.context.isBindingElement = False
                self.context.isAssignmentTarget = True
                self.expect('[')
                property = self.isolateCoverGrammar(self.parseExpression)
                self.expect(']')
                expr = self.finalize(self.startNode(startToken), Node.ComputedMemberExpression(expr, property))

            elif self.lookahead.type is Token.Template and self.lookahead.head:
                quasi = self.parseTemplateLiteral()
                expr = self.finalize(self.startNode(startToken), Node.TaggedTemplateExpression(expr, quasi))

            else:
                break

        self.context.allowIn = previousAllowIn

        return expr

    def parseSuper(self):
        node = self.createNode()

        self.expectKeyword('super')
        if not self.match('[') and not self.match('.'):
            self.throwUnexpectedToken(self.lookahead)

        return self.finalize(node, Node.Super())

    def parseLeftHandSideExpression(self):
        assert self.context.allowIn, 'callee of new expression always allow in keyword.'

        node = self.startNode(self.lookahead)
        if self.matchKeyword('super') and self.context.inFunctionBody:
            expr = self.parseSuper()
        else:
            expr = self.inheritCoverGrammar(self.parseNewExpression if self.matchKeyword('new') else self.parsePrimaryExpression)

        while True:
            if self.match('['):
                self.context.isBindingElement = False
                self.context.isAssignmentTarget = True
                self.expect('[')
                property = self.isolateCoverGrammar(self.parseExpression)
                self.expect(']')
                expr = self.finalize(node, Node.ComputedMemberExpression(expr, property))

            elif self.match('.'):
                self.context.isBindingElement = False
                self.context.isAssignmentTarget = True
                self.expect('.')
                property = self.parseIdentifierName()
                expr = self.finalize(node, Node.StaticMemberExpression(expr, property))

            elif self.lookahead.type is Token.Template and self.lookahead.head:
                quasi = self.parseTemplateLiteral()
                expr = self.finalize(node, Node.TaggedTemplateExpression(expr, quasi))

            else:
                break

        return expr

    # https://tc39.github.io/ecma262/#sec-update-expressions

    def parseUpdateExpression(self):
        startToken = self.lookahead

        if self.match('++', '--'):
            node = self.startNode(startToken)
            token = self.nextToken()
            expr = self.inheritCoverGrammar(self.parseUnaryExpression)
            if self.context.strict and expr.type is Syntax.Identifier and self.scanner.isRestrictedWord(expr.name):
                self.tolerateError(Messages.StrictLHSPrefix)
            if not self.context.isAssignmentTarget:
                self.tolerateError(Messages.InvalidLHSInAssignment)
            prefix = True
            expr = self.finalize(node, Node.UpdateExpression(token.value, expr, prefix))
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False
        else:
            expr = self.inheritCoverGrammar(self.parseLeftHandSideExpressionAllowCall)
            if not self.hasLineTerminator and self.lookahead.type is Token.Punctuator:
                if self.match('++', '--'):
                    if self.context.strict and expr.type is Syntax.Identifier and self.scanner.isRestrictedWord(expr.name):
                        self.tolerateError(Messages.StrictLHSPostfix)
                    if not self.context.isAssignmentTarget:
                        self.tolerateError(Messages.InvalidLHSInAssignment)
                    self.context.isAssignmentTarget = False
                    self.context.isBindingElement = False
                    operator = self.nextToken().value
                    prefix = False
                    expr = self.finalize(self.startNode(startToken), Node.UpdateExpression(operator, expr, prefix))

        return expr

    # https://tc39.github.io/ecma262/#sec-unary-operators

    def parseAwaitExpression(self):
        node = self.createNode()
        self.nextToken()
        argument = self.parseUnaryExpression()
        return self.finalize(node, Node.AwaitExpression(argument))

    def parseUnaryExpression(self):
        if (
            self.match('+', '-', '~', '!') or
            self.matchKeyword('delete', 'void', 'typeof')
        ):
            node = self.startNode(self.lookahead)
            token = self.nextToken()
            expr = self.inheritCoverGrammar(self.parseUnaryExpression)
            expr = self.finalize(node, Node.UnaryExpression(token.value, expr))
            if self.context.strict and expr.operator == 'delete' and expr.argument.type is Syntax.Identifier:
                self.tolerateError(Messages.StrictDelete)
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False
        elif self.context.allowAwait and self.matchContextualKeyword('await'):
            expr = self.parseAwaitExpression()
        else:
            expr = self.parseUpdateExpression()

        return expr

    def parseExponentiationExpression(self):
        startToken = self.lookahead

        expr = self.inheritCoverGrammar(self.parseUnaryExpression)
        if expr.type is not Syntax.UnaryExpression and self.match('**'):
            self.nextToken()
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False
            left = expr
            right = self.isolateCoverGrammar(self.parseExponentiationExpression)
            expr = self.finalize(self.startNode(startToken), Node.BinaryExpression('**', left, right))

        return expr

    # https://tc39.github.io/ecma262/#sec-exp-operator
    # https://tc39.github.io/ecma262/#sec-multiplicative-operators
    # https://tc39.github.io/ecma262/#sec-additive-operators
    # https://tc39.github.io/ecma262/#sec-bitwise-shift-operators
    # https://tc39.github.io/ecma262/#sec-relational-operators
    # https://tc39.github.io/ecma262/#sec-equality-operators
    # https://tc39.github.io/ecma262/#sec-binary-bitwise-operators
    # https://tc39.github.io/ecma262/#sec-binary-logical-operators

    def binaryPrecedence(self, token):
        op = token.value
        if token.type is Token.Punctuator:
            precedence = self.operatorPrecedence.get(op, 0)
        elif token.type is Token.Keyword:
            precedence = 7 if (op == 'instanceof' or (self.context.allowIn and op == 'in')) else 0
        else:
            precedence = 0
        return precedence

    def parseBinaryExpression(self):
        startToken = self.lookahead

        expr = self.inheritCoverGrammar(self.parseExponentiationExpression)

        token = self.lookahead
        prec = self.binaryPrecedence(token)
        if prec > 0:
            self.nextToken()

            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False

            markers = [startToken, self.lookahead]
            left = expr
            right = self.isolateCoverGrammar(self.parseExponentiationExpression)

            stack = [left, token.value, right]
            precedences = [prec]
            while True:
                prec = self.binaryPrecedence(self.lookahead)
                if prec <= 0:
                    break

                # Reduce: make a binary expression from the three topmost entries.
                while len(stack) > 2 and prec <= precedences[-1]:
                    right = stack.pop()
                    operator = stack.pop()
                    precedences.pop()
                    left = stack.pop()
                    markers.pop()
                    node = self.startNode(markers[-1])
                    stack.append(self.finalize(node, Node.BinaryExpression(operator, left, right)))

                # Shift.
                stack.append(self.nextToken().value)
                precedences.append(prec)
                markers.append(self.lookahead)
                stack.append(self.isolateCoverGrammar(self.parseExponentiationExpression))

            # Final reduce to clean-up the stack.
            i = len(stack) - 1
            expr = stack[i]

            lastMarker = markers.pop()
            while i > 1:
                marker = markers.pop()
                lastLineStart = lastMarker.lineStart if lastMarker else 0
                node = self.startNode(marker, lastLineStart)
                operator = stack[i - 1]
                expr = self.finalize(node, Node.BinaryExpression(operator, stack[i - 2], expr))
                i -= 2
                lastMarker = marker

        return expr

    # https://tc39.github.io/ecma262/#sec-conditional-operator

    def parseConditionalExpression(self):
        startToken = self.lookahead

        expr = self.inheritCoverGrammar(self.parseBinaryExpression)
        if self.match('?'):
            self.nextToken()

            previousAllowIn = self.context.allowIn
            self.context.allowIn = True
            consequent = self.isolateCoverGrammar(self.parseAssignmentExpression)
            self.context.allowIn = previousAllowIn

            self.expect(':')
            alternate = self.isolateCoverGrammar(self.parseAssignmentExpression)

            expr = self.finalize(self.startNode(startToken), Node.ConditionalExpression(expr, consequent, alternate))
            self.context.isAssignmentTarget = False
            self.context.isBindingElement = False

        return expr

    # https://tc39.github.io/ecma262/#sec-assignment-operators

    def checkPatternParam(self, options, param):
        typ = param.type
        if typ is Syntax.Identifier:
            self.validateParam(options, param, param.name)
        elif typ is Syntax.RestElement:
            self.checkPatternParam(options, param.argument)
        elif typ is Syntax.AssignmentPattern:
            self.checkPatternParam(options, param.left)
        elif typ is Syntax.ArrayPattern:
            for element in param.elements:
                if element is not None:
                    self.checkPatternParam(options, element)
        elif typ is Syntax.ObjectPattern:
            for prop in param.properties:
                self.checkPatternParam(options, prop if prop.type is Syntax.RestElement else prop.value)

        options.simple = options.simple and isinstance(param, Node.Identifier)

    def reinterpretAsCoverFormalsList(self, expr):
        params = [expr]

        asyncArrow = False
        typ = expr.type
        if typ is Syntax.Identifier:
            pass
        elif typ is Syntax.ArrowParameterPlaceHolder:
            params = expr.params
            asyncArrow = expr.isAsync
        else:
            return None

        options = Params(
            simple=True,
            paramSet={},
        )

        for param in params:
            if param.type is Syntax.AssignmentPattern:
                if param.right.type is Syntax.YieldExpression:
                    if param.right.argument:
                        self.throwUnexpectedToken(self.lookahead)
                    param.right.type = Syntax.Identifier
                    param.right.name = 'yield'
                    del param.right.argument
                    del param.right.delegate
            elif asyncArrow and param.type is Syntax.Identifier and param.name == 'await':
                self.throwUnexpectedToken(self.lookahead)
            self.checkPatternParam(options, param)

        if self.context.strict or not self.context.allowYield:
            for param in params:
                if param.type is Syntax.YieldExpression:
                    self.throwUnexpectedToken(self.lookahead)

        if options.message is Messages.StrictParamDupe:
            token = options.stricted if self.context.strict else options.firstRestricted
            self.throwUnexpectedToken(token, options.message)

        return Params(
            simple=options.simple,
            params=params,
            stricted=options.stricted,
            firstRestricted=options.firstRestricted,
            message=options.message
        )

    def parseAssignmentExpression(self):
        if not self.context.allowYield and self.matchKeyword('yield'):
            expr = self.parseYieldExpression()
        else:
            startToken = self.lookahead
            token = startToken
            expr = self.parseConditionalExpression()

            if token.type is Token.Identifier and (token.lineNumber == self.lookahead.lineNumber) and token.value == 'async':
                if self.lookahead.type is Token.Identifier or self.matchKeyword('yield'):
                    arg = self.parsePrimaryExpression()
                    self.reinterpretExpressionAsPattern(arg)
                    expr = Node.AsyncArrowParameterPlaceHolder([arg])

            if expr.type is Syntax.ArrowParameterPlaceHolder or self.match('=>'):

                # https://tc39.github.io/ecma262/#sec-arrow-function-definitions
                self.context.isAssignmentTarget = False
                self.context.isBindingElement = False
                isAsync = expr.isAsync
                list = self.reinterpretAsCoverFormalsList(expr)

                if list:
                    if self.hasLineTerminator:
                        self.tolerateUnexpectedToken(self.lookahead)
                    self.context.firstCoverInitializedNameError = None

                    previousStrict = self.context.strict
                    previousAllowStrictDirective = self.context.allowStrictDirective
                    self.context.allowStrictDirective = list.simple

                    previousAllowYield = self.context.allowYield
                    previousAwait = self.context.allowAwait
                    self.context.allowYield = True
                    self.context.allowAwait = isAsync

                    node = self.startNode(startToken)
                    self.expect('=>')
                    if self.match('{'):
                        previousAllowIn = self.context.allowIn
                        self.context.allowIn = True
                        body = self.parseFunctionSourceElements()
                        self.context.allowIn = previousAllowIn
                    else:
                        body = self.isolateCoverGrammar(self.parseAssignmentExpression)
                    expression = body.type is not Syntax.BlockStatement

                    if self.context.strict and list.firstRestricted:
                        self.throwUnexpectedToken(list.firstRestricted, list.message)
                    if self.context.strict and list.stricted:
                        self.tolerateUnexpectedToken(list.stricted, list.message)
                    if isAsync:
                        expr = self.finalize(node, Node.AsyncArrowFunctionExpression(list.params, body, expression))
                    else:
                        expr = self.finalize(node, Node.ArrowFunctionExpression(list.params, body, expression))

                    self.context.strict = previousStrict
                    self.context.allowStrictDirective = previousAllowStrictDirective
                    self.context.allowYield = previousAllowYield
                    self.context.allowAwait = previousAwait
            else:
                if self.matchAssign():
                    if not self.context.isAssignmentTarget:
                        self.tolerateError(Messages.InvalidLHSInAssignment)

                    if self.context.strict and expr.type is Syntax.Identifier:
                        id = expr
                        if self.scanner.isRestrictedWord(id.name):
                            self.tolerateUnexpectedToken(token, Messages.StrictLHSAssignment)
                        if self.scanner.isStrictModeReservedWord(id.name):
                            self.tolerateUnexpectedToken(token, Messages.StrictReservedWord)

                    if not self.match('='):
                        self.context.isAssignmentTarget = False
                        self.context.isBindingElement = False
                    else:
                        self.reinterpretExpressionAsPattern(expr)

                    token = self.nextToken()
                    operator = token.value
                    right = self.isolateCoverGrammar(self.parseAssignmentExpression)
                    expr = self.finalize(self.startNode(startToken), Node.AssignmentExpression(operator, expr, right))
                    self.context.firstCoverInitializedNameError = None

        return expr

    # https://tc39.github.io/ecma262/#sec-comma-operator

    def parseExpression(self):
        startToken = self.lookahead
        expr = self.isolateCoverGrammar(self.parseAssignmentExpression)

        if self.match(','):
            expressions = []
            expressions.append(expr)
            while self.lookahead.type is not Token.EOF:
                if not self.match(','):
                    break
                self.nextToken()
                expressions.append(self.isolateCoverGrammar(self.parseAssignmentExpression))

            expr = self.finalize(self.startNode(startToken), Node.SequenceExpression(expressions))

        return expr

    # https://tc39.github.io/ecma262/#sec-block

    def parseStatementListItem(self):
        self.context.isAssignmentTarget = True
        self.context.isBindingElement = True
        if self.lookahead.type is Token.Keyword:
            value = self.lookahead.value
            if value == 'export':
                if not self.context.isModule:
                    self.tolerateUnexpectedToken(self.lookahead, Messages.IllegalExportDeclaration)
                statement = self.parseExportDeclaration()
            elif value == 'import':
                if self.matchImportCall():
                    statement = self.parseExpressionStatement()
                else:
                    if not self.context.isModule:
                        self.tolerateUnexpectedToken(self.lookahead, Messages.IllegalImportDeclaration)
                    statement = self.parseImportDeclaration()
            elif value == 'const':
                statement = self.parseLexicalDeclaration(Params(inFor=False))
            elif value == 'function':
                statement = self.parseFunctionDeclaration()
            elif value == 'class':
                statement = self.parseClassDeclaration()
            elif value == 'let':
                statement = self.parseLexicalDeclaration(Params(inFor=False)) if self.isLexicalDeclaration() else self.parseStatement()
            else:
                statement = self.parseStatement()
        else:
            statement = self.parseStatement()

        return statement

    def parseBlock(self):
        node = self.createNode()

        self.expect('{')
        block = []
        while True:
            if self.match('}'):
                break
            block.append(self.parseStatementListItem())
        self.expect('}')

        return self.finalize(node, Node.BlockStatement(block))

    # https://tc39.github.io/ecma262/#sec-let-and-const-declarations

    def parseLexicalBinding(self, kind, options):
        node = self.createNode()
        params = []
        id = self.parsePattern(params, kind)

        if self.context.strict and id.type is Syntax.Identifier:
            if self.scanner.isRestrictedWord(id.name):
                self.tolerateError(Messages.StrictVarName)

        init = None
        if kind == 'const':
            if not self.matchKeyword('in') and not self.matchContextualKeyword('of'):
                if self.match('='):
                    self.nextToken()
                    init = self.isolateCoverGrammar(self.parseAssignmentExpression)
                else:
                    self.throwError(Messages.DeclarationMissingInitializer, 'const')
        elif (not options.inFor and id.type is not Syntax.Identifier) or self.match('='):
            self.expect('=')
            init = self.isolateCoverGrammar(self.parseAssignmentExpression)

        return self.finalize(node, Node.VariableDeclarator(id, init))

    def parseBindingList(self, kind, options):
        lst = [self.parseLexicalBinding(kind, options)]

        while self.match(','):
            self.nextToken()
            lst.append(self.parseLexicalBinding(kind, options))

        return lst

    def isLexicalDeclaration(self):
        state = self.scanner.saveState()
        self.scanner.scanComments()
        next = self.scanner.lex()
        self.scanner.restoreState(state)

        return (
            (next.type is Token.Identifier) or
            (next.type is Token.Punctuator and next.value == '[') or
            (next.type is Token.Punctuator and next.value == '{') or
            (next.type is Token.Keyword and next.value == 'let') or
            (next.type is Token.Keyword and next.value == 'yield')
        )

    def parseLexicalDeclaration(self, options):
        node = self.createNode()
        kind = self.nextToken().value
        assert kind == 'let' or kind == 'const', 'Lexical declaration must be either or const'

        declarations = self.parseBindingList(kind, options)
        self.consumeSemicolon()

        return self.finalize(node, Node.VariableDeclaration(declarations, kind))

    # https://tc39.github.io/ecma262/#sec-destructuring-binding-patterns

    def parseBindingRestElement(self, params, kind=None):
        node = self.createNode()

        self.expect('...')
        arg = self.parsePattern(params, kind)

        return self.finalize(node, Node.RestElement(arg))

    def parseArrayPattern(self, params, kind=None):
        node = self.createNode()

        self.expect('[')
        elements = []
        while not self.match(']'):
            if self.match(','):
                self.nextToken()
                elements.append(None)
            else:
                if self.match('...'):
                    elements.append(self.parseBindingRestElement(params, kind))
                    break
                else:
                    elements.append(self.parsePatternWithDefault(params, kind))
                if not self.match(']'):
                    self.expect(',')
        self.expect(']')

        return self.finalize(node, Node.ArrayPattern(elements))

    def parsePropertyPattern(self, params, kind=None):
        node = self.createNode()

        computed = False
        shorthand = False
        method = False

        key = None

        if self.lookahead.type is Token.Identifier:
            keyToken = self.lookahead
            key = self.parseVariableIdentifier()
            init = self.finalize(node, Node.Identifier(keyToken.value))
            if self.match('='):
                params.append(keyToken)
                shorthand = True
                self.nextToken()
                expr = self.parseAssignmentExpression()
                value = self.finalize(self.startNode(keyToken), Node.AssignmentPattern(init, expr))
            elif not self.match(':'):
                params.append(keyToken)
                shorthand = True
                value = init
            else:
                self.expect(':')
                value = self.parsePatternWithDefault(params, kind)
        else:
            computed = self.match('[')
            key = self.parseObjectPropertyKey()
            self.expect(':')
            value = self.parsePatternWithDefault(params, kind)

        return self.finalize(node, Node.Property('init', key, computed, value, method, shorthand))

    def parseRestProperty(self, params, kind):
        node = self.createNode()
        self.expect('...')
        arg = self.parsePattern(params)
        if self.match('='):
            self.throwError(Messages.DefaultRestProperty)
        if not self.match('}'):
            self.throwError(Messages.PropertyAfterRestProperty)
        return self.finalize(node, Node.RestElement(arg))

    def parseObjectPattern(self, params, kind=None):
        node = self.createNode()
        properties = []

        self.expect('{')
        while not self.match('}'):
            properties.append(self.parseRestProperty(params, kind) if self.match('...') else self.parsePropertyPattern(params, kind))
            if not self.match('}'):
                self.expect(',')
        self.expect('}')

        return self.finalize(node, Node.ObjectPattern(properties))

    def parsePattern(self, params, kind=None):
        if self.match('['):
            pattern = self.parseArrayPattern(params, kind)
        elif self.match('{'):
            pattern = self.parseObjectPattern(params, kind)
        else:
            if self.matchKeyword('let') and (kind in ('const', 'let')):
                self.tolerateUnexpectedToken(self.lookahead, Messages.LetInLexicalBinding)
            params.append(self.lookahead)
            pattern = self.parseVariableIdentifier(kind)

        return pattern

    def parsePatternWithDefault(self, params, kind=None):
        startToken = self.lookahead

        pattern = self.parsePattern(params, kind)
        if self.match('='):
            self.nextToken()
            previousAllowYield = self.context.allowYield
            self.context.allowYield = True
            right = self.isolateCoverGrammar(self.parseAssignmentExpression)
            self.context.allowYield = previousAllowYield
            pattern = self.finalize(self.startNode(startToken), Node.AssignmentPattern(pattern, right))

        return pattern

    # https://tc39.github.io/ecma262/#sec-variable-statement

    def parseVariableIdentifier(self, kind=None):
        node = self.createNode()

        token = self.nextToken()
        if token.type is Token.Keyword and token.value == 'yield':
            if self.context.strict:
                self.tolerateUnexpectedToken(token, Messages.StrictReservedWord)
            elif not self.context.allowYield:
                self.throwUnexpectedToken(token)
        elif token.type is not Token.Identifier:
            if self.context.strict and token.type is Token.Keyword and self.scanner.isStrictModeReservedWord(token.value):
                self.tolerateUnexpectedToken(token, Messages.StrictReservedWord)
            else:
                if self.context.strict or token.value != 'let' or kind != 'var':
                    self.throwUnexpectedToken(token)
        elif (self.context.isModule or self.context.allowAwait) and token.type is Token.Identifier and token.value == 'await':
            self.tolerateUnexpectedToken(token)

        return self.finalize(node, Node.Identifier(token.value))

    def parseVariableDeclaration(self, options):
        node = self.createNode()

        params = []
        id = self.parsePattern(params, 'var')

        if self.context.strict and id.type is Syntax.Identifier:
            if self.scanner.isRestrictedWord(id.name):
                self.tolerateError(Messages.StrictVarName)

        init = None
        if self.match('='):
            self.nextToken()
            init = self.isolateCoverGrammar(self.parseAssignmentExpression)
        elif id.type is not Syntax.Identifier and not options.inFor:
            self.expect('=')

        return self.finalize(node, Node.VariableDeclarator(id, init))

    def parseVariableDeclarationList(self, options):
        opt = Params(inFor=options.inFor)

        lst = []
        lst.append(self.parseVariableDeclaration(opt))
        while self.match(','):
            self.nextToken()
            lst.append(self.parseVariableDeclaration(opt))

        return lst

    def parseVariableStatement(self):
        node = self.createNode()
        self.expectKeyword('var')
        declarations = self.parseVariableDeclarationList(Params(inFor=False))
        self.consumeSemicolon()

        return self.finalize(node, Node.VariableDeclaration(declarations, 'var'))

    # https://tc39.github.io/ecma262/#sec-empty-statement

    def parseEmptyStatement(self):
        node = self.createNode()
        self.expect(';')
        return self.finalize(node, Node.EmptyStatement())

    # https://tc39.github.io/ecma262/#sec-expression-statement

    def parseExpressionStatement(self):
        node = self.createNode()
        expr = self.parseExpression()
        self.consumeSemicolon()
        return self.finalize(node, Node.ExpressionStatement(expr))

    # https://tc39.github.io/ecma262/#sec-if-statement

    def parseIfClause(self):
        if self.context.strict and self.matchKeyword('function'):
            self.tolerateError(Messages.StrictFunction)
        return self.parseStatement()

    def parseIfStatement(self):
        node = self.createNode()
        alternate = None

        self.expectKeyword('if')
        self.expect('(')
        test = self.parseExpression()

        if not self.match(')') and self.config.tolerant:
            self.tolerateUnexpectedToken(self.nextToken())
            consequent = self.finalize(self.createNode(), Node.EmptyStatement())
        else:
            self.expect(')')
            consequent = self.parseIfClause()
            if self.matchKeyword('else'):
                self.nextToken()
                alternate = self.parseIfClause()

        return self.finalize(node, Node.IfStatement(test, consequent, alternate))

    # https://tc39.github.io/ecma262/#sec-do-while-statement

    def parseDoWhileStatement(self):
        node = self.createNode()
        self.expectKeyword('do')

        previousInIteration = self.context.inIteration
        self.context.inIteration = True
        body = self.parseStatement()
        self.context.inIteration = previousInIteration

        self.expectKeyword('while')
        self.expect('(')
        test = self.parseExpression()

        if not self.match(')') and self.config.tolerant:
            self.tolerateUnexpectedToken(self.nextToken())
        else:
            self.expect(')')
            if self.match(';'):
                self.nextToken()

        return self.finalize(node, Node.DoWhileStatement(body, test))

    # https://tc39.github.io/ecma262/#sec-while-statement

    def parseWhileStatement(self):
        node = self.createNode()

        self.expectKeyword('while')
        self.expect('(')
        test = self.parseExpression()

        if not self.match(')') and self.config.tolerant:
            self.tolerateUnexpectedToken(self.nextToken())
            body = self.finalize(self.createNode(), Node.EmptyStatement())
        else:
            self.expect(')')

            previousInIteration = self.context.inIteration
            self.context.inIteration = True
            body = self.parseStatement()
            self.context.inIteration = previousInIteration

        return self.finalize(node, Node.WhileStatement(test, body))

    # https://tc39.github.io/ecma262/#sec-for-statement
    # https://tc39.github.io/ecma262/#sec-for-in-and-for-of-statements

    def parseForStatement(self):
        init = None
        test = None
        update = None
        forIn = True
        left = None
        right = None

        node = self.createNode()
        self.expectKeyword('for')
        self.expect('(')

        if self.match(';'):
            self.nextToken()
        else:
            if self.matchKeyword('var'):
                init = self.createNode()
                self.nextToken()

                previousAllowIn = self.context.allowIn
                self.context.allowIn = False
                declarations = self.parseVariableDeclarationList(Params(inFor=True))
                self.context.allowIn = previousAllowIn

                if len(declarations) == 1 and self.matchKeyword('in'):
                    decl = declarations[0]
                    if decl.init and (decl.id.type is Syntax.ArrayPattern or decl.id.type is Syntax.ObjectPattern or self.context.strict):
                        self.tolerateError(Messages.ForInOfLoopInitializer, 'for-in')
                    init = self.finalize(init, Node.VariableDeclaration(declarations, 'var'))
                    self.nextToken()
                    left = init
                    right = self.parseExpression()
                    init = None
                elif len(declarations) == 1 and declarations[0].init is None and self.matchContextualKeyword('of'):
                    init = self.finalize(init, Node.VariableDeclaration(declarations, 'var'))
                    self.nextToken()
                    left = init
                    right = self.parseAssignmentExpression()
                    init = None
                    forIn = False
                else:
                    init = self.finalize(init, Node.VariableDeclaration(declarations, 'var'))
                    self.expect(';')
            elif self.matchKeyword('const', 'let'):
                init = self.createNode()
                kind = self.nextToken().value

                if not self.context.strict and self.lookahead.value == 'in':
                    init = self.finalize(init, Node.Identifier(kind))
                    self.nextToken()
                    left = init
                    right = self.parseExpression()
                    init = None
                else:
                    previousAllowIn = self.context.allowIn
                    self.context.allowIn = False
                    declarations = self.parseBindingList(kind, Params(inFor=True))
                    self.context.allowIn = previousAllowIn

                    if len(declarations) == 1 and declarations[0].init is None and self.matchKeyword('in'):
                        init = self.finalize(init, Node.VariableDeclaration(declarations, kind))
                        self.nextToken()
                        left = init
                        right = self.parseExpression()
                        init = None
                    elif len(declarations) == 1 and declarations[0].init is None and self.matchContextualKeyword('of'):
                        init = self.finalize(init, Node.VariableDeclaration(declarations, kind))
                        self.nextToken()
                        left = init
                        right = self.parseAssignmentExpression()
                        init = None
                        forIn = False
                    else:
                        self.consumeSemicolon()
                        init = self.finalize(init, Node.VariableDeclaration(declarations, kind))
            else:
                initStartToken = self.lookahead
                previousAllowIn = self.context.allowIn
                self.context.allowIn = False
                init = self.inheritCoverGrammar(self.parseAssignmentExpression)
                self.context.allowIn = previousAllowIn

                if self.matchKeyword('in'):
                    if not self.context.isAssignmentTarget or init.type is Syntax.AssignmentExpression:
                        self.tolerateError(Messages.InvalidLHSInForIn)

                    self.nextToken()
                    self.reinterpretExpressionAsPattern(init)
                    left = init
                    right = self.parseExpression()
                    init = None
                elif self.matchContextualKeyword('of'):
                    if not self.context.isAssignmentTarget or init.type is Syntax.AssignmentExpression:
                        self.tolerateError(Messages.InvalidLHSInForLoop)

                    self.nextToken()
                    self.reinterpretExpressionAsPattern(init)
                    left = init
                    right = self.parseAssignmentExpression()
                    init = None
                    forIn = False
                else:
                    if self.match(','):
                        initSeq = [init]
                        while self.match(','):
                            self.nextToken()
                            initSeq.append(self.isolateCoverGrammar(self.parseAssignmentExpression))
                        init = self.finalize(self.startNode(initStartToken), Node.SequenceExpression(initSeq))
                    self.expect(';')

        if left is None:
            if not self.match(';'):
                test = self.parseExpression()
            self.expect(';')
            if not self.match(')'):
                update = self.parseExpression()

        if not self.match(')') and self.config.tolerant:
            self.tolerateUnexpectedToken(self.nextToken())
            body = self.finalize(self.createNode(), Node.EmptyStatement())
        else:
            self.expect(')')

            previousInIteration = self.context.inIteration
            self.context.inIteration = True
            body = self.isolateCoverGrammar(self.parseStatement)
            self.context.inIteration = previousInIteration

        if left is None:
            return self.finalize(node, Node.ForStatement(init, test, update, body))

        if forIn:
            return self.finalize(node, Node.ForInStatement(left, right, body))

        return self.finalize(node, Node.ForOfStatement(left, right, body))

    # https://tc39.github.io/ecma262/#sec-continue-statement

    def parseContinueStatement(self):
        node = self.createNode()
        self.expectKeyword('continue')

        label = None
        if self.lookahead.type is Token.Identifier and not self.hasLineTerminator:
            id = self.parseVariableIdentifier()
            label = id

            key = '$' + id.name
            if key not in self.context.labelSet:
                self.throwError(Messages.UnknownLabel, id.name)

        self.consumeSemicolon()
        if label is None and not self.context.inIteration:
            self.throwError(Messages.IllegalContinue)

        return self.finalize(node, Node.ContinueStatement(label))

    # https://tc39.github.io/ecma262/#sec-break-statement

    def parseBreakStatement(self):
        node = self.createNode()
        self.expectKeyword('break')

        label = None
        if self.lookahead.type is Token.Identifier and not self.hasLineTerminator:
            id = self.parseVariableIdentifier()

            key = '$' + id.name
            if key not in self.context.labelSet:
                self.throwError(Messages.UnknownLabel, id.name)
            label = id

        self.consumeSemicolon()
        if label is None and not self.context.inIteration and not self.context.inSwitch:
            self.throwError(Messages.IllegalBreak)

        return self.finalize(node, Node.BreakStatement(label))

    # https://tc39.github.io/ecma262/#sec-return-statement

    def parseReturnStatement(self):
        if not self.context.inFunctionBody:
            self.tolerateError(Messages.IllegalReturn)

        node = self.createNode()
        self.expectKeyword('return')

        hasArgument = (
            (
                not self.match(';') and not self.match('}') and
                not self.hasLineTerminator and self.lookahead.type is not Token.EOF
            ) or
            self.lookahead.type is Token.StringLiteral or
            self.lookahead.type is Token.Template
        )
        argument = self.parseExpression() if hasArgument else None
        self.consumeSemicolon()

        return self.finalize(node, Node.ReturnStatement(argument))

    # https://tc39.github.io/ecma262/#sec-with-statement

    def parseWithStatement(self):
        if self.context.strict:
            self.tolerateError(Messages.StrictModeWith)

        node = self.createNode()

        self.expectKeyword('with')
        self.expect('(')
        object = self.parseExpression()

        if not self.match(')') and self.config.tolerant:
            self.tolerateUnexpectedToken(self.nextToken())
            body = self.finalize(self.createNode(), Node.EmptyStatement())
        else:
            self.expect(')')
            body = self.parseStatement()

        return self.finalize(node, Node.WithStatement(object, body))

    # https://tc39.github.io/ecma262/#sec-switch-statement

    def parseSwitchCase(self):
        node = self.createNode()

        if self.matchKeyword('default'):
            self.nextToken()
            test = None
        else:
            self.expectKeyword('case')
            test = self.parseExpression()
        self.expect(':')

        consequent = []
        while True:
            if self.match('}') or self.matchKeyword('default', 'case'):
                break
            consequent.append(self.parseStatementListItem())

        return self.finalize(node, Node.SwitchCase(test, consequent))

    def parseSwitchStatement(self):
        node = self.createNode()
        self.expectKeyword('switch')

        self.expect('(')
        discriminant = self.parseExpression()
        self.expect(')')

        previousInSwitch = self.context.inSwitch
        self.context.inSwitch = True

        cases = []
        defaultFound = False
        self.expect('{')
        while True:
            if self.match('}'):
                break
            clause = self.parseSwitchCase()
            if clause.test is None:
                if defaultFound:
                    self.throwError(Messages.MultipleDefaultsInSwitch)
                defaultFound = True
            cases.append(clause)
        self.expect('}')

        self.context.inSwitch = previousInSwitch

        return self.finalize(node, Node.SwitchStatement(discriminant, cases))

    # https://tc39.github.io/ecma262/#sec-labelled-statements

    def parseLabelledStatement(self):
        node = self.createNode()
        expr = self.parseExpression()

        if expr.type is Syntax.Identifier and self.match(':'):
            self.nextToken()

            id = expr
            key = '$' + id.name
            if key in self.context.labelSet:
                self.throwError(Messages.Redeclaration, 'Label', id.name)

            self.context.labelSet[key] = True
            if self.matchKeyword('class'):
                self.tolerateUnexpectedToken(self.lookahead)
                body = self.parseClassDeclaration()
            elif self.matchKeyword('function'):
                token = self.lookahead
                declaration = self.parseFunctionDeclaration()
                if self.context.strict:
                    self.tolerateUnexpectedToken(token, Messages.StrictFunction)
                elif declaration.generator:
                    self.tolerateUnexpectedToken(token, Messages.GeneratorInLegacyContext)
                body = declaration
            else:
                body = self.parseStatement()
            del self.context.labelSet[key]

            statement = Node.LabeledStatement(id, body)
        else:
            self.consumeSemicolon()
            statement = Node.ExpressionStatement(expr)

        return self.finalize(node, statement)

    # https://tc39.github.io/ecma262/#sec-throw-statement

    def parseThrowStatement(self):
        node = self.createNode()
        self.expectKeyword('throw')

        if self.hasLineTerminator:
            self.throwError(Messages.NewlineAfterThrow)

        argument = self.parseExpression()
        self.consumeSemicolon()

        return self.finalize(node, Node.ThrowStatement(argument))

    # https://tc39.github.io/ecma262/#sec-try-statement

    def parseCatchClause(self):
        node = self.createNode()

        self.expectKeyword('catch')

        self.expect('(')
        if self.match(')'):
            self.throwUnexpectedToken(self.lookahead)

        params = []
        param = self.parsePattern(params)
        paramMap = {}
        for p in params:
            key = '$' + p.value
            if key in paramMap:
                self.tolerateError(Messages.DuplicateBinding, p.value)
            paramMap[key] = True

        if self.context.strict and param.type is Syntax.Identifier:
            if self.scanner.isRestrictedWord(param.name):
                self.tolerateError(Messages.StrictCatchVariable)

        self.expect(')')
        body = self.parseBlock()

        return self.finalize(node, Node.CatchClause(param, body))

    def parseFinallyClause(self):
        self.expectKeyword('finally')
        return self.parseBlock()

    def parseTryStatement(self):
        node = self.createNode()
        self.expectKeyword('try')

        block = self.parseBlock()
        handler = self.parseCatchClause() if self.matchKeyword('catch') else None
        finalizer = self.parseFinallyClause() if self.matchKeyword('finally') else None

        if not handler and not finalizer:
            self.throwError(Messages.NoCatchOrFinally)

        return self.finalize(node, Node.TryStatement(block, handler, finalizer))

    # https://tc39.github.io/ecma262/#sec-debugger-statement

    def parseDebuggerStatement(self):
        node = self.createNode()
        self.expectKeyword('debugger')
        self.consumeSemicolon()
        return self.finalize(node, Node.DebuggerStatement())

    # https://tc39.github.io/ecma262/#sec-ecmascript-language-statements-and-declarations

    def parseStatement(self):
        typ = self.lookahead.type
        if typ in (
            Token.BooleanLiteral,
            Token.NullLiteral,
            Token.NumericLiteral,
            Token.StringLiteral,
            Token.Template,
            Token.RegularExpression,
        ):
            statement = self.parseExpressionStatement()

        elif typ is Token.Punctuator:
            value = self.lookahead.value
            if value == '{':
                statement = self.parseBlock()
            elif value == '(':
                statement = self.parseExpressionStatement()
            elif value == ';':
                statement = self.parseEmptyStatement()
            else:
                statement = self.parseExpressionStatement()

        elif typ is Token.Identifier:
            statement = self.parseFunctionDeclaration() if self.matchAsyncFunction() else self.parseLabelledStatement()

        elif typ is Token.Keyword:
            value = self.lookahead.value
            if value == 'break':
                statement = self.parseBreakStatement()
            elif value == 'continue':
                statement = self.parseContinueStatement()
            elif value == 'debugger':
                statement = self.parseDebuggerStatement()
            elif value == 'do':
                statement = self.parseDoWhileStatement()
            elif value == 'for':
                statement = self.parseForStatement()
            elif value == 'function':
                statement = self.parseFunctionDeclaration()
            elif value == 'if':
                statement = self.parseIfStatement()
            elif value == 'return':
                statement = self.parseReturnStatement()
            elif value == 'switch':
                statement = self.parseSwitchStatement()
            elif value == 'throw':
                statement = self.parseThrowStatement()
            elif value == 'try':
                statement = self.parseTryStatement()
            elif value == 'var':
                statement = self.parseVariableStatement()
            elif value == 'while':
                statement = self.parseWhileStatement()
            elif value == 'with':
                statement = self.parseWithStatement()
            else:
                statement = self.parseExpressionStatement()

        else:
            statement = self.throwUnexpectedToken(self.lookahead)

        return statement

    # https://tc39.github.io/ecma262/#sec-function-definitions

    def parseFunctionSourceElements(self):
        node = self.createNode()

        self.expect('{')
        body = self.parseDirectivePrologues()

        previousLabelSet = self.context.labelSet
        previousInIteration = self.context.inIteration
        previousInSwitch = self.context.inSwitch
        previousInFunctionBody = self.context.inFunctionBody

        self.context.labelSet = {}
        self.context.inIteration = False
        self.context.inSwitch = False
        self.context.inFunctionBody = True

        while self.lookahead.type is not Token.EOF:
            if self.match('}'):
                break
            body.append(self.parseStatementListItem())

        self.expect('}')

        self.context.labelSet = previousLabelSet
        self.context.inIteration = previousInIteration
        self.context.inSwitch = previousInSwitch
        self.context.inFunctionBody = previousInFunctionBody

        return self.finalize(node, Node.BlockStatement(body))

    def validateParam(self, options, param, name):
        key = '$' + name
        if self.context.strict:
            if self.scanner.isRestrictedWord(name):
                options.stricted = param
                options.message = Messages.StrictParamName
            if key in options.paramSet:
                options.stricted = param
                options.message = Messages.StrictParamDupe
        elif not options.firstRestricted:
            if self.scanner.isRestrictedWord(name):
                options.firstRestricted = param
                options.message = Messages.StrictParamName
            elif self.scanner.isStrictModeReservedWord(name):
                options.firstRestricted = param
                options.message = Messages.StrictReservedWord
            elif key in options.paramSet:
                options.stricted = param
                options.message = Messages.StrictParamDupe

        options.paramSet[key] = True

    def parseRestElement(self, params):
        node = self.createNode()

        self.expect('...')
        arg = self.parsePattern(params)
        if self.match('='):
            self.throwError(Messages.DefaultRestParameter)
        if not self.match(')'):
            self.throwError(Messages.ParameterAfterRestParameter)

        return self.finalize(node, Node.RestElement(arg))

    def parseFormalParameter(self, options):
        params = []
        param = self.parseRestElement(params) if self.match('...') else self.parsePatternWithDefault(params)
        for p in params:
            self.validateParam(options, p, p.value)
        options.simple = options.simple and isinstance(param, Node.Identifier)
        options.params.append(param)

    def parseFormalParameters(self, firstRestricted=None):
        options = Params(
            simple=True,
            params=[],
            firstRestricted=firstRestricted
        )

        self.expect('(')
        if not self.match(')'):
            options.paramSet = {}
            while self.lookahead.type is not Token.EOF:
                self.parseFormalParameter(options)
                if self.match(')'):
                    break
                self.expect(',')
                if self.match(')'):
                    break
        self.expect(')')

        return Params(
            simple=options.simple,
            params=options.params,
            stricted=options.stricted,
            firstRestricted=options.firstRestricted,
            message=options.message
        )

    def matchAsyncFunction(self):
        match = self.matchContextualKeyword('async')
        if match:
            state = self.scanner.saveState()
            self.scanner.scanComments()
            next = self.scanner.lex()
            self.scanner.restoreState(state)

            match = (state.lineNumber == next.lineNumber) and (next.type is Token.Keyword) and (next.value == 'function')

        return match

    def parseFunctionDeclaration(self, identifierIsOptional=False):
        node = self.createNode()

        isAsync = self.matchContextualKeyword('async')
        if isAsync:
            self.nextToken()

        self.expectKeyword('function')

        isGenerator = False if isAsync else self.match('*')
        if isGenerator:
            self.nextToken()

        id = None
        firstRestricted = None

        if not identifierIsOptional or not self.match('('):
            token = self.lookahead
            id = self.parseVariableIdentifier()
            if self.context.strict:
                if self.scanner.isRestrictedWord(token.value):
                    self.tolerateUnexpectedToken(token, Messages.StrictFunctionName)
            else:
                if self.scanner.isRestrictedWord(token.value):
                    firstRestricted = token
                    message = Messages.StrictFunctionName
                elif self.scanner.isStrictModeReservedWord(token.value):
                    firstRestricted = token
                    message = Messages.StrictReservedWord

        previousAllowAwait = self.context.allowAwait
        previousAllowYield = self.context.allowYield
        self.context.allowAwait = isAsync
        self.context.allowYield = not isGenerator

        formalParameters = self.parseFormalParameters(firstRestricted)
        params = formalParameters.params
        stricted = formalParameters.stricted
        firstRestricted = formalParameters.firstRestricted
        if formalParameters.message:
            message = formalParameters.message

        previousStrict = self.context.strict
        previousAllowStrictDirective = self.context.allowStrictDirective
        self.context.allowStrictDirective = formalParameters.simple
        body = self.parseFunctionSourceElements()
        if self.context.strict and firstRestricted:
            self.throwUnexpectedToken(firstRestricted, message)
        if self.context.strict and stricted:
            self.tolerateUnexpectedToken(stricted, message)

        self.context.strict = previousStrict
        self.context.allowStrictDirective = previousAllowStrictDirective
        self.context.allowAwait = previousAllowAwait
        self.context.allowYield = previousAllowYield

        if isAsync:
            return self.finalize(node, Node.AsyncFunctionDeclaration(id, params, body))

        return self.finalize(node, Node.FunctionDeclaration(id, params, body, isGenerator))

    def parseFunctionExpression(self):
        node = self.createNode()

        isAsync = self.matchContextualKeyword('async')
        if isAsync:
            self.nextToken()

        self.expectKeyword('function')

        isGenerator = False if isAsync else self.match('*')
        if isGenerator:
            self.nextToken()

        id = None
        firstRestricted = None

        previousAllowAwait = self.context.allowAwait
        previousAllowYield = self.context.allowYield
        self.context.allowAwait = isAsync
        self.context.allowYield = not isGenerator

        if not self.match('('):
            token = self.lookahead
            id = self.parseIdentifierName() if not self.context.strict and not isGenerator and self.matchKeyword('yield') else self.parseVariableIdentifier()
            if self.context.strict:
                if self.scanner.isRestrictedWord(token.value):
                    self.tolerateUnexpectedToken(token, Messages.StrictFunctionName)
            else:
                if self.scanner.isRestrictedWord(token.value):
                    firstRestricted = token
                    message = Messages.StrictFunctionName
                elif self.scanner.isStrictModeReservedWord(token.value):
                    firstRestricted = token
                    message = Messages.StrictReservedWord

        formalParameters = self.parseFormalParameters(firstRestricted)
        params = formalParameters.params
        stricted = formalParameters.stricted
        firstRestricted = formalParameters.firstRestricted
        if formalParameters.message:
            message = formalParameters.message

        previousStrict = self.context.strict
        previousAllowStrictDirective = self.context.allowStrictDirective
        self.context.allowStrictDirective = formalParameters.simple
        body = self.parseFunctionSourceElements()
        if self.context.strict and firstRestricted:
            self.throwUnexpectedToken(firstRestricted, message)
        if self.context.strict and stricted:
            self.tolerateUnexpectedToken(stricted, message)
        self.context.strict = previousStrict
        self.context.allowStrictDirective = previousAllowStrictDirective
        self.context.allowAwait = previousAllowAwait
        self.context.allowYield = previousAllowYield

        if isAsync:
            return self.finalize(node, Node.AsyncFunctionExpression(id, params, body))

        return self.finalize(node, Node.FunctionExpression(id, params, body, isGenerator))

    # https://tc39.github.io/ecma262/#sec-directive-prologues-and-the-use-strict-directive

    def parseDirective(self):
        token = self.lookahead

        node = self.createNode()
        expr = self.parseExpression()
        directive = self.getTokenRaw(token)[1:-1] if expr.type is Syntax.Literal else None
        self.consumeSemicolon()

        return self.finalize(node, Node.Directive(expr, directive) if directive else Node.ExpressionStatement(expr))

    def parseDirectivePrologues(self):
        firstRestricted = None

        body = []
        while True:
            token = self.lookahead
            if token.type is not Token.StringLiteral:
                break

            statement = self.parseDirective()
            body.append(statement)
            directive = statement.directive
            if not isinstance(directive, basestring):
                break

            if directive == 'use strict':
                self.context.strict = True
                if firstRestricted:
                    self.tolerateUnexpectedToken(firstRestricted, Messages.StrictOctalLiteral)
                if not self.context.allowStrictDirective:
                    self.tolerateUnexpectedToken(token, Messages.IllegalLanguageModeDirective)
            else:
                if not firstRestricted and token.octal:
                    firstRestricted = token

        return body

    # https://tc39.github.io/ecma262/#sec-method-definitions

    def qualifiedPropertyName(self, token):
        typ = token.type
        if typ in (
            Token.Identifier,
            Token.StringLiteral,
            Token.BooleanLiteral,
            Token.NullLiteral,
            Token.NumericLiteral,
            Token.Keyword,
        ):
            return True
        elif typ is Token.Punctuator:
            return token.value == '['
        return False

    def parseGetterMethod(self):
        node = self.createNode()

        isGenerator = False
        previousAllowYield = self.context.allowYield
        self.context.allowYield = not isGenerator
        formalParameters = self.parseFormalParameters()
        if len(formalParameters.params) > 0:
            self.tolerateError(Messages.BadGetterArity)
        method = self.parsePropertyMethod(formalParameters)
        self.context.allowYield = previousAllowYield

        return self.finalize(node, Node.FunctionExpression(None, formalParameters.params, method, isGenerator))

    def parseSetterMethod(self):
        node = self.createNode()

        isGenerator = False
        previousAllowYield = self.context.allowYield
        self.context.allowYield = not isGenerator
        formalParameters = self.parseFormalParameters()
        if len(formalParameters.params) != 1:
            self.tolerateError(Messages.BadSetterArity)
        elif isinstance(formalParameters.params[0], Node.RestElement):
            self.tolerateError(Messages.BadSetterRestParameter)
        method = self.parsePropertyMethod(formalParameters)
        self.context.allowYield = previousAllowYield

        return self.finalize(node, Node.FunctionExpression(None, formalParameters.params, method, isGenerator))

    def parseGeneratorMethod(self):
        node = self.createNode()

        isGenerator = True
        previousAllowYield = self.context.allowYield

        self.context.allowYield = True
        params = self.parseFormalParameters()
        self.context.allowYield = False
        method = self.parsePropertyMethod(params)
        self.context.allowYield = previousAllowYield

        return self.finalize(node, Node.FunctionExpression(None, params.params, method, isGenerator))

    # https://tc39.github.io/ecma262/#sec-generator-function-definitions

    def isStartOfExpression(self):
        start = True

        value = self.lookahead.value
        typ = self.lookahead.type
        if typ is Token.Punctuator:
            start = value in ('[', '(', '{', '+', '-', '!', '~', '++', '--', '/', '/=')  # regular expression literal )

        elif typ is Token.Keyword:
            start = value in ('class', 'delete', 'function', 'let', 'new', 'super', 'this', 'typeof', 'void', 'yield')

        return start

    def parseYieldExpression(self):
        node = self.createNode()
        self.expectKeyword('yield')

        argument = None
        delegate = False
        if not self.hasLineTerminator:
            previousAllowYield = self.context.allowYield
            self.context.allowYield = False
            delegate = self.match('*')
            if delegate:
                self.nextToken()
                argument = self.parseAssignmentExpression()
            elif self.isStartOfExpression():
                argument = self.parseAssignmentExpression()
            self.context.allowYield = previousAllowYield

        return self.finalize(node, Node.YieldExpression(argument, delegate))

    # https://tc39.github.io/ecma262/#sec-class-definitions

    def parseClassElement(self, hasConstructor):
        token = self.lookahead
        node = self.createNode()

        kind = ''
        key = None
        value = None
        computed = False
        isStatic = False
        isAsync = False

        if self.match('*'):
            self.nextToken()

        else:
            computed = self.match('[')
            key = self.parseObjectPropertyKey()
            id = key
            if id.name == 'static' and (self.qualifiedPropertyName(self.lookahead) or self.match('*')):
                token = self.lookahead
                isStatic = True
                computed = self.match('[')
                if self.match('*'):
                    self.nextToken()
                else:
                    key = self.parseObjectPropertyKey()
            if token.type is Token.Identifier and not self.hasLineTerminator and token.value == 'async':
                punctuator = self.lookahead.value
                if punctuator != ':' and punctuator != '(' and punctuator != '*':
                    isAsync = True
                    token = self.lookahead
                    key = self.parseObjectPropertyKey()
                    if token.type is Token.Identifier and token.value == 'constructor':
                        self.tolerateUnexpectedToken(token, Messages.ConstructorIsAsync)

        lookaheadPropertyKey = self.qualifiedPropertyName(self.lookahead)
        if token.type is Token.Identifier:
            if token.value == 'get' and lookaheadPropertyKey:
                kind = 'get'
                computed = self.match('[')
                key = self.parseObjectPropertyKey()
                self.context.allowYield = False
                value = self.parseGetterMethod()
            elif token.value == 'set' and lookaheadPropertyKey:
                kind = 'set'
                computed = self.match('[')
                key = self.parseObjectPropertyKey()
                value = self.parseSetterMethod()
            elif self.config.classProperties and not self.match('('):
                kind = 'init'
                id = self.finalize(node, Node.Identifier(token.value))
                if self.match('='):
                    self.nextToken()
                    value = self.parseAssignmentExpression()

        elif token.type is Token.Punctuator and token.value == '*' and lookaheadPropertyKey:
            kind = 'method'
            computed = self.match('[')
            key = self.parseObjectPropertyKey()
            value = self.parseGeneratorMethod()

        if not kind and key and self.match('('):
            kind = 'method'
            value = self.parsePropertyMethodAsyncFunction() if isAsync else self.parsePropertyMethodFunction()

        if not kind:
            self.throwUnexpectedToken(self.lookahead)

        if not computed:
            if isStatic and self.isPropertyKey(key, 'prototype'):
                self.throwUnexpectedToken(token, Messages.StaticPrototype)
            if not isStatic and self.isPropertyKey(key, 'constructor'):
                if kind != 'method' or (value and value.generator):
                    self.throwUnexpectedToken(token, Messages.ConstructorSpecialMethod)
                if hasConstructor.value:
                    self.throwUnexpectedToken(token, Messages.DuplicateConstructor)
                else:
                    hasConstructor.value = True
                kind = 'constructor'

        if kind in ('constructor', 'method', 'get', 'set'):
            return self.finalize(node, Node.MethodDefinition(key, computed, value, kind, isStatic))

        else:
            return self.finalize(node, Node.FieldDefinition(key, computed, value, kind, isStatic))

    def parseClassElementList(self):
        body = []
        hasConstructor = Value(False)

        self.expect('{')
        while not self.match('}'):
            if self.match(';'):
                self.nextToken()
            else:
                body.append(self.parseClassElement(hasConstructor))
        self.expect('}')

        return body

    def parseClassBody(self):
        node = self.createNode()
        elementList = self.parseClassElementList()

        return self.finalize(node, Node.ClassBody(elementList))

    def parseClassDeclaration(self, identifierIsOptional=False):
        node = self.createNode()

        previousStrict = self.context.strict
        self.context.strict = True
        self.expectKeyword('class')

        id = None if identifierIsOptional and self.lookahead.type is not Token.Identifier else self.parseVariableIdentifier()
        superClass = None
        if self.matchKeyword('extends'):
            self.nextToken()
            superClass = self.isolateCoverGrammar(self.parseLeftHandSideExpressionAllowCall)
        classBody = self.parseClassBody()
        self.context.strict = previousStrict

        return self.finalize(node, Node.ClassDeclaration(id, superClass, classBody))

    def parseClassExpression(self):
        node = self.createNode()

        previousStrict = self.context.strict
        self.context.strict = True
        self.expectKeyword('class')
        id = self.parseVariableIdentifier() if self.lookahead.type is Token.Identifier else None
        superClass = None
        if self.matchKeyword('extends'):
            self.nextToken()
            superClass = self.isolateCoverGrammar(self.parseLeftHandSideExpressionAllowCall)
        classBody = self.parseClassBody()
        self.context.strict = previousStrict

        return self.finalize(node, Node.ClassExpression(id, superClass, classBody))

    # https://tc39.github.io/ecma262/#sec-scripts
    # https://tc39.github.io/ecma262/#sec-modules

    def parseModule(self):
        self.context.strict = True
        self.context.isModule = True
        self.scanner.isModule = True
        node = self.createNode()
        body = self.parseDirectivePrologues()
        while self.lookahead.type is not Token.EOF:
            body.append(self.parseStatementListItem())
        return self.finalize(node, Node.Module(body))

    def parseScript(self):
        node = self.createNode()
        body = self.parseDirectivePrologues()
        while self.lookahead.type is not Token.EOF:
            body.append(self.parseStatementListItem())
        return self.finalize(node, Node.Script(body))

    # https://tc39.github.io/ecma262/#sec-imports

    def parseModuleSpecifier(self):
        node = self.createNode()

        if self.lookahead.type is not Token.StringLiteral:
            self.throwError(Messages.InvalidModuleSpecifier)

        token = self.nextToken()
        raw = self.getTokenRaw(token)
        return self.finalize(node, Node.Literal(token.value, raw))

    # import {<foo as bar>} ...
    def parseImportSpecifier(self):
        node = self.createNode()

        if self.lookahead.type is Token.Identifier:
            imported = self.parseVariableIdentifier()
            local = imported
            if self.matchContextualKeyword('as'):
                self.nextToken()
                local = self.parseVariableIdentifier()
        else:
            imported = self.parseIdentifierName()
            local = imported
            if self.matchContextualKeyword('as'):
                self.nextToken()
                local = self.parseVariableIdentifier()
            else:
                self.throwUnexpectedToken(self.nextToken())

        return self.finalize(node, Node.ImportSpecifier(local, imported))

    # {foo, bar as bas
    def parseNamedImports(self):
        self.expect('{')
        specifiers = []
        while not self.match('}'):
            specifiers.append(self.parseImportSpecifier())
            if not self.match('}'):
                self.expect(',')
        self.expect('}')

        return specifiers

    # import <foo> ...
    def parseImportDefaultSpecifier(self):
        node = self.createNode()
        local = self.parseIdentifierName()
        return self.finalize(node, Node.ImportDefaultSpecifier(local))

    # import <* as foo> ...
    def parseImportNamespaceSpecifier(self):
        node = self.createNode()

        self.expect('*')
        if not self.matchContextualKeyword('as'):
            self.throwError(Messages.NoAsAfterImportNamespace)
        self.nextToken()
        local = self.parseIdentifierName()

        return self.finalize(node, Node.ImportNamespaceSpecifier(local))

    def parseImportDeclaration(self):
        if self.context.inFunctionBody:
            self.throwError(Messages.IllegalImportDeclaration)

        node = self.createNode()
        self.expectKeyword('import')

        specifiers = []
        if self.lookahead.type is Token.StringLiteral:
            # import 'foo'
            src = self.parseModuleSpecifier()
        else:
            if self.match('{'):
                # import {bar
                specifiers.extend(self.parseNamedImports())
            elif self.match('*'):
                # import * as foo
                specifiers.append(self.parseImportNamespaceSpecifier())
            elif self.isIdentifierName(self.lookahead) and not self.matchKeyword('default'):
                # import foo
                specifiers.append(self.parseImportDefaultSpecifier())
                if self.match(','):
                    self.nextToken()
                    if self.match('*'):
                        # import foo, * as foo
                        specifiers.append(self.parseImportNamespaceSpecifier())
                    elif self.match('{'):
                        # import foo, {bar
                        specifiers.extend(self.parseNamedImports())
                    else:
                        self.throwUnexpectedToken(self.lookahead)
            else:
                self.throwUnexpectedToken(self.nextToken())

            if not self.matchContextualKeyword('from'):
                message = Messages.UnexpectedToken if self.lookahead.value else Messages.MissingFromClause
                self.throwError(message, self.lookahead.value)
            self.nextToken()
            src = self.parseModuleSpecifier()
        self.consumeSemicolon()

        return self.finalize(node, Node.ImportDeclaration(specifiers, src))

    # https://tc39.github.io/ecma262/#sec-exports

    def parseExportSpecifier(self):
        node = self.createNode()

        local = self.parseIdentifierName()
        exported = local
        if self.matchContextualKeyword('as'):
            self.nextToken()
            exported = self.parseIdentifierName()

        return self.finalize(node, Node.ExportSpecifier(local, exported))

    def parseExportDefaultSpecifier(self):
        node = self.createNode()
        local = self.parseIdentifierName()
        return self.finalize(node, Node.ExportDefaultSpecifier(local))

    def parseExportDeclaration(self):
        if self.context.inFunctionBody:
            self.throwError(Messages.IllegalExportDeclaration)

        node = self.createNode()
        self.expectKeyword('export')

        if self.matchKeyword('default'):
            # export default ...
            self.nextToken()
            if self.matchKeyword('function'):
                # export default function foo (:
                # export default function (:
                declaration = self.parseFunctionDeclaration(True)
                exportDeclaration = self.finalize(node, Node.ExportDefaultDeclaration(declaration))
            elif self.matchKeyword('class'):
                # export default class foo {
                declaration = self.parseClassDeclaration(True)
                exportDeclaration = self.finalize(node, Node.ExportDefaultDeclaration(declaration))
            elif self.matchContextualKeyword('async'):
                # export default async function f (:
                # export default async function (:
                # export default async x => x
                declaration = self.parseFunctionDeclaration(True) if self.matchAsyncFunction() else self.parseAssignmentExpression()
                exportDeclaration = self.finalize(node, Node.ExportDefaultDeclaration(declaration))
            else:
                if self.matchContextualKeyword('from'):
                    self.throwError(Messages.UnexpectedToken, self.lookahead.value)
                # export default {}
                # export default []
                # export default (1 + 2)
                if self.match('{'):
                    declaration = self.parseObjectInitializer()
                elif self.match('['):
                    declaration = self.parseArrayInitializer()
                else:
                    declaration = self.parseAssignmentExpression()
                self.consumeSemicolon()
                exportDeclaration = self.finalize(node, Node.ExportDefaultDeclaration(declaration))

        elif self.match('*'):
            # export * from 'foo'
            self.nextToken()
            if not self.matchContextualKeyword('from'):
                message = Messages.UnexpectedToken if self.lookahead.value else Messages.MissingFromClause
                self.throwError(message, self.lookahead.value)
            self.nextToken()
            src = self.parseModuleSpecifier()
            self.consumeSemicolon()
            exportDeclaration = self.finalize(node, Node.ExportAllDeclaration(src))

        elif self.lookahead.type is Token.Keyword:
            # export var f = 1
            value = self.lookahead.value
            if value in (
                'let',
                'const',
            ):
                declaration = self.parseLexicalDeclaration(Params(inFor=False))
            elif value in (
                'var',
                'class',
                'function',
            ):
                declaration = self.parseStatementListItem()
            else:
                self.throwUnexpectedToken(self.lookahead)
            exportDeclaration = self.finalize(node, Node.ExportNamedDeclaration(declaration, [], None))

        elif self.matchAsyncFunction():
            declaration = self.parseFunctionDeclaration()
            exportDeclaration = self.finalize(node, Node.ExportNamedDeclaration(declaration, [], None))

        else:
            specifiers = []
            source = None
            isExportFromIdentifier = False

            expectSpecifiers = True
            if self.lookahead.type is Token.Identifier:
                specifiers.append(self.parseExportDefaultSpecifier())
                if self.match(','):
                    self.nextToken()
                else:
                    expectSpecifiers = False

            if expectSpecifiers:
                self.expect('{')
                while not self.match('}'):
                    isExportFromIdentifier = isExportFromIdentifier or self.matchKeyword('default')
                    specifiers.append(self.parseExportSpecifier())
                    if not self.match('}'):
                        self.expect(',')
                self.expect('}')

            if self.matchContextualKeyword('from'):
                # export {default} from 'foo'
                # export {foo} from 'foo'
                self.nextToken()
                source = self.parseModuleSpecifier()
                self.consumeSemicolon()
            elif isExportFromIdentifier:
                # export {default}; # missing fromClause
                message = Messages.UnexpectedToken if self.lookahead.value else Messages.MissingFromClause
                self.throwError(message, self.lookahead.value)
            else:
                # export {foo}
                self.consumeSemicolon()
            exportDeclaration = self.finalize(node, Node.ExportNamedDeclaration(None, specifiers, source))

        return exportDeclaration
