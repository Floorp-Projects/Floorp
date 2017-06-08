#
#   Encodings and Formats for Elliptic Curve Cryptography
#

import StringIO

# Big-Endian Encoding

def enc_long(n):
    '''Encodes arbitrarily large number n to a sequence of bytes.
    Big endian byte order is used.'''
    s = ""
    while n > 0:
        s = chr(n & 0xFF) + s
        n >>= 8
    return s

def enc_int(n):
    '''Encodes an integer n to a 4-byte string.
    Big endian byte order is used.'''
    return chr((n >> 24) & 0xFF) + chr((n >> 16) & 0xFF) + \
           chr((n >>  8) & 0xFF) + chr( n        & 0xFF)

def enc_fixed_long(n, length):
    return enc_long(n)[:length].rjust(length, '\x00')

def dec_long(s):
    '''Decodes s to its numeric representation.
    Big endian byte order is used.'''
    n = 0
    for c in s:
        n = (n << 8) | ord(c)
    return n

# dec_int not necessary,
# dec_long does the same when provided with 4 bytes input.

# Chunks

def enc_chunks(*args):
    '''Chain given string args or sub-chunks to a single chunk'''
    return ''.join([enc_int(len(a)) + a for a in args])

def dec_chunks(s):
    '''Split a chunk into strings or sub-chunks'''
    i = 0
    result = []
    while i < len(s):
        size = dec_long(s[i : i + 4])
        i += 4
        result.append(s[i : i + size])
        i += size
    return result

# Point and signature data

def enc_point(p):
    '''Encode a point p = (x, y)'''
    x, y = p
    sx = enc_long(x)
    sy = enc_long(y)
    diff = len(sx) - len(sy)
    if diff > 0:
        sy = '\x00' * diff + sy
    elif diff < 0:
        sx = '\x00' * -diff + sx
    return sx + sy

def dec_point(s):
    '''Decode an even length string s to a point(x, y)'''
    d = len(s) / 2
    return (dec_long(s[:d]), dec_long(s[d:]))


class Encoder:

    def __init__(self):
        self._io = StringIO.StringIO()

    def int(self, n, size = 4):
        self._io.write(enc_fixed_long(n, size))
        return self

    def long(self, n, pre = 2):
        lstr = enc_long(n)
        self._io.write(enc_fixed_long(len(lstr), pre) + lstr)
        return self

    def str(self, s, pre = 2):
        self._io.write(enc_fixed_long(len(s), pre) + s)
        return self

    def point(self, p, pre = 2):
        lstr = enc_point(p)
        self._io.write(enc_fixed_long(len(lstr), pre) + lstr)
        return self

    def chunk(self, enc, pre = 2):
        lstr = enc.out()
        self._io.write(enc_fixed_long(len(lstr), pre) + lstr)
        return self

    def out(self):
        return self._io.getvalue()

class Decoder:

    def __init__(self, data, offset = 0):
        self._io = StringIO.StringIO(data)
        self._io.seek(offset)
        self._res = []
        self._limit = None
        self._parent = None

    def _ret(self):
##        if self._parent and self._io.tell() >= self._limit:
##            return self.exit()
##        else:
##            return self
        return self

    def int(self, size = 4):
        self._res.append(dec_long(self._io.read(size)))
        return self._ret()
        

    def long(self, pre = 2):
        llen = dec_long(self._io.read(pre))
        self._res.append(dec_long(self._io.read(llen)))
        return self._ret()

    def str(self, pre = 2):
        llen = dec_long(self._io.read(pre))
        self._res.append(self._io.read(llen))
        return self._ret()

    def point(self, pre = 2):
        llen = dec_long(self._io.read(pre))
        self._res.append(dec_point(self._io.read(llen)))
        return self._ret()

    def enter(self, pre = 2):
        llen = dec_long(self._io.read(pre))
        subcoder = Decoder("")
        subcoder._io = self._io
        subcoder._parent = self
        subcoder._limit = self._io.tell() + llen
        return subcoder

    def chunk(self, pre = 2):
        llen = dec_long(self._io.read(pre))
        self._res.append(Decoder(self._io.read(llen)))
        return self._ret()

    def exit(self):
        if self._parent:
            self._parent._io.seek(self._limit)
            self._parent._res.append(self._res)
            return self._parent
        else:
            raise RuntimeError, "Cannont exit top level Decoder"

    def continues(self):
        return (not self._limit) or (self._io.tell() < self._limit)

    def out(self, exit_all = False):
        if exit_all and self._parent:
            return self.exit().out()
        else:
            r = self._res
            self._res = []
            return r

    def only(self):
        if self._res:
            return self._res.pop(0)
        else:
            return RuntimeError, "Only what? (Empty decoder stack)"
