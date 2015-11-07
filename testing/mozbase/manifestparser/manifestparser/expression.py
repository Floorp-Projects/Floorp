# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import sys
import traceback

__all__ = ['parse', 'ParseError', 'ExpressionParser']

# expr.py
# from:
# http://k0s.org/mozilla/hg/expressionparser
# http://hg.mozilla.org/users/tmielczarek_mozilla.com/expressionparser

# Implements a top-down parser/evaluator for simple boolean expressions.
# ideas taken from http://effbot.org/zone/simple-top-down-parsing.htm
#
# Rough grammar:
# expr := literal
#       | '(' expr ')'
#       | expr '&&' expr
#       | expr '||' expr
#       | expr '==' expr
#       | expr '!=' expr
#       | expr '<' expr
#       | expr '>' expr
#       | expr '<=' expr
#       | expr '>=' expr
# literal := BOOL
#          | INT
#          | STRING
#          | IDENT
# BOOL   := true|false
# INT    := [0-9]+
# STRING := "[^"]*"
# IDENT  := [A-Za-z_]\w*

# Identifiers take their values from a mapping dictionary passed as the second
# argument.

# Glossary (see above URL for details):
# - nud: null denotation
# - led: left detonation
# - lbp: left binding power
# - rbp: right binding power

class ident_token(object):
    def __init__(self, scanner, value):
        self.value = value
    def nud(self, parser):
        # identifiers take their value from the value mappings passed
        # to the parser
        return parser.value(self.value)

class literal_token(object):
    def __init__(self, scanner, value):
        self.value = value
    def nud(self, parser):
        return self.value

class eq_op_token(object):
    "=="
    def led(self, parser, left):
        return left == parser.expression(self.lbp)

class neq_op_token(object):
    "!="
    def led(self, parser, left):
        return left != parser.expression(self.lbp)

class lt_op_token(object):
    "<"
    def led(self, parser, left):
        return left < parser.expression(self.lbp)

class gt_op_token(object):
    ">"
    def led(self, parser, left):
        return left > parser.expression(self.lbp)

class le_op_token(object):
    "<="
    def led(self, parser, left):
        return left <= parser.expression(self.lbp)

class ge_op_token(object):
    ">="
    def led(self, parser, left):
        return left >= parser.expression(self.lbp)

class not_op_token(object):
    "!"
    def nud(self, parser):
        return not parser.expression(100)

class and_op_token(object):
    "&&"
    def led(self, parser, left):
        right = parser.expression(self.lbp)
        return left and right

class or_op_token(object):
    "||"
    def led(self, parser, left):
        right = parser.expression(self.lbp)
        return left or right

class lparen_token(object):
    "("
    def nud(self, parser):
        expr = parser.expression()
        parser.advance(rparen_token)
        return expr

class rparen_token(object):
    ")"

class end_token(object):
    """always ends parsing"""

### derived literal tokens

class bool_token(literal_token):
    def __init__(self, scanner, value):
        value = {'true':True, 'false':False}[value]
        literal_token.__init__(self, scanner, value)

class int_token(literal_token):
    def __init__(self, scanner, value):
        literal_token.__init__(self, scanner, int(value))

class string_token(literal_token):
    def __init__(self, scanner, value):
        literal_token.__init__(self, scanner, value[1:-1])

precedence = [(end_token, rparen_token),
              (or_op_token,),
              (and_op_token,),
              (lt_op_token, gt_op_token, le_op_token, ge_op_token,
               eq_op_token, neq_op_token),
              (lparen_token,),
              ]
for index, rank in enumerate(precedence):
    for token in rank:
        token.lbp = index # lbp = lowest left binding power

class ParseError(Exception):
    """error parsing conditional expression"""

