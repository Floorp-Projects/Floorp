#!/usr/bin/env python

"""parse_pgen.py - Parse grammars written in the pgen parser specification language.

I'm not sure I want to keep this pgen mini-language around; ignore this for now.
"""

import sys
from collections import namedtuple

from .lexer import LexicalGrammar
from .grammar import Grammar, Production, CallMethod, is_concrete_element, Optional
from . import gen
from . import parse_pgen_generated


pgen_lexer = LexicalGrammar(
    "goal nt var token { } ; ? = => ( ) ,",
    r'([ \t\r\n]|#.*)*',
    IDENT=r'[A-Za-z_](?:\w|[_-])*',
    STR=r'"[^\\\n"]*"',
    MATCH=r'\$(?:0|[1-9][0-9]*)',
    COMMENT=r'//.*',
)


def list_of(e, allow_comments=False):
    nt = e + 's'
    prods = [
        Production([e], CallMethod('single', (0,))),
        Production([nt, e], CallMethod('append', (0, 1))),
    ]
    if allow_comments:
        prods.append(Production(['COMMENT'], CallMethod('empty', (0,))))
    return prods


def call_method(name, body):
    arg_indexes = []
    current = 0
    for e in body:
        if is_concrete_element(e):
            if e not in discards:
                arg_indexes.append(current)
            current += 1

    return CallMethod(name, tuple(arg_indexes))


def prod(body, reducer):
    if isinstance(reducer, str):
        reducer = call_method(reducer, body)
    return Production(body, reducer)


discards = set('token var nt goal Some None = => ; ( ) { } , ?'.split())

pgen_grammar = Grammar(
    {
        'grammar': [
            [Optional('token_defs'), 'nt_defs']
        ],
        'token_defs': list_of('token_def'),
        'token_def': [
            prod(['token', 'IDENT', '=', 'STR', ';'], 'const_token'),
            prod(['var', 'token', 'IDENT', ';'], 'var_token'),
        ],
        'nt_defs': [
            prod(['nt_def'], 'nt_defs_single'),
            prod(['nt_defs', 'nt_def'], 'nt_defs_append'),
        ],
        'nt_def': [
            prod([Optional('COMMENT'), Optional('goal'), 'nt', 'IDENT', '{',
                  gen.Optional('prods'), '}'], 'nt_def'),
        ],
        'prods': list_of('prod', allow_comments=True),
        'prod': [
            prod(['terms', gen.Optional('reducer'), ';'], 'prod'),
        ],
        'terms': list_of('term'),
        'term': [
            ['symbol'],
            prod(['symbol', '?'], 'optional'),
        ],
        'symbol': [
            prod(['IDENT'], 'ident'),
            prod(['STR'], 'str'),
        ],
        'reducer': [
            prod(['=>', 'expr'], 1)
        ],
        'expr': [
            prod(['MATCH'], 'expr_match'),
            prod(['IDENT', '(', gen.Optional('expr_args'), ')'], 'expr_call'),
            prod(['Some', '(', 'expr', ')'], 'expr_some'),
            prod(['None'], 'expr_none'),
        ],
        'expr_args': [
            prod(['expr'], 'args_single'),
            prod(['expr_args', ',', 'expr'], 'args_append'),
        ],
    },
    goal_nts=['grammar'],
    variable_terminals=['IDENT', 'STR', 'MATCH', 'COMMENT']
)


Literal = namedtuple("Literal", "chars")

default_token_list = [
    ("Var", "var"),
    ("Token", "token"),
    ("Goal", "goal"),
    ("Nt", "nt"),
    ("IDENT", None),
    ("STR", None),
    ("OpenBrace", "{"),
    ("CloseBrace", "}"),
    ("OpenParenthesis", "("),
    ("CloseParenthesis", ")"),
    ("Colon", ":"),
    ("EqualSign", "="),
    ("Asterisk", "*"),
    ("PlusSign", "+"),
    ("MinusSign", "-"),
    ("Slash", "/"),
    ("Semicolon", ";"),
    ("QuestionMark", "?"),
    ("RightArrow", "->"),
    ("Comma", ","),
]


