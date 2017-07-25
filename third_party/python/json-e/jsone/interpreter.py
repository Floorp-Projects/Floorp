from __future__ import absolute_import, print_function, unicode_literals

from .prattparser import PrattParser, infix, prefix
from .shared import JSONTemplateError, string
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


class ExpressionError(JSONTemplateError):

    @classmethod
    def expectation(cls, operator, expected):
        return cls('{} expected {}'.format(operator, expected))


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
        ['in'],
        ['||'],
        ['&&'],
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
            raise ExpressionError('expression to be evaluated must be a string')
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
            raise ExpressionError.expectation('unary -', 'number')
        return -v

    @prefix("+")
    def uplus(self, token, pc):
        v = pc.parse('unary')
        if not isNumber(v):
            raise ExpressionError.expectation('unary +', 'number')
        return v

    @prefix("identifier")
    def identifier(self, token, pc):
        try:
            return self.context[token.value]
        except KeyError:
            raise ExpressionError('no context value named "{}"'.format(token.value))

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
            raise ExpressionError.expectation('+', 'number or string')
        right = pc.parse(token.kind)
        if not isinstance(right, (string, int, float)) or isinstance(right, bool):
            raise ExpressionError.expectation('+', 'number or string')
        if type(right) != type(left) and \
                (isinstance(left, string) or isinstance(right, string)):
            raise ExpressionError.expectation('+', 'matching types')
        return left + right

    @infix('-', '*', '/', '**')
    def arith(self, left, token, pc):
        op = token.kind
        if not isNumber(left):
            raise ExpressionError.expectation(op, 'number')
        right = pc.parse({'**': '**-right-associative'}.get(op))
        if not isNumber(right):
            raise ExpressionError.expectation(op, 'number')
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
            raise ExpressionError.expectation('.', 'object')
        k = pc.require('identifier').value
        try:
            return left[k]
        except KeyError:
            raise ExpressionError('{} not found in {}'.format(k, json.dumps(left)))

    @infix("(")
    def function_call(self, left, token, pc):
        if not callable(left):
            raise ExpressionError('function call', 'callable')
        args = parseList(pc, ',', ')')
        return left(args)

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
            raise ExpressionError.expectation(op, 'matching types')
        return OPERATORS[op](left, right)

    @infix("in")
    def contains(self, left, token, pc):
        right = pc.parse(token.kind)
        if isinstance(right, dict):
            if not isinstance(left, string):
                raise ExpressionError.expectation('in-object', 'string on left side')
        elif isinstance(right, string):
            if not isinstance(left, string):
                raise ExpressionError.expectation('in-string', 'string on left side')
        elif not isinstance(right, list):
            raise ExpressionError.expectation('in', 'Array, string, or object on right side')
        try:
            return left in right
        except TypeError:
            raise ExpressionError.expectation('in', 'scalar value, collection')


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
                raise ExpressionError.expectation('[..]', 'integer')
        else:
            try:
                return value[a]
            except IndexError:
                raise ExpressionError('index out of bounds')
            except TypeError:
                raise ExpressionError.expectation('[..]', 'integer')

    if not isinstance(value, dict):
        raise ExpressionError.expectation('[..]', 'object, array, or string')
    if not isinstance(a, string):
        raise ExpressionError.expectation('[..]', 'string index')

    try:
        return value[a]
    except KeyError:
        return None
        #raise ExpressionError('{} not found in {}'.format(a, json.dumps(value)))
