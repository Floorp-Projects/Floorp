#!/usr/bin/env python3

"""gen.py - Fifth stab at a parser generator.

**Grammars.**
A grammar is a dictionary {str: [[symbol]]} mapping names of nonterminals to
lists of right-hand sides. Each right-hand side is a list of symbols. There
are several kinds of symbols; see grammar.py to learn more.

Instead of a list of right-hand sides, the value of a grammar entry may be a
function; see grammar.Nt for details.

**Token streams.**
The user passes to each method an object representing the input sequence.
This object must support two methods:

*   `src.peek()` returns the kind of the next token, or `None` at the end of
    input.

*   `src.take(kind)` throws an exception if `src.peek() != kind`;
    otherwise, it removes the next token from the input stream and returns it.
    The special case `src.take(None)` checks that the input stream is empty:
    if so, it returns None; if not, it throws.

For very basic needs, see `lexer.LexicalGrammar`.

"""

from __future__ import annotations

import io
import typing

from .grammar import Grammar
from . import emit
from .rewrites import CanonicalGrammar
from .parse_table import ParseTable


# *** Parser generation *******************************************************

def generate_parser_states(
        grammar: Grammar,
        *,
        verbose: bool = False,
        progress: bool = False,
        debug: bool = False
) -> ParseTable:
    parse_table = ParseTable(CanonicalGrammar(grammar), verbose, progress, debug)
    return parse_table


def generate_parser(
        out: io.TextIOBase,
        source: Grammar,
        *,
        verbose: bool = False,
        progress: bool = False,
        debug: bool = False,
        target: str = 'python',
        handler_info: typing.Any = None
) -> None:
    assert target in ('python', 'rust')

    if isinstance(source, Grammar):
        grammar = CanonicalGrammar(source)
        parser_data = generate_parser_states(
            source, verbose=verbose, progress=progress, debug=debug)
    elif isinstance(source, ParseTable):
        parser_data = source
        parser_data.debug_info = debug
    else:
        raise TypeError("unrecognized source: {!r}".format(source))

    if target == 'rust':
        if isinstance(parser_data, ParseTable):
            emit.write_rust_parse_table(out, parser_data, handler_info)
        else:
            raise ValueError("Unexpected parser_data kind")
    else:
        if isinstance(parser_data, ParseTable):
            emit.write_python_parse_table(out, parser_data)
        else:
            raise ValueError("Unexpected parser_data kind")


def compile(grammar, verbose=False, debug=False):
    assert isinstance(grammar, Grammar)
    out = io.StringIO()
    generate_parser(out, grammar, verbose=verbose, debug=debug)
    scope = {}
    if verbose:
        with open("parse_with_python.py", "w") as f:
            f.write(out.getvalue())
    exec(out.getvalue(), scope)
    return scope['Parser']


# *** Fun demo ****************************************************************

def demo():
    from .grammar import example_grammar
    grammar = example_grammar()

    from . import lexer
    tokenize = lexer.LexicalGrammar(
        "+ - * / ( )", NUM=r'0|[1-9][0-9]*', VAR=r'[_A-Za-z]\w+')

    import io
    out = io.StringIO()
    generate_parser(out, grammar)
    code = out.getvalue()
    print(code)
    print("----")

    sandbox = {}
    exec(code, sandbox)
    Parser = sandbox['Parser']

    while True:
        try:
            line = input('> ')
        except EOFError:
            break

        try:
            parser = Parser()
            lexer = tokenize(parser)
            lexer.write(line)
            result = lexer.close()
        except Exception as exc:
            print(exc.__class__.__name__ + ": " + str(exc))
        else:
            print(result)


if __name__ == '__main__':
    demo()
