#!/usr/bin/env python

"""parser.py - A JavaScript parser, currently with many bugs.

See README.md for instructions.
"""

import jsparagus
from . import parser_tables
from .lexer import JSLexer


# "type: ignore" because mypy can't see inside js_parser.parser_tables.
class JSParser(parser_tables.Parser):  # type: ignore
    def __init__(self, goal='Script', builder=None):
        super().__init__(goal, builder)
        self._goal = goal

    def clone(self):
        return JSParser(self._goal, self.methods)

    def on_recover(self, error_code, lexer, stv):
        """Check that ASI error recovery is really acceptable."""
        if error_code == 'asi':
            # ASI is allowed in three places:
            # - at the end of the source text
            # - before a close brace `}`
            # - after a LineTerminator
            # Hence the three-part if-condition below.
            #
            # The other quirks of ASI are implemented by massaging the syntax,
            # in parse_esgrammar.py.
            if not self.closed and stv.term != '}' and not lexer.saw_line_terminator():
                lexer.throw("missing semicolon")
        else:
            # ASI is always allowed in this one state.
            assert error_code == 'do_while_asi'


def parse_Script(text):
    lexer = JSLexer(JSParser('Script'))
    lexer.write(text)
    return lexer.close()
