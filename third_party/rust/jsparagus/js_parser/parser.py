#!/usr/bin/env python

"""parser.py - A JavaScript parser, currently with many bugs.

See README.md for instructions.
"""

import jsparagus
from . import parser_tables
from .lexer import JSLexer


Script_entry_state = 0  # ew, magic number, get pgen to emit this


class JSParser(jsparagus.runtime.Parser):
    def __init__(self):
        jsparagus.runtime.Parser.__init__(
            self,
            parser_tables.actions,
            parser_tables.error_codes,
            Script_entry_state,
            parser_tables.DefaultMethods()
        )

    def clone(self):
        return JSParser()

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
    lexer = JSLexer(JSParser())
    lexer.write(text)
    return lexer.close()
