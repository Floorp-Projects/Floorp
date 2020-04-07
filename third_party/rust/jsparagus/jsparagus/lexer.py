""" Lexical analysis is the breaking of a string into tokens. """

import re
import linecache
from builtins import SyntaxError as BaseSyntaxError


class SyntaxError(BaseSyntaxError):
    pass


class UnexpectedEndError(SyntaxError):
    pass


class LexicalGrammar:
    """Quick and dirty lexer implementation.

    In order to support multi-part lexing (multiple calls to .write()),
    both 1. the `ignore` regular expression; and 2. the union of the family of
    regular expressions given by `tokens` and `regexp`; must have have the
    following property: if they match a string s, they also match every prefix
    of that string.

    This requirement is not enforced by assertions; if it's not met, the
    tokenizer will just have bugs when sent multiple chunks of data.
    """
    def __init__(self, tokens, ignore=r'[ \t]*', **regexps):
        def token_to_re(token):
            s = re.escape(token)
            if s.isalpha():
                s += r'\b'
            return s

        token_list = sorted(tokens.split(), key=len, reverse=True)
        self.ignore_re = re.compile(ignore)
        self.token_re = re.compile("|".join(token_to_re(token) for token in token_list))
        self.parser_pairs = [(k, re.compile(v)) for k, v in regexps.items()]

    def __call__(self, parser, filename=None):
        return Tokenizer(self, parser, filename)


class FlatStringLexer:
    def __init__(self, parser, filename=None):
        self.parser = parser
        self.src = ''
        self.previous_token_end = 0
        self.current_token_start = 0
        self.start_lineno = 1
        self.start_column = 0
        self.point = 0
        self.filename = filename
        self.closed = False

    def write(self, text):
        assert not self.closed
        self.src += text
        self._drain()

    def close(self):
        assert not self.closed
        self.closed = True
        self._drain()
        assert self.src == ''
        return self.parser.close(self)

    def _drain(self):
        assert self.previous_token_end == 0
        assert self.current_token_start == 0
        assert self.point == 0
        closing = self.closed

        terminal_id = self._match(closing)
        while terminal_id is not None:
            self.parser.write_terminal(self, terminal_id)
            terminal_id = self._match(closing)

        # Update position info.
        discarded_text = self.src[:self.point]
        newline_count = self.src[:self.point].count('\n')
        self.start_lineno += newline_count
        if newline_count > 0:
            self.start_column = self.point - discarded_text.rindex('\n')
        else:
            self.start_column += self.point

        # Drop the parsed text and reset counters. Note that setting
        # self.previous_token_end to 0 really is correct. Setting
        # self.current_token_start to 0 is as good as anything else, because
        # there is no current token.
        self.src = self.src[self.point:]
        self.point = 0
        self.previous_token_end = 0
        self.current_token_start = 0

    def current_token_position(self):
        src_pre = self.src[:self.current_token_start]
        lineno = self.start_lineno + src_pre.count("\n")
        if '\n' in src_pre:
            line_start_index = src_pre.rfind("\n") + 1
            column = self.current_token_start - line_start_index  # can be zero
        else:
            column = self.start_column + self.current_token_start
        return lineno, column

    def current_line(self):
        # OK, this is gruesome, but we return the current line if we have the
        # whole thing and otherwise we ... try loading it from disk.
        if '\n' in self.src[:self.current_token_start]:
            line_start = self.src.rindex('\n', 0, self.current_token_start) + 1
        elif self.start_column == 0:
            line_start = 0
        else:
            line_start = -1

        if line_start != -1:
            line_end = self.src.find('\n', line_start)
            if line_end == -1:
                if self.closed:
                    return self.src[line_start:] + '\n'
            else:
                return self.src[line_start:line_end] + '\n'

        # Fallback case. Python's linecache.getline() deliberately silences all
        # errors.
        lineno = self.current_token_position()[0]
        return linecache.getline(self.filename, lineno)

    def throw(self, msg_or_exception):
        lineno, column = self.current_token_position()
        if isinstance(msg_or_exception, Exception):
            e = msg_or_exception
            e.filename = self.filename
            e.lineno = lineno
            e.offset = column + 1
        else:
            # Apparently this is the secret handshake to create a Python
            # SyntaxError and get a good error message when Python prints it.
            line = self.current_line()
            args = (self.filename, lineno, column + 1, line)
            e = SyntaxError(msg_or_exception, args)
        raise e

    def throw_unexpected_end(self):
        self.throw(UnexpectedEndError("unexpected end of input"))


class Tokenizer(FlatStringLexer):
    def __init__(self, lexical_grammar, parser, filename=None):
        super().__init__(parser, filename)
        self.ignore_re = lexical_grammar.ignore_re
        self.token_re = lexical_grammar.token_re
        self.parser_pairs = lexical_grammar.parser_pairs
        self.src = ''
        self.filename = filename
        self.last_point = 0
        self.point = 0
        self._current_match = None

    def take(self):
        return self._current_match.group()

    def saw_line_terminator(self):
        """True if there's a LineTerminator before the current token."""
        i = self.previous_token_end
        j = self.current_token_start
        ws_between = self.src[i:j]
        return any(c in ws_between for c in '\r\n\u2028\u2029')

    def _match(self, closing):
        # Advance over text matching ignore_re.
        ignore_match = self.ignore_re.match(self.src, self.point)
        if ignore_match is None:
            raise ValueError("ignore_re should always match")
        point = ignore_match.end()
        if point == len(self.src):
            if closing:
                self.point = point
            self._current_match = None
            return None

        # Try the token_re.
        token_match = self.token_re.match(self.src, point)

        # Try all the parser_pairs.
        for name, pattern in self.parser_pairs:
            match = pattern.match(self.src, point)
            if match is not None:
                break
        else:
            name = match = None

        if match is not None and token_match is not None and match.end() > token_match.end():
            pass
        elif token_match is not None:
            name, match = token_match.group(0), token_match
        elif match is not None:
            pass
        else:
            self.throw("unexpected characters {!r}"
                       .format(self.src[point:point + 12]))

        # But how do we know subsequent .write() calls won't provide more text,
        # extending this token? Here we take advantage of the odd requirement
        # LexicalGrammar imposes on its users. Every prefix of a match is a
        # match. So if this hypothetical "extended" token would match, then the
        # entire remainder of self.src is a match.
        if not closing and match.end() == len(self.src):
            # This token might be extensible. Refuse to match.
            self._current_match = None
            return None

        # This token definitely is not extensible.
        self.previous_token_end = self.point
        self.current_token_start = match.start()
        self.point = match.end()
        self._current_match = match
        return name
