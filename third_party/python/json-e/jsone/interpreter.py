from __future__ import absolute_import, print_function, unicode_literals

from .prattparser import PrattParser, infix, prefix
from .shared import TemplateError, InterpreterError, string
import operator
import json

OPERATORS = {
    '-': operator.sub,
    '*': operator.mul,
    '/': operator.truediv,
    '**': operator.pow,
    '==': operator.eq,
    '!=': operator.ne,
    '<=': operator.le,
    '<': operator.lt,
    '>': operator.gt,
    '>=': operator.ge,
    '&&': lambda a, b: bool(a and b),
    '||': lambda a, b: bool(a or b),
}


def infixExpectationError(operator, expected):
    return InterpreterError('infix: {} expects {} {} {}'.
                            format(operator, expected, operator, expected))


class ExpressionEvaluator(PrattParser):

    ignore = '\\s+'
    patterns = {
        'number': '[0-9]+(?:\\.[0-9]+)?',
        'identifier': '[a-zA-Z_][a-zA-Z_0-9]*',
        'string': '\'[^\']*\'|"[^"]*"',
        # avoid matching these as prefixes of identifiers e.g., `insinutations`
        'true': 'true(?![a-zA-Z_0-9])',
        'false': 'false(?![a-zA-Z_0-9])',
        'in': 'in(?![a-zA-Z_0-9])',
        'null': 'null(?![a-zA-Z_0-9])',
    }
    tokens = [
        '**', '+', '-', '*', '/', '[', ']', '.', '(', ')', '{', '}', ':', ',',
        '>=', '<=', '<', '>', '==', '!=', '!', '&&', '||', 'true', 'false', 'in',
        'null', 'number', 'identifier', 'string',
    ]
    precedence = [
        ['||'],
        ['&&'],
        ['in'],
        ['==', '!='],
        ['>=', '<=', '<', '>'],
        ['+', '-'],
        ['*', '/'],
        ['**-right-associative'],
        ['**'],
        ['[', '.'],
        ['('],
        ['unary'],
    ]

    def __init__(self, context):
        super(ExpressionEvaluator, self).__init__()
        self.context = context

    def parse(self, expression):
        if not isinstance(expression, string):
            raise TemplateError('expression to be evaluated must be a string')
        return super(ExpressionEvaluator, self).parse(expression)

    @prefix('number')
    def number(self, token, pc):
        v = token.value
        return float(v) if '.' in v else int(v)

    @prefix("!")
    def bang(self, token, pc):
        return not pc.parse('unary')

    @prefix("-")
    def uminus(self, token, pc):
        v = pc.parse('unary')
        if not isNumber(v):
            raise InterpreterError('{} expects {}'.format('unary -', 'number'))
        return -v

    @prefix("+")
    def uplus(self, token, pc):
        v = pc.parse('unary')
        if not isNumber(v):
            raise InterpreterError('{} expects {}'.format('unary +', 'number'))
        return v

    @prefix("identifier")
    def identifier(self, token, pc):
        try:
            return self.context[token.value]
        except KeyError:
            raise InterpreterError(
                'unknown context value {}'.format(token.value))

    @prefix("null")
    def null(self, token, pc):
        return None

    @prefix("[")
    def array_bracket(self, token, pc):
        return parseList(pc, ',', ']')

    @prefix("(")
    def grouping_paren(self, token, pc):
        rv = pc.parse()
        pc.require(')')
        return rv

    @prefix("{")
    def object_brace(self, token, pc):
        return parseObject(pc)

    @prefix("string")
    def string(self, token, pc):
        return parseString(token.value)

    @prefix("true")
    def true(self, token, pc):
        return True

    @prefix("false")
    def false(self, token, ps):
        return False

    @infix("+")
    def plus(self, left, token, pc):
        if not isinstance(left, (string, int, float)) or isinstance(left, bool):
            raise infixExpectationError('+', 'number/string')
        right = pc.parse(token.kind)
        if not isinstance(right, (string, int, float)) or isinstance(right, bool):
            raise infixExpectationError('+', 'number/string')
        if type(right) != type(left) and \
                (isinstance(left, string) or isinstance(right, string)):
            raise infixExpectationError('+', 'numbers/strings')
        return left + right

    @infix('-', '*', '/', '**')
    def arith(self, left, token, pc):
        op = token.kind
        if not isNumber(left):
            raise infixExpectationError(op, 'number')
        right = pc.parse({'**': '**-right-associative'}.get(op))
        if not isNumber(right):
            raise infixExpectationError(op, 'number')
        return OPERATORS[op](left, right)

    @infix("[")
    def index_slice(self, left, token, pc):
        a = None
        b = None
        is_interval = False
        if pc.attempt(':'):
            a = 0
            is_interval = True
        else:
            a = pc.parse()
            if pc.attempt(':'):
                is_interval = True

        if is_interval and not pc.attempt(']'):
            b = pc.parse()
            pc.require(']')

        if not is_interval:
            pc.require(']')

        return accessProperty(left, a, b, is_interval)

    @infix(".")
    def property_dot(self, left, token, pc):
        if not isinstance(left, dict):
            raise infixExpectationError('.', 'object')
        k = pc.require('identifier').value
        try:
            return left[k]
        except KeyError:
            raise TemplateError(
                '{} not found in {}'.format(k, json.dumps(left)))

    @infix("(")
    def function_call(self, left, token, pc):
        if not callable(left):
            raise TemplateError('function call', 'callable')
        args = parseList(pc, ',', ')')
        return left(*args)

    @infix('==', '!=', '||', '&&')
    def equality_and_logic(self, left, token, pc):
        op = token.kind
        right = pc.parse(op)
        return OPERATORS[op](left, right)

    @infix('<=', '<', '>', '>=')
    def inequality(self, left, token, pc):
        op = token.kind
        right = pc.parse(op)
        if type(left) != type(right) or \
                not (isinstance(left, (int, float, string)) and not isinstance(left, bool)):
            raise infixExpectationError(op, 'numbers/strings')
        return OPERATORS[op](left, right)

    @infix("in")
    def contains(self, left, token, pc):
        right = pc.parse(token.kind)
        if isinstance(right, dict):
            if not isinstance(left, string):
                raise infixExpectationError('in-object', 'string on left side')
        elif isinstance(right, string):
            if not isinstance(left, string):
                raise infixExpectationError('in-string', 'string on left side')
        elif not isinstance(right, list):
            raise infixExpectationError(
                'in', 'Array, string, or object on right side')
        try:
            return left in right
        except TypeError:
            raise infixExpectationError('in', 'scalar value, collection')


