from __future__ import unicode_literals


class StringIter():
    def __init__(self, source):
        self.source = source
        self.len = len(source)
        self.i = 0

    def next(self):
        if self.i < self.len:
            ret = self.source[self.i]
            self.i += 1
            return ret
        return None

    def get_slice(self, start, end):
        return self.source[start:end]


class ParserStream():
    def __init__(self, string):
        self.iter = StringIter(string)
        self.buf = []
        self.peek_index = 0
        self.index = 0

        self.ch = None

        self.iter_end = False
        self.peek_end = False

        self.ch = self.iter.next()

    def next(self):
        if self.iter_end:
            return None

        if len(self.buf) == 0:
            self.ch = self.iter.next()
        else:
            self.ch = self.buf.pop(0)

        self.index += 1

        if self.ch == None:
            self.iter_end = True
            self.peek_end = True

        self.peek_index = self.index

        return self.ch

    def current(self):
        return self.ch

    def current_is(self, ch):
        return self.ch == ch

    def current_peek(self):
        if self.peek_end:
            return None

        diff = self.peek_index - self.index

        if diff == 0:
            return self.ch
        return self.buf[diff - 1]

    def current_peek_is(self, ch):
        return self.current_peek() == ch

    def peek(self):
        if self.peek_end:
            return None

        self.peek_index += 1

        diff = self.peek_index - self.index

        if diff > len(self.buf):
            ch = self.iter.next()
            if ch is not None:
                self.buf.append(ch)
            else:
                self.peek_end = True
                return None

        return self.buf[diff - 1]

    def get_index(self):
        return self.index

    def get_peek_index(self):
        return self.peek_index

    def peek_char_is(self, ch):
        if self.peek_end:
            return False

        ret = self.peek()

        self.peek_index -= 1

        return ret == ch

    def reset_peek(self):
        self.peek_index = self.index
        self.peek_end = self.iter_end

    def skip_to_peek(self):
        diff = self.peek_index - self.index

        for i in range(0, diff):
            self.ch = self.buf.pop(0)

        self.index = self.peek_index

    def get_slice(self, start, end):
        return self.iter.get_slice(start, end)
