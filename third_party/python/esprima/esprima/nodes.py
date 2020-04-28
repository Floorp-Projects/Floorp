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
from .syntax import Syntax
from .scanner import RegExp


class Node(Object):
    def __dir__(self):
        return list(self.__dict__.keys())

    def __iter__(self):
        return self.__iter__

    def keys(self):
        return self.__dict__.keys()

    def items(self):
        return self.__dict__.items()


class ArrayExpression(Node):
    def __init__(self, elements):
        self.type = Syntax.ArrayExpression
        self.elements = elements


class ArrayPattern(Node):
    def __init__(self, elements):
        self.type = Syntax.ArrayPattern
        self.elements = elements


class ArrowFunctionExpression(Node):
    def __init__(self, params, body, expression):
        self.type = Syntax.ArrowFunctionExpression
        self.generator = False
        self.isAsync = False
        self.params = params
        self.body = body
        self.expression = expression


class AssignmentExpression(Node):
    def __init__(self, operator, left, right):
        self.type = Syntax.AssignmentExpression
        self.operator = operator
        self.left = left
        self.right = right


class AssignmentPattern(Node):
    def __init__(self, left, right):
        self.type = Syntax.AssignmentPattern
        self.left = left
        self.right = right


class AsyncArrowFunctionExpression(Node):
    def __init__(self, params, body, expression):
        self.type = Syntax.ArrowFunctionExpression
        self.generator = False
        self.isAsync = True
        self.params = params
        self.body = body
        self.expression = expression


class AsyncFunctionDeclaration(Node):
    def __init__(self, id, params, body):
        self.type = Syntax.FunctionDeclaration
        self.generator = False
        self.expression = False
        self.isAsync = True
        self.id = id
        self.params = params
        self.body = body


class AsyncFunctionExpression(Node):
    def __init__(self, id, params, body):
        self.type = Syntax.FunctionExpression
        self.generator = False
        self.expression = False
        self.isAsync = True
        self.id = id
        self.params = params
        self.body = body


class AwaitExpression(Node):
    def __init__(self, argument):
        self.type = Syntax.AwaitExpression
        self.argument = argument


class BinaryExpression(Node):
    def __init__(self, operator, left, right):
        self.type = Syntax.LogicalExpression if operator in ('||', '&&') else Syntax.BinaryExpression
        self.operator = operator
        self.left = left
        self.right = right


class BlockStatement(Node):
    def __init__(self, body):
        self.type = Syntax.BlockStatement
        self.body = body


class BreakStatement(Node):
    def __init__(self, label):
        self.type = Syntax.BreakStatement
        self.label = label


class CallExpression(Node):
    def __init__(self, callee, args):
        self.type = Syntax.CallExpression
        self.callee = callee
        self.arguments = args


class CatchClause(Node):
    def __init__(self, param, body):
        self.type = Syntax.CatchClause
        self.param = param
        self.body = body


class ClassBody(Node):
    def __init__(self, body):
        self.type = Syntax.ClassBody
        self.body = body


class ClassDeclaration(Node):
    def __init__(self, id, superClass, body):
        self.type = Syntax.ClassDeclaration
        self.id = id
        self.superClass = superClass
        self.body = body


class ClassExpression(Node):
    def __init__(self, id, superClass, body):
        self.type = Syntax.ClassExpression
        self.id = id
        self.superClass = superClass
        self.body = body


class ComputedMemberExpression(Node):
    def __init__(self, object, property):
        self.type = Syntax.MemberExpression
        self.computed = True
        self.object = object
        self.property = property


class ConditionalExpression(Node):
    def __init__(self, test, consequent, alternate):
        self.type = Syntax.ConditionalExpression
        self.test = test
        self.consequent = consequent
        self.alternate = alternate


class ContinueStatement(Node):
    def __init__(self, label):
        self.type = Syntax.ContinueStatement
        self.label = label


class DebuggerStatement(Node):
    def __init__(self):
        self.type = Syntax.DebuggerStatement


class Directive(Node):
    def __init__(self, expression, directive):
        self.type = Syntax.ExpressionStatement
        self.expression = expression
        self.directive = directive


class DoWhileStatement(Node):
    def __init__(self, body, test):
        self.type = Syntax.DoWhileStatement
        self.body = body
        self.test = test


class EmptyStatement(Node):
    def __init__(self):
        self.type = Syntax.EmptyStatement


class ExportAllDeclaration(Node):
    def __init__(self, source):
        self.type = Syntax.ExportAllDeclaration
        self.source = source