def isNumber(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def parseString(v):
    return v[1:-1]


def parseList(pc, separator, terminator):
    rv = []
    if not pc.attempt(terminator):
        while True:
            rv.append(pc.parse())
            if not pc.attempt(separator):
                break
        pc.require(terminator)
    return rv


def parseObject(pc):
    rv = {}
    if not pc.attempt('}'):
        while True:
            k = pc.require('identifier', 'string')
            if k.kind == 'string':
                k = parseString(k.value)
            else:
                k = k.value
            pc.require(':')
            v = pc.parse()
            rv[k] = v
            if not pc.attempt(','):
                break
        pc.require('}')
    return rv


def accessProperty(value, a, b, is_interval):
    if isinstance(value, (list, string)):
        if is_interval:
            if b is None:
                b = len(value)
            try:
                return value[a:b]
            except TypeError:
                raise infixExpectationError('[..]', 'integer')
        else:
            try:
                return value[a]
            except IndexError:
                raise TemplateError('index out of bounds')
            except TypeError:
                raise infixExpectationError('[..]', 'integer')

    if not isinstance(value, dict):
        raise infixExpectationError('[..]', 'object, array, or string')
    if not isinstance(a, string):
        raise infixExpectationError('[..]', 'string index')

    try:
        return value[a]
    except KeyError:
        return None
