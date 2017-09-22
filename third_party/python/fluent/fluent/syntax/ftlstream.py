from __future__ import unicode_literals
from .stream import ParserStream
from .errors import ParseError


INLINE_WS = (' ', '\t')


class FTLParserStream(ParserStream):
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

    def skip_inline_ws(self):
        while self.ch:
            if self.ch not in INLINE_WS:
                break
            self.next()

    def expect_char(self, ch):
        if self.ch == ch:
            self.next()
            return True

        if ch == '\n':
            # Unicode Character 'SYMBOL FOR NEWLINE' (U+2424)
            raise ParseError('E0003', '\u2424')

        raise ParseError('E0003', ch)

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

    def is_id_start(self):
        if self.ch is None:
            return False

        cc = ord(self.ch)

        return (cc >= 97 and cc <= 122) or \
               (cc >= 65 and cc <= 90) or \
                cc == 95

    def is_number_start(self):
        cc = ord(self.ch)

        return (cc >= 48 and cc <= 57) or cc == 45

    def is_peek_next_line_variant_start(self):
        if not self.current_peek_is('\n'):
            return False

        self.peek()

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

    def is_peek_next_line_pattern(self):
        if not self.current_peek_is('\n'):
            return False

        self.peek()

        ptr = self.get_peek_index()

        self.peek_inline_ws()

        if (self.get_peek_index() - ptr == 0):
            self.reset_peek()
            return False

        if (self.current_peek_is('}') or
                self.current_peek_is('.') or
                self.current_peek_is('#') or
                self.current_peek_is('[') or
                self.current_peek_is('*')):
            self.reset_peek()
            return False

        self.reset_peek()
        return True

    def is_peek_next_line_tag_start(self):

        if not self.current_peek_is('\n'):
            return False

        self.peek()

        ptr = self.get_peek_index()

        self.peek_inline_ws()

        if (self.get_peek_index() - ptr == 0):
            self.reset_peek()
            return False

        if self.current_peek_is('#'):
            self.reset_peek()
            return True

        self.reset_peek()
        return False

    def skip_to_next_entry_start(self):
        while self.ch:
            if self.current_is('\n') and not self.peek_char_is('\n'):
                self.next()

                if self.ch is None or self.is_id_start() or \
                   (self.current_is('/') and self.peek_char_is('/')) or \
                   (self.current_is('[') and self.peek_char_is('[')):
                    break
            self.next()

    def take_id_start(self):
        if self.is_id_start():
            ret = self.ch
            self.next()
            return ret

        raise ParseError('E0004', 'a-zA-Z_')

    def take_id_char(self):
        def closure(ch):
            cc = ord(ch)
            return ((cc >= 97 and cc <= 122) or
                    (cc >= 65 and cc <= 90) or
                    (cc >= 48 and cc <= 57) or
                    cc == 95 or cc == 45)
        return self.take_char(closure)

    def take_symb_char(self):
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
