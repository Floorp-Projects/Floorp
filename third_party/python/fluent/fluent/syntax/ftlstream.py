from __future__ import unicode_literals
from .stream import ParserStream
from .errors import ParseError


INLINE_WS = (' ', '\t')
SPECIAL_LINE_START_CHARS = ('}', '.', '[', '*')


class FTLParserStream(ParserStream):
    last_comment_zero_four_syntax = False

    def skip_inline_ws(self):
        while self.ch:
            if self.ch not in INLINE_WS:
                break
            self.next()

    def peek_inline_ws(self):
        ch = self.current_peek()
        while ch:
            if ch not in INLINE_WS:
                break
            ch = self.peek()

    def skip_blank_lines(self):
        while True:
            self.peek_inline_ws()

            if self.current_peek_is('\n'):
                self.skip_to_peek()
                self.next()
            else:
                self.reset_peek()
                break

    def peek_blank_lines(self):
        while True:
            line_start = self.get_peek_index()

            self.peek_inline_ws()

            if self.current_peek_is('\n'):
                self.peek()
            else:
                self.reset_peek(line_start)
                break

    def skip_indent(self):
        self.skip_blank_lines()
        self.skip_inline_ws()

    def expect_char(self, ch):
        if self.ch == ch:
            self.next()
            return True

        if ch == '\n':
            # Unicode Character 'SYMBOL FOR NEWLINE' (U+2424)
            raise ParseError('E0003', '\u2424')

        raise ParseError('E0003', ch)

    def expect_indent(self):
        self.expect_char('\n')
        self.skip_blank_lines()
        self.expect_char(' ')
        self.skip_inline_ws()

    def take_char_if(self, ch):
        if self.ch == ch:
            self.next()
            return True
        return False

    def take_char(self, f):
        ch = self.ch
        if ch is not None and f(ch):
            self.next()
            return ch
        return None

    def is_char_id_start(self, ch=None):
        if ch is None:
            return False

        cc = ord(ch)
        return (cc >= 97 and cc <= 122) or \
               (cc >= 65 and cc <= 90)

    def is_entry_id_start(self):
        if self.current_is('-'):
            self.peek()

        ch = self.current_peek()
        is_id = self.is_char_id_start(ch)
        self.reset_peek()
        return is_id

    def is_number_start(self):
        if self.current_is('-'):
            self.peek()

        cc = ord(self.current_peek())
        is_digit = cc >= 48 and cc <= 57
        self.reset_peek()
        return is_digit

    def is_char_pattern_continuation(self, ch):
        if ch is None:
            return False

        return ch not in SPECIAL_LINE_START_CHARS

    def is_peek_pattern_start(self):
        self.peek_inline_ws()
        ch = self.current_peek()

        # Inline Patterns may start with any char.
        if ch is not None and ch != '\n':
            return True

        return self.is_peek_next_line_pattern_start()

    def is_peek_next_line_zero_four_style_comment(self):
        if not self.current_peek_is('\n'):
            return False

        self.peek()

        if self.current_peek_is('/'):
            self.peek()
            if self.current_peek_is('/'):
                self.reset_peek()
                return True

        self.reset_peek()
        return False

    # -1 - any
    #  0 - comment
    #  1 - group comment
    #  2 - resource comment
    def is_peek_next_line_comment(self, level=-1):
        if not self.current_peek_is('\n'):
            return False

        i = 0

        while (i <= level or (level == -1 and i < 3)):
            self.peek()
            if not self.current_peek_is('#'):
                if i != level and level != -1:
                    self.reset_peek()
                    return False
                break
            i += 1

        self.peek()

        if self.current_peek() in [' ', '\n']:
            self.reset_peek()
            return True

        self.reset_peek()
        return False

    def is_peek_next_line_variant_start(self):
        if not self.current_peek_is('\n'):
            return False

        self.peek()

        self.peek_blank_lines()

        ptr = self.get_peek_index()

        self.peek_inline_ws()

        if (self.get_peek_index() - ptr == 0):
            self.reset_peek()
            return False

        if self.current_peek_is('*'):
            self.peek()

        if self.current_peek_is('[') and not self.peek_char_is('['):
            self.reset_peek()
            return True

        self.reset_peek()
        return False

    def is_peek_next_line_attribute_start(self):
        if not self.current_peek_is('\n'):
            return False

        self.peek()

        self.peek_blank_lines()

        ptr = self.get_peek_index()

        self.peek_inline_ws()

        if (self.get_peek_index() - ptr == 0):
            self.reset_peek()
            return False

        if self.current_peek_is('.'):
            self.reset_peek()
            return True

        self.reset_peek()
        return False

    def is_peek_next_line_pattern_start(self):
        if not self.current_peek_is('\n'):
            return False

        self.peek()

        self.peek_blank_lines()

        ptr = self.get_peek_index()

        self.peek_inline_ws()

        if (self.get_peek_index() - ptr == 0):
            self.reset_peek()
            return False

        if not self.is_char_pattern_continuation(self.current_peek()):
            self.reset_peek()
            return False

        self.reset_peek()
        return True

    def skip_to_next_entry_start(self):
        while self.ch:
            if self.current_is('\n') and not self.peek_char_is('\n'):
                self.next()

                if self.ch is None or \
                   self.is_entry_id_start() or \
                   self.current_is('#') or \
                   (self.current_is('/') and self.peek_char_is('/')) or \
                   (self.current_is('[') and self.peek_char_is('[')):
                    break
            self.next()

    def take_id_start(self, allow_term):
        if allow_term and self.current_is('-'):
            self.next()
            return '-'

        if self.is_char_id_start(self.ch):
            ret = self.ch
            self.next()
            return ret

        allowed_range = 'a-zA-Z-' if allow_term else 'a-zA-Z'
        raise ParseError('E0004', allowed_range)

    def take_id_char(self):
        def closure(ch):
            cc = ord(ch)
            return ((cc >= 97 and cc <= 122) or
                    (cc >= 65 and cc <= 90) or
                    (cc >= 48 and cc <= 57) or
                    cc == 95 or cc == 45)
        return self.take_char(closure)

    def take_variant_name_char(self):
        def closure(ch):
            if ch is None:
                return False
            cc = ord(ch)
            return (cc >= 97 and cc <= 122) or \
                   (cc >= 65 and cc <= 90) or \
                   (cc >= 48 and cc <= 57) or \
                cc == 95 or cc == 45 or cc == 32
        return self.take_char(closure)

    def take_digit(self):
        def closure(ch):
            cc = ord(ch)
            return (cc >= 48 and cc <= 57)
        return self.take_char(closure)
