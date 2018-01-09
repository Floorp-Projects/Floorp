from __future__ import absolute_import, print_function, unicode_literals

import re
from collections import namedtuple
from .shared import TemplateError
from .six import with_metaclass, viewitems


class SyntaxError(TemplateError):

    @classmethod
    def unexpected(cls, got, exp):
        exp = ', '.join(sorted(exp))
        return cls('Found {}, expected {}'.format(got.value, exp))


Token = namedtuple('Token', ['kind', 'value', 'start', 'end'])


def prefix(*kinds):
    """Decorate a method as handling prefix tokens of the given kinds"""
    def wrap(fn):
        try:
            fn.prefix_kinds.extend(kinds)
        except AttributeError:
            fn.prefix_kinds = list(kinds)
        return fn
    return wrap


def infix(*kinds):
    """Decorate a method as handling infix tokens of the given kinds"""
    def wrap(fn):
        try:
            fn.infix_kinds.extend(kinds)
        except AttributeError:
            fn.infix_kinds = list(kinds)
        return fn
    return wrap


class PrattParserMeta(type):

    def __init__(cls, name, bases, body):
        # set up rules based on decorated methods
        infix_rules = cls.infix_rules = {}
        prefix_rules = cls.prefix_rules = {}
        for prop, value in viewitems(body):
            if hasattr(value, 'prefix_kinds'):
                for kind in value.prefix_kinds:
                    prefix_rules[kind] = value
                delattr(cls, prop)
            if hasattr(value, 'infix_kinds'):
                for kind in value.infix_kinds:
                    infix_rules[kind] = value
                delattr(cls, prop)

        # build a regular expression to generate a sequence of tokens
        token_patterns = [
            '({})'.format(cls.patterns.get(t, re.escape(t)))
            for t in cls.tokens]
        if cls.ignore:
            token_patterns.append('(?:{})'.format(cls.ignore))
        cls.token_re = re.compile('^(?:' + '|'.join(token_patterns) + ')')

        # build a map from token kind to precedence level
        cls.precedence_map = {
            kind: prec + 1
            for (prec, row) in enumerate(cls.precedence)
            for kind in row
        }


class PrattParser(with_metaclass(PrattParserMeta, object)):

    # regular expression for ignored input (e.g., whitespace)
    ignore = None

    # regular expressions for tokens that do not match themselves
    patterns = {}

    # all token kinds (note that order matters - the first matching token
    # will be returned)
    tokens = []

    # precedence of tokens, as a list of lists, from lowest to highest
    precedence = []

    def parse(self, source):
        pc = ParseContext(self, source, self._generate_tokens(source))
        result = pc.parse()
        # if there are any tokens remaining, that's an error..
        token = pc.attempt()
        if token:
            raise SyntaxError.unexpected(token, self.infix_rules)
        return result

    def parseUntilTerminator(self, source, terminator):
        pc = ParseContext(self, source, self._generate_tokens(source))
        result = pc.parse()
        token = pc.attempt()
        if token.kind != terminator:
            raise SyntaxError.unexpected(token, [terminator])
        return (result, token.start)

    def _generate_tokens(self, source):
        offset = 0
        while True:
            start = offset
            remainder = source[offset:]
            mo = self.token_re.match(remainder)
            if not mo:
                if remainder:
                    raise SyntaxError(
                        "Unexpected input: '{}'".format(remainder))
                break
            offset += mo.end()

            # figure out which token matched (note that idx is 0-based)
            indexes = list(
                filter(lambda x: x[1] is not None, enumerate(mo.groups())))
            if indexes:
                idx = indexes[0][0]
                yield Token(
                    kind=self.tokens[idx],
                    value=mo.group(idx + 1),  # (mo.group is 1-based)
                    start=start,
                    end=offset)


class ParseContext(object):

    def __init__(self, parser, source, token_generator):
        self.parser = parser
        self.source = source

        self._tokens = token_generator
        self._error = None

        self._advance()

    def _advance(self):
        try:
            self.next_token = next(self._tokens)
        except StopIteration:
            self.next_token = None
        except SyntaxError as exc:
            self._error = exc

    def attempt(self, *kinds):
        """Try to get the next token if it matches one of the kinds given,
        otherwise returning None. If no kinds are given, any kind is
        accepted."""
        if self._error:
            raise self._error
        token = self.next_token
        if not token:
            return None
        if kinds and token.kind not in kinds:
            return None
        self._advance()
        return token

    def require(self, *kinds):
        """Get the next token, raising an exception if it doesn't match one of
        the given kinds, or the input ends. If no kinds are given, returns the
        next token of any kind."""
        token = self.attempt()
        if not token:
            raise SyntaxError('Unexpected end of input')
        if kinds and token.kind not in kinds:
            raise SyntaxError.unexpected(token, kinds)
        return token

    def parse(self, precedence=None):
        parser = self.parser
        precedence = parser.precedence_map[precedence] if precedence else 0
        token = self.require()
        prefix_rule = parser.prefix_rules.get(token.kind)
        if not prefix_rule:
            raise SyntaxError.unexpected(token, parser.prefix_rules)
        left = prefix_rule(parser, token, self)
        while self.next_token:
            kind = self.next_token.kind
            if kind not in parser.infix_rules:
                break
            if precedence >= parser.precedence_map[kind]:
                break
            token = self.require()
            left = parser.infix_rules[kind](parser, left, token, self)
        return left
