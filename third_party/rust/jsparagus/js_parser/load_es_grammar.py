""" Functions for loading the ECMAScript lexical and syntactic grammars. """

from jsparagus.ordered import OrderedSet, OrderedFrozenSet
from jsparagus import gen, grammar
from .lexer import ECMASCRIPT_FULL_KEYWORDS, ECMASCRIPT_CONDITIONAL_KEYWORDS
from .parse_esgrammar import parse_esgrammar


ECMASCRIPT_LEXICAL_SYNTHETIC_TERMINALS: grammar.SyntheticTerminalsDict = {
    # Theoretically, this should be the set of all Unicode characters, but that
    # would take a lot of memory, and in practice, the set is not used.
    'SourceCharacter': OrderedFrozenSet([]),
}

ECMASCRIPT_LEXICAL_GOAL_NTS = [
    'WhiteSpace',
    'InputElementDiv',
    'InputElementRegExp',
]


def load_lexical_grammar(filename):
    """Load the ECMAScript lexical grammar."""
    with open(filename) as f:
        grammar_text = f.read()
    g = parse_esgrammar(
        grammar_text,
        filename=filename,
        goals=ECMASCRIPT_LEXICAL_GOAL_NTS,
        synthetic_terminals=ECMASCRIPT_LEXICAL_SYNTHETIC_TERMINALS,
        terminal_names=ECMASCRIPT_LEXICAL_SYNTHETIC_TERMINALS.keys())
    return gen.expand_parameterized_nonterminals(g)


ECMASCRIPT_SYNTACTIC_GOAL_NTS = [
    'Script',
    'Module',
    # 'FormalParameters',
    # 'FunctionBody',
]

# Identifiers are complicated. A "synthetic terminal" is a shorthand symbol
# that stands for any one of a set of terminals. For example, *IdentifierName*
# stands for any token that looks like an identifier, including keywords.
#
# These sets must use the names of the terminals produced by the lexer.  Except
# for `Name`, our lexer output uses the terminal symbols of the syntactic
# grammar, which include some nonterminals of the lexical grammar. The
# syntactic grammar uses `BooleanLiteral`, not `true` and `false`; and it uses
# `NullLiteral` instead of `null`.
ECMASCRIPT_SYNTHETIC_TERMINALS = {
    'IdentifierName': OrderedSet([
        'Name',
        'BooleanLiteral',
        'NullLiteral',
        'NameWithEscape',
        *ECMASCRIPT_FULL_KEYWORDS,
        *ECMASCRIPT_CONDITIONAL_KEYWORDS
    ]) - OrderedSet(['true', 'false', 'null']),
    'Identifier': OrderedSet([
        'Name',
        'NameWithEscape',
        *ECMASCRIPT_CONDITIONAL_KEYWORDS
    ]),
}

# Lexical nonterminals that are used as terminals in the syntactic grammar.
ECMASCRIPT_TOKEN_NAMES = [
    'BooleanLiteral',
    'IdentifierName',
    'PrivateIdentifier',
    'NoSubstitutionTemplate',
    'NullLiteral',
    'NumericLiteral',
    'BigIntLiteral',
    'RegularExpressionLiteral',
    'StringLiteral',
    'TemplateHead',
    'TemplateMiddle',
    'TemplateTail',
]

# List of all terminals, other than keywords, that our (hand-coded) lexer
# produces.
#
# (What our lexer implements for IdentifierName and friends is a slight
# variation on the spec. See `ECMASCRIPT_SYNTHETIC_TERMINALS` above.)
TERMINAL_NAMES_FOR_SYNTACTIC_GRAMMAR = ECMASCRIPT_TOKEN_NAMES + [
    'Identifier',
    'Name',
]


def load_syntactic_grammar(filename, extensions):
    """Load the ECMAScript syntactic grammar."""
    with open(filename) as f:
        grammar_text = f.read()

    extensions_content = []
    for ext_filename in extensions:
        # Extract grammar_extension! macro content, and store in a list.
        with open(ext_filename) as ext_file:
            content = None
            start_line = 0
            for lineno, line in enumerate(ext_file):
                if line.startswith("grammar_extension!"):
                    assert line.endswith("{\n")
                    content = ""
                    # +2: enumerate starts at 0, while the first line is 1.
                    # Also, the first line added to the content variable is the
                    # next one.
                    start_line = lineno + 2
                    continue
                if line.startswith("}") and content:
                    extensions_content.append((ext_filename, start_line, content))
                    content = None
                    continue
                if content is not None:
                    content += line

    g = parse_esgrammar(
        grammar_text,
        filename=filename,
        extensions=extensions_content,
        goals=ECMASCRIPT_SYNTACTIC_GOAL_NTS,
        synthetic_terminals=ECMASCRIPT_SYNTHETIC_TERMINALS,
        terminal_names=TERMINAL_NAMES_FOR_SYNTACTIC_GRAMMAR)

    return g