class ExportDefaultDeclaration(Node):
    def __init__(self, declaration):
        self.type = Syntax.ExportDefaultDeclaration
        self.declaration = declaration


class ExportNamedDeclaration(Node):
    def __init__(self, declaration, specifiers, source):
        self.type = Syntax.ExportNamedDeclaration
        self.declaration = declaration
        self.specifiers = specifiers
        self.source = source


class ExportSpecifier(Node):
    def __init__(self, local, exported):
        self.type = Syntax.ExportSpecifier
        self.exported = exported
        self.local = local


class ExportDefaultSpecifier(Node):
    def __init__(self, local):
        self.type = Syntax.ExportDefaultSpecifier
        self.local = local


class ExpressionStatement(Node):
    def __init__(self, expression):
        self.type = Syntax.ExpressionStatement
        self.expression = expression


class ForInStatement(Node):
    def __init__(self, left, right, body):
        self.type = Syntax.ForInStatement
        self.each = False
        self.left = left
        self.right = right
        self.body = body


class ForOfStatement(Node):
    def __init__(self, left, right, body):
        self.type = Syntax.ForOfStatement
        self.left = left
        self.right = right
        self.body = body


class ForStatement(Node):
    def __init__(self, init, test, update, body):
        self.type = Syntax.ForStatement
        self.init = init
        self.test = test
        self.update = update
        self.body = body


class FunctionDeclaration(Node):
    def __init__(self, id, params, body, generator):
        self.type = Syntax.FunctionDeclaration
        self.expression = False
        self.isAsync = False
        self.id = id
        self.params = params
        self.body = body
        self.generator = generator


class FunctionExpression(Node):
    def __init__(self, id, params, body, generator):
        self.type = Syntax.FunctionExpression
        self.expression = False
        self.isAsync = False
        self.id = id
        self.params = params
        self.body = body
        self.generator = generator


class Identifier(Node):
    def __init__(self, name):
        self.type = Syntax.Identifier
        self.name = name


class IfStatement(Node):
    def __init__(self, test, consequent, alternate):
        self.type = Syntax.IfStatement
        self.test = test
        self.consequent = consequent
        self.alternate = alternate


class Import(Node):
    def __init__(self):
        self.type = Syntax.Import


class ImportDeclaration(Node):
    def __init__(self, specifiers, source):
        self.type = Syntax.ImportDeclaration
        self.specifiers = specifiers
        self.source = source


class ImportDefaultSpecifier(Node):
    def __init__(self, local):
        self.type = Syntax.ImportDefaultSpecifier
        self.local = local


class ImportNamespaceSpecifier(Node):
    def __init__(self, local):
        self.type = Syntax.ImportNamespaceSpecifier
        self.local = local


class ImportSpecifier(Node):
    def __init__(self, local, imported):
        self.type = Syntax.ImportSpecifier
        self.local = local
        self.imported = imported


class LabeledStatement(Node):
    def __init__(self, label, body):
        self.type = Syntax.LabeledStatement
        self.label = label
        self.body = body


class Literal(Node):
    def __init__(self, value, raw):
        self.type = Syntax.Literal
        self.value = value
        self.raw = raw


class MetaProperty(Node):
    def __init__(self, meta, property):
        self.type = Syntax.MetaProperty
        self.meta = meta
        self.property = property


class MethodDefinition(Node):
    def __init__(self, key, computed, value, kind, isStatic):
        self.type = Syntax.MethodDefinition
        self.key = key
        self.computed = computed
        self.value = value
        self.kind = kind
        self.static = isStatic


class FieldDefinition(Node):
    def __init__(self, key, computed, value, kind, isStatic):
        self.type = Syntax.FieldDefinition
        self.key = key
        self.computed = computed
        self.value = value
        self.kind = kind
        self.static = isStatic


class Module(Node):
    def __init__(self, body):
        self.type = Syntax.Program
        self.sourceType = 'module'
        self.body = body


class NewExpression(Node):
    def __init__(self, callee, args):
        self.type = Syntax.NewExpression
        self.callee = callee
        self.arguments = args


class ObjectExpression(Node):
    def __init__(self, properties):
        self.type = Syntax.ObjectExpression
        self.properties = properties


class ObjectPattern(Node):
    def __init__(self, properties):
        self.type = Syntax.ObjectPattern
        self.properties = properties


