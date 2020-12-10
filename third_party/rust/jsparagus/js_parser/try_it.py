#!/usr/bin/env python

"""js.py - Repl-like toy to explore parsing of lines of JS.

See README.md for instructions.
"""

import argparse
import traceback
from .lexer import JSLexer
from .parser import JSParser
from jsparagus.lexer import SyntaxError


def interactive_input(lexer, prompt="js> "):
    while True:
        line = input(prompt)
        lexer.write(line + "\n")
        if lexer.can_close():
            return lexer.close()
        prompt = "..> "


def rpl():
    """Read-print loop."""
    while True:
        parser = JSLexer(JSParser(), filename="<stdin>")
        try:
            result = interactive_input(parser)
        except EOFError:
            print()
            break
        except SyntaxError:
            traceback.print_exc(limit=0)
            continue
        print(result)


def main():
    parser = argparse.ArgumentParser(description="Try out the JS parser.")
    parser.add_argument('input_file', metavar='FILE', nargs='?',
                        help=".js file to parse")
    options = parser.parse_args()

    if options.input_file is not None:
        with open(options.input_file) as f:
            source = f.readlines()
        lexer = JSLexer(JSParser())
        for line in source:
            print(line.rstrip())
            lexer.write(line)
        ast = lexer.close()
        print(ast)
    else:
        rpl()


if __name__ == '__main__':
    main()
