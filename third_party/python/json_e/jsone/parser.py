from .AST import Primitive, UnaryOp, ContextValue, BinOp, FunctionCall, ValueAccess, Object, List
from collections import namedtuple
import re
from .shared import TemplateError

Token = namedtuple('Token', ['kind', 'value', 'start', 'end'])

expectedTokens = ["!", "(", "+", "-", "[", "false", "identifier", "null", "number", "string", "true", "{"]


class SyntaxError(TemplateError):

    @classmethod
    def unexpected(cls, got, exp):
        exp = ', '.join(sorted(exp))
        return cls('Found: {} token, expected one of: {}'.format(got.value, exp))


class Parser(object):
    def __init__(self, source, tokenizer):
        self.tokens = tokenizer.generate_tokens(source)
        self.source = source
        self.current_token = next(self.tokens)
        self.unaryOpTokens = ["-", "+", "!"]
        self.primitivesTokens = ["number", "null", "true", "false", "string"]
        self.operatorsByPriority = [["||"], ["&&"], ["in"], ["==", "!="], ["<", ">", "<=", ">="], ["+", "-"],
                                    ["*", "/"], ["**"]]

    def take_token(self, *kinds):
        if not self.current_token:
            raise SyntaxError('Unexpected end of input')
        if kinds and self.current_token.kind not in kinds:
            raise SyntaxError.unexpected(self.current_token, kinds)
        try:
            self.current_token = next(self.tokens)
        except StopIteration:
            self.current_token = None
        except SyntaxError as exc:
            raise exc

    def parse(self, level=0):
        """  expr : logicalAnd (OR logicalAnd)* """
        """  logicalAnd : inStatement (AND inStatement)* """
        """  inStatement : equality (IN equality)*  """
        """  equality : comparison (EQUALITY | INEQUALITY  comparison)* """
        """  comparison : addition (LESS | GREATER | LESSEQUAL | GREATEREQUAL addition)* """
        """  addition : multiplication (PLUS | MINUS multiplication)* """
        """  multiplication : exponentiation (MUL | DIV exponentiation)* """
        """  exponentiation : propertyAccessOrFunc (EXP exponentiation)* """
        if level == len(self.operatorsByPriority) - 1:
            node = self.parse_property_access_or_func()
            token = self.current_token

            while token is not None and token.kind in self.operatorsByPriority[level]:
                self.take_token(token.kind)
                node = BinOp(token, self.parse(level), node)
                token = self.current_token
        else:
            node = self.parse(level + 1)
            token = self.current_token

            while token is not None and token.kind in self.operatorsByPriority[level]:
                self.take_token(token.kind)
                node = BinOp(token, node, self.parse(level + 1))
                token = self.current_token

        return node

    def parse_property_access_or_func(self):
        """  propertyAccessOrFunc : unit (accessWithBrackets | DOT id | functionCall)* """
        node = self.parse_unit()
        token = self.current_token
        operators = ["[", "(", "."]
        while token is not None and token.kind in operators:
            if token.kind == "[":
                node = self.parse_access_with_brackets(node)
            elif token.kind == ".":
                token = self.current_token
                self.take_token(".")
                right_part = Primitive(self.current_token)
                self.take_token("identifier")
                node = BinOp(token, node, right_part)
            elif token.kind == "(":
                node = self.parse_function_call(node)
            token = self.current_token
        return node

    def parse_unit(self):
        # unit : unaryOp unit | primitives | contextValue | LPAREN expr RPAREN | list | object
        token = self.current_token
        if self.current_token is None:
            raise SyntaxError('Unexpected end of input')
        node = None

        if token.kind in self.unaryOpTokens:
            self.take_token(token.kind)
            node = UnaryOp(token, self.parse_unit())
        elif token.kind in self.primitivesTokens:
            self.take_token(token.kind)
            node = Primitive(token)
        elif token.kind == "identifier":
            self.take_token(token.kind)
            node = ContextValue(token)
        elif token.kind == "(":
            self.take_token("(")
            node = self.parse()
            if node is None:
                raise SyntaxError.unexpected(self.current_token, expectedTokens)
            self.take_token(")")
        elif token.kind == "[":
            node = self.parse_list()
        elif token.kind == "{":
            node = self.parse_object()

        return node

    def parse_function_call(self, name):
        """functionCall: LPAREN (expr ( COMMA expr)*)? RPAREN"""
        args = []
        token = self.current_token
        self.take_token("(")

        if self.current_token.kind != ")":
            node = self.parse()
            args.append(node)

            while self.current_token is not None and self.current_token.kind == ",":
                if args[-1] is None:
                    raise SyntaxError.unexpected(self.current_token, expectedTokens)
                self.take_token(",")
                node = self.parse()
                args.append(node)

        self.take_token(")")
        node = FunctionCall(token, name, args)

        return node

    def parse_list(self):
        """  list: LSQAREBRAKET (expr (COMMA expr)*)? RSQAREBRAKET """
        arr = []
        token = self.current_token
        self.take_token("[")

        if self.current_token != "]":
            node = self.parse()
            arr.append(node)

            while self.current_token and self.current_token.kind == ",":
                if arr[-1] is None:
                    raise SyntaxError.unexpected(self.current_token, expectedTokens)
                self.take_token(",")
                node = self.parse()
                arr.append(node)

        self.take_token("]")
        node = List(token, arr)

        return node

    def parse_access_with_brackets(self, node):
        """  valueAccess : LSQAREBRAKET expr |(expr? COLON expr?)  RSQAREBRAKET)"""
        left = None
        right = None
        is_interval = False
        token = self.current_token
        self.take_token("[")
        if self.current_token.kind == "]":
            raise SyntaxError.unexpected(self.current_token, expectedTokens)
        if self.current_token.kind != ":":
            left = self.parse()
        if self.current_token.kind == ":":
            is_interval = True
            self.take_token(":")
        if self.current_token.kind != "]":
            right = self.parse()

        if is_interval and right is None and self.current_token.kind != "]":
            raise SyntaxError.unexpected(self.current_token, expectedTokens)

        self.take_token("]")
        node = ValueAccess(token, node, is_interval, left, right)

        return node

    def parse_object(self):
        # """   object : LCURLYBRACE ( STR | ID COLON expr (COMMA STR | ID COLON expr)*)?
        # RCURLYBRACE """
        obj = {}
        objToken = self.current_token
        self.take_token("{")
        token = self.current_token

        while token is not None and (token.kind == "string" or token.kind == "identifier"):
            key = token.value
            if token.kind == "string":
                key = parse_string(key)
            self.take_token(token.kind)
            self.take_token(":")
            value = self.parse()
            if value is None:
                raise SyntaxError.unexpected(self.current_token, expectedTokens)
            obj[key] = value
            if self.current_token and self.current_token.kind == "}":
                break
            else:
                self.take_token(",")
            token = self.current_token

        self.take_token("}")
        node = Object(objToken, obj)

        return node


def parse_string(string):
    return string[1:-1]


class Tokenizer(object):
    def __init__(self, ignore, patterns, tokens):
        self.ignore = ignore
        self.patterns = patterns
        self.tokens = tokens
        # build a regular expression to generate a sequence of tokens
        token_patterns = [
            '({})'.format(self.patterns.get(t, re.escape(t)))
            for t in self.tokens]
        if self.ignore:
            token_patterns.append('(?:{})'.format(self.ignore))
        self.token_re = re.compile('^(?:' + '|'.join(token_patterns) + ')')

    def generate_tokens(self, source):
        offset = 0
        while True:
            start = offset
            remainder = source[offset:]
            mo = self.token_re.match(remainder)
            if not mo:
                if remainder:
                    raise SyntaxError(
                        "Unexpected input for '{}' at '{}'".format(source, remainder))
                break
            offset += mo.end()

            # figure out which token matched (note that idx is 0-based)
            indexes = [idx for idx, grp in enumerate(mo.groups()) if grp is not None]
            if indexes:
                idx = indexes[0]
                yield Token(
                    kind=self.tokens[idx],
                    value=mo.group(idx + 1),  # (mo.group is 1-based)
                    start=start,
                    end=offset)