class Property(Node):
    def __init__(self, kind, key, computed, value, method, shorthand):
        self.type = Syntax.Property
        self.key = key
        self.computed = computed
        self.value = value
        self.kind = kind
        self.method = method
        self.shorthand = shorthand


class RegexLiteral(Node):
    def __init__(self, value, raw, pattern, flags):
        self.type = Syntax.Literal
        self.value = value
        self.raw = raw
        self.regex = RegExp(
            pattern=pattern,
            flags=flags,
        )


class RestElement(Node):
    def __init__(self, argument):
        self.type = Syntax.RestElement
        self.argument = argument


class ReturnStatement(Node):
    def __init__(self, argument):
        self.type = Syntax.ReturnStatement
        self.argument = argument


class Script(Node):
    def __init__(self, body):
        self.type = Syntax.Program
        self.sourceType = 'script'
        self.body = body


class SequenceExpression(Node):
    def __init__(self, expressions):
        self.type = Syntax.SequenceExpression
        self.expressions = expressions


class SpreadElement(Node):
    def __init__(self, argument):
        self.type = Syntax.SpreadElement
        self.argument = argument


class StaticMemberExpression(Node):
    def __init__(self, object, property):
        self.type = Syntax.MemberExpression
        self.computed = False
        self.object = object
        self.property = property


class Super(Node):
    def __init__(self):
        self.type = Syntax.Super


class SwitchCase(Node):
    def __init__(self, test, consequent):
        self.type = Syntax.SwitchCase
        self.test = test
        self.consequent = consequent


class SwitchStatement(Node):
    def __init__(self, discriminant, cases):
        self.type = Syntax.SwitchStatement
        self.discriminant = discriminant
        self.cases = cases


class TaggedTemplateExpression(Node):
    def __init__(self, tag, quasi):
        self.type = Syntax.TaggedTemplateExpression
        self.tag = tag
        self.quasi = quasi


class TemplateElement(Node):
    class Value(Object):
        def __init__(self, raw, cooked):
            self.raw = raw
            self.cooked = cooked

    def __init__(self, raw, cooked, tail):
        self.type = Syntax.TemplateElement
        self.value = TemplateElement.Value(raw, cooked)
        self.tail = tail


class TemplateLiteral(Node):
    def __init__(self, quasis, expressions):
        self.type = Syntax.TemplateLiteral
        self.quasis = quasis
        self.expressions = expressions


class ThisExpression(Node):
    def __init__(self):
        self.type = Syntax.ThisExpression


class ThrowStatement(Node):
    def __init__(self, argument):
        self.type = Syntax.ThrowStatement
        self.argument = argument


class TryStatement(Node):
    def __init__(self, block, handler, finalizer):
        self.type = Syntax.TryStatement
        self.block = block
        self.handler = handler
        self.finalizer = finalizer


class UnaryExpression(Node):
    def __init__(self, operator, argument):
        self.type = Syntax.UnaryExpression
        self.prefix = True
        self.operator = operator
        self.argument = argument


class UpdateExpression(Node):
    def __init__(self, operator, argument, prefix):
        self.type = Syntax.UpdateExpression
        self.operator = operator
        self.argument = argument
        self.prefix = prefix


class VariableDeclaration(Node):
    def __init__(self, declarations, kind):
        self.type = Syntax.VariableDeclaration
        self.declarations = declarations
        self.kind = kind


class VariableDeclarator(Node):
    def __init__(self, id, init):
        self.type = Syntax.VariableDeclarator
        self.id = id
        self.init = init


class WhileStatement(Node):
    def __init__(self, test, body):
        self.type = Syntax.WhileStatement
        self.test = test
        self.body = body


class WithStatement(Node):
    def __init__(self, object, body):
        self.type = Syntax.WithStatement
        self.object = object
        self.body = body


class YieldExpression(Node):
    def __init__(self, argument, delegate):
        self.type = Syntax.YieldExpression
        self.argument = argument
        self.delegate = delegate


class ArrowParameterPlaceHolder(Node):
    def __init__(self, params):
        self.type = Syntax.ArrowParameterPlaceHolder
        self.params = params
        self.isAsync = False


class AsyncArrowParameterPlaceHolder(Node):
    def __init__(self, params):
        self.type = Syntax.ArrowParameterPlaceHolder
        self.params = params
        self.isAsync = True


class BlockComment(Node):
    def __init__(self, value):
        self.type = Syntax.BlockComment
        self.value = value


class LineComment(Node):
    def __init__(self, value):
        self.type = Syntax.LineComment
        self.value = value