class ExpressionParser(object):
    """
    A parser for a simple expression language.

    The expression language can be described as follows::

        EXPRESSION ::= LITERAL | '(' EXPRESSION ')' | '!' EXPRESSION | EXPRESSION OP EXPRESSION
        OP ::= '==' | '!=' | '<' | '>' | '<=' | '>=' | '&&' | '||'
        LITERAL ::= BOOL | INT | IDENT | STRING
        BOOL ::= 'true' | 'false'
        INT ::= [0-9]+
        IDENT ::= [a-zA-Z_]\w*
        STRING ::= '"' [^\"] '"' | ''' [^\'] '''

    At its core, expressions consist of booleans, integers, identifiers and.
    strings. Booleans are one of *true* or *false*. Integers are a series
    of digits. Identifiers are a series of English letters and underscores.
    Strings are a pair of matching quote characters (single or double) with
    zero or more characters inside.

    Expressions can be combined with operators: the equals (==) and not
    equals (!=) operators compare two expressions and produce a boolean. The
    and (&&) and or (||) operators take two expressions and produce the logical
    AND or OR value of them, respectively. An expression can also be prefixed
    with the not (!) operator, which produces its logical negation.

    Finally, any expression may be contained within parentheses for grouping.

    Identifiers take their values from the mapping provided.
    """

    scanner = None

    def __init__(self, text, valuemapping, strict=False):
        """
        Initialize the parser
        :param text: The expression to parse as a string.
        :param valuemapping: A dict mapping identifier names to values.
        :param strict: If true, referencing an identifier that was not
                       provided in :valuemapping: will raise an error.
        """
        self.text = text
        self.valuemapping = valuemapping
        self.strict = strict

    def _tokenize(self):
        """
        Lex the input text into tokens and yield them in sequence.
        """
        if not ExpressionParser.scanner:
            ExpressionParser.scanner = re.Scanner([
                # Note: keep these in sync with the class docstring above.
                (r"true|false", bool_token),
                (r"[a-zA-Z_]\w*", ident_token),
                (r"[0-9]+", int_token),
                (r'("[^"]*")|(\'[^\']*\')', string_token),
                (r"==", eq_op_token()),
                (r"!=", neq_op_token()),
                (r"<=", le_op_token()),
                (r">=", ge_op_token()),
                (r"<", lt_op_token()),
                (r">", gt_op_token()),
                (r"\|\|", or_op_token()),
                (r"!", not_op_token()),
                (r"&&", and_op_token()),
                (r"\(", lparen_token()),
                (r"\)", rparen_token()),
                (r"\s+", None), # skip whitespace
            ])
        tokens, remainder = ExpressionParser.scanner.scan(self.text)
        for t in tokens:
            yield t
        yield end_token()

    def value(self, ident):
        """
        Look up the value of |ident| in the value mapping passed in the
        constructor.
        """
        if self.strict:
            return self.valuemapping[ident]
        else:
            return self.valuemapping.get(ident, None)

    def advance(self, expected):
        """
        Assert that the next token is an instance of |expected|, and advance
        to the next token.
        """
        if not isinstance(self.token, expected):
            raise Exception, "Unexpected token!"
        self.token = self.iter.next()

    def expression(self, rbp=0):
        """
        Parse and return the value of an expression until a token with
        right binding power greater than rbp is encountered.
        """
        t = self.token
        self.token = self.iter.next()
        left = t.nud(self)
        while rbp < self.token.lbp:
            t = self.token
            self.token = self.iter.next()
            left = t.led(self, left)
        return left

    def parse(self):
        """
        Parse and return the value of the expression in the text
        passed to the constructor. Raises a ParseError if the expression
        could not be parsed.
        """
        try:
            self.iter = self._tokenize()
            self.token = self.iter.next()
            return self.expression()
        except:
            extype, ex, tb = sys.exc_info()
            formatted = ''.join(traceback.format_exception_only(extype, ex))
            raise ParseError("could not parse: %s\nexception: %svariables: %s" % (self.text, formatted, self.valuemapping)), None, tb

    __call__ = parse


def parse(text, **values):
    """
    Parse and evaluate a boolean expression.
    :param text: The expression to parse, as a string.
    :param values: A dict containing a name to value mapping for identifiers
                   referenced in *text*.
    :rtype: the final value of the expression.
    :raises: :py:exc::ParseError: will be raised if parsing fails.
    """
    return ExpressionParser(text, values).parse()