class AstBuilder:
    def grammar(self, token_defs, nt_defs):
        nonterminals, goal_nts = nt_defs
        return (token_defs or default_token_list, nonterminals, goal_nts)

    def empty(self, value):
        return []

    def single(self, value):
        return [value]

    def append(self, values, value):
        values.append(value)
        return values

    def const_token(self, name, picture):
        assert picture[0] == '"'
        assert picture[-1] == '"'
        return (name, picture[1:-1])

    def var_token(self, name):
        return (name, None)

    def comment(self, comment):
        pass

    def nt_defs_single(self, nt_def):
        return self.nt_defs_append(({}, []), nt_def)

    def nt_defs_append(self, grammar_in, nt_def):
        is_goal, nt, prods = nt_def
        grammar, goal_nts = grammar_in
        if nt in grammar:
            raise ValueError("multiple definitions for nt {}".format(nt))
        grammar[nt] = prods
        if is_goal:
            goal_nts.append(nt)
        return grammar, goal_nts

    def nt_def(self, _comment, goal_kw, ident, prods):
        is_goal = goal_kw == "goal"
        prods = [Production(body, reducer) for body, reducer in prods]
        return (is_goal, ident, prods)

    def prod(self, symbols, reducer):
        if reducer is None:
            if sum(1 for e in symbols if is_concrete_element(e)) == 1:
                reducer = 0
            else:
                raise ValueError("reducer required for {!r}".format(symbols))
        return (symbols, reducer)

    def optional(self, sym):
        return gen.Optional(sym)

    def ident(self, sym):
        return sym

    def str(self, sym):
        assert len(sym) > 1
        assert sym[0] == '"'
        assert sym[-1] == '"'
        chars = sym[1:-1]  # This is a bit sloppy.
        return Literal(chars)

    def expr_match(self, match):
        assert match.startswith('$')
        return int(match[1:])

    def expr_call(self, ident, args):
        return CallMethod(ident, tuple(args or ()))

    def args_single(self, expr):
        return [expr]

    def args_append(self, args, arg):
        args.append(arg)
        return args


def check_grammar(result):
    tokens, nonterminals, goal_nts = result
    tokens_by_name = {}
    tokens_by_image = {}
    for name, image in tokens:
        if name in tokens_by_name:
            raise ValueError("token `{}` redeclared".format(name))
        tokens_by_name[name] = image
        if image is not None and image in tokens_by_image:
            raise ValueError("multiple tokens look like \"{}\"".format(image))
        tokens_by_image[image] = name
        if name in nonterminals:
            raise ValueError("`{}` is declared as both a token and a nonterminal (pick one)".format(name))

    def check_element(nt, i, e):
        if isinstance(e, Optional):
            return Optional(check_element(nt, i, e.inner))
        elif isinstance(e, Literal):
            if e.chars not in tokens_by_image:
                raise ValueError("in {} production {}: undeclared token \"{}\"".format(nt, i, e.chars))
            return e.chars
        else:
            assert isinstance(e, str), e.__class__.__name__
            if e in nonterminals:
                return e
            elif e in tokens_by_name:
                image = tokens_by_name[e]
                if image is not None:
                    return image
                return e
            else:
                raise ValueError("in {} production {}: undeclared symbol {}".format(nt, i, e))

    out = {nt: [] for nt in nonterminals}
    for nt, rhs_list in nonterminals.items():
        for i, p in enumerate(rhs_list):
            out_rhs = [check_element(nt, i, e) for e in p.body]
            out[nt].append(p.copy_with(body=out_rhs))

    return (tokens, out, goal_nts)


def load_grammar(filename):
    with open(filename) as f:
        text = f.read()
    parser = parse_pgen_generated.Parser(builder=AstBuilder())
    lexer = pgen_lexer(parser, filename=filename)
    lexer.write(text)
    result = lexer.close()
    tokens, nonterminals, goals = check_grammar(result)
    variable_terminals = [name for name, image in tokens if image is None]
    return Grammar(nonterminals,
                   goal_nts=goals,
                   variable_terminals=variable_terminals)


def regenerate():
    import sys
    gen.generate_parser(sys.stdout, pgen_grammar)


if __name__ == '__main__':
    if sys.argv[1:] == ['--regenerate']:
        regenerate()
    else:
        print("usage: python -m jsparagus.parse_pgen --regenerate")
        sys.exit(1)
