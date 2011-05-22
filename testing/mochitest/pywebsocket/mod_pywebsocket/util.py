# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""WebSocket utilities.
"""


import array

# Import hash classes from a module available and recommended for each Python
# version and re-export those symbol. Use sha and md5 module in Python 2.4, and
# hashlib module in Python 2.6.
try:
    import hashlib
    md5_hash = hashlib.md5
    sha1_hash = hashlib.sha1
except ImportError:
    import md5
    import sha
    md5_hash = md5.md5
    sha1_hash = sha.sha

import StringIO
import logging
import os
import re
import traceback
import zlib


def get_stack_trace():
    """Get the current stack trace as string.

    This is needed to support Python 2.3.
    TODO: Remove this when we only support Python 2.4 and above.
          Use traceback.format_exc instead.
    """

    out = StringIO.StringIO()
    traceback.print_exc(file=out)
    return out.getvalue()


def prepend_message_to_exception(message, exc):
    """Prepend message to the exception."""

    exc.args = (message + str(exc),)
    return


def __translate_interp(interp, cygwin_path):
    """Translate interp program path for Win32 python to run cygwin program
    (e.g. perl).  Note that it doesn't support path that contains space,
    which is typically true for Unix, where #!-script is written.
    For Win32 python, cygwin_path is a directory of cygwin binaries.

    Args:
      interp: interp command line
      cygwin_path: directory name of cygwin binary, or None
    Returns:
      translated interp command line.
    """
    if not cygwin_path:
        return interp
    m = re.match('^[^ ]*/([^ ]+)( .*)?', interp)
    if m:
        cmd = os.path.join(cygwin_path, m.group(1))
        return cmd + m.group(2)
    return interp


def get_script_interp(script_path, cygwin_path=None):
    """Gets #!-interpreter command line from the script.

    It also fixes command path.  When Cygwin Python is used, e.g. in WebKit,
    it could run "/usr/bin/perl -wT hello.pl".
    When Win32 Python is used, e.g. in Chromium, it couldn't.  So, fix
    "/usr/bin/perl" to "<cygwin_path>\perl.exe".

    Args:
      script_path: pathname of the script
      cygwin_path: directory name of cygwin binary, or None
    Returns:
      #!-interpreter command line, or None if it is not #!-script.
    """
    fp = open(script_path)
    line = fp.readline()
    fp.close()
    m = re.match('^#!(.*)', line)
    if m:
        return __translate_interp(m.group(1), cygwin_path)
    return None

def wrap_popen3_for_win(cygwin_path):
    """Wrap popen3 to support #!-script on Windows.

    Args:
      cygwin_path:  path for cygwin binary if command path is needed to be
                    translated.  None if no translation required.
    """
    __orig_popen3 = os.popen3
    def __wrap_popen3(cmd, mode='t', bufsize=-1):
        cmdline = cmd.split(' ')
        interp = get_script_interp(cmdline[0], cygwin_path)
        if interp:
            cmd = interp + ' ' + cmd
        return __orig_popen3(cmd, mode, bufsize)
    os.popen3 = __wrap_popen3


def hexify(s):
    return ' '.join(map(lambda x: '%02x' % ord(x), s))


def get_class_logger(o):
    return logging.getLogger(
        '%s.%s' % (o.__class__.__module__, o.__class__.__name__))


class NoopMasker(object):
    def __init__(self):
        pass

    def mask(self, s):
        return s


class RepeatedXorMasker(object):
    """A masking object that applies XOR on the string given to mask method
    with the masking bytes given to the constructor repeatedly. This object
    remembers the position in the masking bytes the last mask method call ended
    and resumes from that point on the next mask method call.
    """

    def __init__(self, mask):
        self._mask = map(ord, mask)
        self._mask_size = len(self._mask)
        self._count = 0

    def mask(self, s):
        result = array.array('B')
        result.fromstring(s)
        for i in xrange(len(result)):
            result[i] ^= self._mask[self._count]
            self._count = (self._count + 1) % self._mask_size
        return result.tostring()


class DeflateRequest(object):
    """A wrapper class for request object to intercept send and recv to perform
    deflate compression and decompression transparently.
    """

    def __init__(self, request):
        self._request = request
        self.connection = DeflateConnection(request.connection)

    def __getattribute__(self, name):
        if name in ('_request', 'connection'):
            return object.__getattribute__(self, name)
        return self._request.__getattribute__(name)

    def __setattr__(self, name, value):
        if name in ('_request', 'connection'):
            return object.__setattr__(self, name, value)
        return self._request.__setattr__(name, value)


# By making wbits option negative, we can suppress CMF/FLG (2 octet) and
# ADLER32 (4 octet) fields of zlib so that we can use zlib module just as
# deflate library. DICTID won't be added as far as we don't set dictionary.
# LZ77 window of 32K will be used for both compression and decompression.
# For decompression, we can just use 32K to cover any windows size. For
# compression, we use 32K so receivers must use 32K.
#
# Compression level is Z_DEFAULT_COMPRESSION. We don't have to match level
# to decode.
#
# See zconf.h, deflate.cc, inflate.cc of zlib library, and zlibmodule.c of
# Python. See also RFC1950 (ZLIB 3.3).
class DeflateSocket(object):
    """A wrapper class for socket object to intercept send and recv to perform
    deflate compression and decompression transparently.
    """

    # Size of the buffer passed to recv to receive compressed data.
    _RECV_SIZE = 4096

    def __init__(self, socket):
        self._socket = socket

        self._logger = logging.getLogger(
            'mod_pywebsocket.util.DeflateSocket')

        self._compress = zlib.compressobj(
            zlib.Z_DEFAULT_COMPRESSION, zlib.DEFLATED, -zlib.MAX_WBITS)
        self._decompress = zlib.decompressobj(-zlib.MAX_WBITS)
        self._unconsumed = ''

    def recv(self, size):
        # TODO(tyoshino): Allow call with size=0. It should block until any
        # decompressed data is available.
        if size <= 0:
           raise Exception('Non-positive size passed')
        data = ''
        while True:
            data += self._decompress.decompress(
                self._unconsumed, size - len(data))
            self._unconsumed = self._decompress.unconsumed_tail
            if self._decompress.unused_data:
                raise Exception('Non-decompressible data found: %r' %
                                self._decompress.unused_data)
            if len(data) != 0:
                break

            read_data = self._socket.recv(DeflateSocket._RECV_SIZE)
            self._logger.debug('Received compressed: %r' % read_data)
            if not read_data:
                break
            self._unconsumed += read_data
        self._logger.debug('Received: %r' % data)
        return data

    def sendall(self, bytes):
        self.send(bytes)

    def send(self, bytes):
        compressed_bytes = self._compress.compress(bytes)
        compressed_bytes += self._compress.flush(zlib.Z_SYNC_FLUSH)
        self._socket.sendall(compressed_bytes)
        self._logger.debug('Wrote: %r' % bytes)
        self._logger.debug('Wrote compressed: %r' % compressed_bytes)
        return len(bytes)


class DeflateConnection(object):
    """A wrapper class for request object to intercept write and read to
    perform deflate compression and decompression transparently.
    """

    # Size of the buffer passed to recv to receive compressed data.
    _RECV_SIZE = 4096

    def __init__(self, connection):
        self._connection = connection

        self._logger = logging.getLogger(
            'mod_pywebsocket.util.DeflateConnection')

        self._compress = zlib.compressobj(
            zlib.Z_DEFAULT_COMPRESSION, zlib.DEFLATED, -zlib.MAX_WBITS)
        self._decompress = zlib.decompressobj(-zlib.MAX_WBITS)
        self._unconsumed = ''

    def put_bytes(self, bytes):
        self.write(bytes)

    def read(self, size=-1):
        # TODO(tyoshino): Allow call with size=0.
        if size == 0 or size < -1:
           raise Exception('size must be -1 or positive')

        data = ''
        while True:
            if size < 0:
                data += self._decompress.decompress(self._unconsumed)
            else:
                data += self._decompress.decompress(
                    self._unconsumed, size - len(data))
            self._unconsumed = self._decompress.unconsumed_tail
            if self._decompress.unused_data:
                raise Exception('Non-decompressible data found: %r' %
                                self._decompress.unused_data)

            if size >= 0 and len(data) != 0:
                break

            # TODO(tyoshino): Make this read efficient by some workaround.
            #
            # In 3.0.3 and prior of mod_python, read blocks until length bytes
            # was read. We don't know the exact size to read while using
            # deflate, so read byte-by-byte.
            #
            # _StandaloneRequest.read that ultimately performs
            # socket._fileobject.read also blocks until length bytes was read
            read_data = self._connection.read(1)
            self._logger.debug('Read compressed: %r' % read_data)
            if not read_data:
                break
            self._unconsumed += read_data
        self._logger.debug('Read: %r' % data)
        return data

    def write(self, bytes):
        compressed_bytes = self._compress.compress(bytes)
        compressed_bytes += self._compress.flush(zlib.Z_SYNC_FLUSH)
        self._logger.debug('Wrote compressed: %r' % compressed_bytes)
        self._logger.debug('Wrote: %r' % bytes)
        self._connection.write(compressed_bytes)


# vi:sts=4 sw=4 et
