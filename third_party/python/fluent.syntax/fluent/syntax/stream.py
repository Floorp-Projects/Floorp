from __future__ import unicode_literals
from .errors import ParseError


class ParserStream(object):
    def __init__(self, string):
        self.string = string
        self.index = 0
        self.peek_offset = 0

    def get(self, offset):
        try:
            return self.string[offset]
        except IndexError:
            return None

    def char_at(self, offset):
        # When the cursor is at CRLF, return LF but don't move the cursor. The
        # cursor still points to the EOL position, which in this case is the
        # beginning of the compound CRLF sequence. This ensures slices of
        # [inclusive, exclusive) continue to work properly.
        if self.get(offset) == '\r' \
                and self.get(offset + 1) == '\n':
            return '\n'

        return self.get(offset)

    @property
    def current_char(self):
        return self.char_at(self.index)

    @property
    def current_peek(self):
        return self.char_at(self.index + self.peek_offset)

    def next(self):
        self.peek_offset = 0
        # Skip over CRLF as if it was a single character.
        if self.get(self.index) == '\r' \
                and self.get(self.index + 1) == '\n':
            self.index += 1
        self.index += 1
        return self.get(self.index)

    def peek(self):
        # Skip over CRLF as if it was a single character.
        if self.get(self.index + self.peek_offset) == '\r' \
                and self.get(self.index + self.peek_offset + 1) == '\n':
            self.peek_offset += 1
        self.peek_offset += 1
        return self.get(self.index + self.peek_offset)

    def reset_peek(self, offset=0):
        self.peek_offset = offset

    def skip_to_peek(self):
        self.index += self.peek_offset
        self.peek_offset = 0


EOL = '\n'
EOF = None
SPECIAL_LINE_START_CHARS = ('}', '.', '[', '*')


class FluentParserStream(ParserStream):

    def peek_blank_inline(self):
        start = self.index + self.peek_offset
        while self.current_peek == ' ':
            self.peek()
        return self.string[start:self.index + self.peek_offset]

    def skip_blank_inline(self):
        blank = self.peek_blank_inline()
        self.skip_to_peek()
        return blank

    def peek_blank_block(self):
        blank = ""
        while True:
            line_start = self.peek_offset
            self.peek_blank_inline()

            if self.current_peek == EOL:
                blank += EOL
                self.peek()
                continue

            if self.current_peek is EOF:
                # Treat the blank line at EOF as a blank block.
                return blank

            # Any other char; reset to column 1 on this line.
            self.reset_peek(line_start)
            return blank

    def skip_blank_block(self):
        blank = self.peek_blank_block()
        self.skip_to_peek()
        return blank

    def peek_blank(self):
        while self.current_peek in (" ", EOL):
            self.peek()

    def skip_blank(self):
        self.peek_blank()
        self.skip_to_peek()

    def expect_char(self, ch):
        if self.current_char == ch:
            self.next()
            return True

        raise ParseError('E0003', ch)

    def expect_line_end(self):
        if self.current_char is EOF:
            # EOF is a valid line end in Fluent.
            return True

        if self.current_char == EOL:
            self.next()
            return True

        # Unicode Character 'SYMBOL FOR NEWLINE' (U+2424)
        raise ParseError('E0003', '\u2424')

    def take_char(self, f):
        ch = self.current_char
        if ch is EOF:
            return EOF
        if f(ch):
            self.next()
            return ch
        return False

    def is_char_id_start(self, ch):
        if ch is EOF:
            return False

        cc = ord(ch)
        return (cc >= 97 and cc <= 122) or \
               (cc >= 65 and cc <= 90)

    def is_identifier_start(self):
        return self.is_char_id_start(self.current_peek)

    def is_number_start(self):
        ch = self.peek() if self.current_char == '-' else self.current_char
        if ch is EOF:
            self.reset_peek()
            return False

        cc = ord(ch)
        is_digit = cc >= 48 and cc <= 57
        self.reset_peek()
        return is_digit

    def is_char_pattern_continuation(self, ch):
        if ch is EOF:
            return False

        return ch not in SPECIAL_LINE_START_CHARS

    def is_value_start(self):
        # Inline Patterns may start with any char.
        return self.current_peek is not EOF and self.current_peek != EOL

    def is_value_continuation(self):
        column1 = self.peek_offset
        self.peek_blank_inline()

        if self.current_peek == '{':
            self.reset_peek(column1)
            return True

        if self.peek_offset - column1 == 0:
            return False

        if self.is_char_pattern_continuation(self.current_peek):
            self.reset_peek(column1)
            return True

        return False

    # -1 - any
    #  0 - comment
    #  1 - group comment
    #  2 - resource comment
    def is_next_line_comment(self, level=-1):
        if self.current_peek != EOL:
            return False

        i = 0

        while (i <= level or (level == -1 and i < 3)):
            if self.peek() != '#':
                if i <= level and level != -1:
                    self.reset_peek()
                    return False
                break
            i += 1

        # The first char after #, ## or ###.
        if self.peek() in (' ', EOL):
            self.reset_peek()
            return True

        self.reset_peek()
        return False

    def is_variant_start(self):
        current_peek_offset = self.peek_offset
        if self.current_peek == '*':
            self.peek()
        if self.current_peek == '[' and self.peek() != '[':
            self.reset_peek(current_peek_offset)
            return True

        self.reset_peek(current_peek_offset)
        return False

    def is_attribute_start(self):
        return self.current_peek == '.'

    def skip_to_next_entry_start(self, junk_start):
        last_newline = self.string.rfind(EOL, 0, self.index)
        if junk_start < last_newline:
            # Last seen newline is _after_ the junk start. It's safe to rewind
            # without the risk of resuming at the same broken entry.
            self.index = last_newline

        while self.current_char:
            # We're only interested in beginnings of line.
            if self.current_char != EOL:
                self.next()
                continue

            # Break if the first char in this line looks like an entry start.
            first = self.next()
            if self.is_char_id_start(first) or first == '-' or first == '#':
                break

            # Syntax 0.4 compatibility
            peek = self.peek()
            self.reset_peek()
            if (first, peek) == ('/', '/') or (first, peek) == ('[', '['):
                break

    def take_id_start(self):
        if self.is_char_id_start(self.current_char):
            ret = self.current_char
            self.next()
            return ret

        raise ParseError('E0004', 'a-zA-Z')

    def take_id_char(self):
        def closure(ch):
            cc = ord(ch)
            return ((cc >= 97 and cc <= 122) or
                    (cc >= 65 and cc <= 90) or
                    (cc >= 48 and cc <= 57) or
                    cc == 95 or cc == 45)
        return self.take_char(closure)

    def take_digit(self):
        def closure(ch):
            cc = ord(ch)
            return (cc >= 48 and cc <= 57)
        return self.take_char(closure)

    def take_hex_digit(self):
        def closure(ch):
            cc = ord(ch)
            return (
                (cc >= 48 and cc <= 57)   # 0-9
                or (cc >= 65 and cc <= 70)  # A-F
                or (cc >= 97 and cc <= 102))  # a-f
        return self.take_char(closure)
