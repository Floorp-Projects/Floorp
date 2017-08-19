"""Encoding utilities"""

import os
import sys

try:
    import builtins
except ImportError:
    import __builtin__ as builtins

__all__ = ['b', 'bchr', 'bytestype', 'envencode', 'fsdecode', 'fsencode',
           'stdoutb', 'stderrb', 'u', 'ul', 'unicodetype']

bytestype = getattr(builtins, 'bytes', str)
unicodetype = getattr(builtins, 'unicode', str)

if getattr(os, 'fsdecode', None) is not None:
    fsdecode = os.fsdecode
    fsencode = os.fsencode
elif bytestype is not str:
    if sys.platform == 'win32':
        def fsdecode(s):
            """Decode a filename from the filesystem encoding"""
            if isinstance(s, unicodetype):
                return s
            encoding = sys.getfilesystemencoding()
            if encoding == 'mbcs':
                return s.decode(encoding)
            else:
                return s.decode(encoding, 'surrogateescape')

        def fsencode(s):
            """Encode a filename to the filesystem encoding"""
            if isinstance(s, bytestype):
                return s
            encoding = sys.getfilesystemencoding()
            if encoding == 'mbcs':
                return s.encode(encoding)
            else:
                return s.encode(encoding, 'surrogateescape')
    else:
        def fsdecode(s):
            """Decode a filename from the filesystem encoding"""
            if isinstance(s, unicodetype):
                return s
            return s.decode(sys.getfilesystemencoding(), 'surrogateescape')

        def fsencode(s):
            """Encode a filename to the filesystem encoding"""
            if isinstance(s, bytestype):
                return s
            return s.encode(sys.getfilesystemencoding(), 'surrogateescape')
else:
    def fsdecode(s):
        """Decode a filename from the filesystem encoding"""
        return s

    def fsencode(s):
        """Encode a filename to the filesystem encoding"""
        return s

if bytestype is str:
    def envencode(s):
        """Encode a byte string to the os.environ encoding"""
        return s
else:
    envencode = fsdecode

if getattr(sys.stdout, 'buffer', None) is not None:
    stdoutb = sys.stdout.buffer
    stderrb = sys.stderr.buffer
else:
    stdoutb = sys.stdout
    stderrb = sys.stderr

if bytestype is str:
    def b(s):
        """Convert an ASCII string literal into a bytes object"""
        return s

    bchr = chr

    def u(s):
        """Convert an ASCII string literal into a unicode object"""
        return s.decode('ascii')
else:
    def b(s):
        """Convert an ASCII string literal into a bytes object"""
        return s.encode('ascii')

    def bchr(i):
        """Return a bytes character for a given integer value"""
        return bytestype([i])

    def u(s):
        """Convert an ASCII string literal into a unicode object"""
        return s

try:
    eval(r'u""')
except SyntaxError:
    ul = eval
else:
    def ul(e):
        """Evaluate e as a unicode string literal"""
        return eval('u' + e)
