#!/usr/bin/env python3

"""jsparagus/main.py - Generate a parser from a pgen grammar.

(This is for testing. pgen will likely go away. Ignore this for now.)
"""

import sys
import argparse
from . import parse_pgen
from . import gen


def main():
    parser = argparse.ArgumentParser(description="Generate a parser.")
    parser.add_argument('--target', choices=['python', 'rust'], default='rust',
                        help="target language to use when printing the parser tables")
    parser.add_argument('grammar', metavar='FILE', nargs=1,
                        help=".pgen file containing the grammar")
    options = parser.parse_args()

    [pgen_filename] = options.grammar
    grammar = parse_pgen.load_grammar(pgen_filename)
    gen.generate_parser(sys.stdout, grammar, target=options.target)


if __name__ == '__main__':
    main()
