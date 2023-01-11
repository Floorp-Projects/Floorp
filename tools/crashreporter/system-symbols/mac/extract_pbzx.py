# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import lzma
import shutil
import struct


class Pbzx(object):
    def __init__(self, fileobj):
        magic = fileobj.read(4)
        if magic != b"pbzx":
            raise Exception("Not a PBZX payload?")
        # The first thing in the file looks like the size of each
        # decompressed chunk except the last one. It should match
        # decompressed_size in all cases except last, but we don't
        # check.
        chunk_size = fileobj.read(8)
        chunk_size = struct.unpack(">Q", chunk_size)[0]
        self.fileobj = fileobj
        self._init_one_chunk()

    def _init_one_chunk(self):
        self.offset = 0
        header = self.fileobj.read(16)
        if header == b"":
            self.chunk = ""
            return
        if len(header) != 16:
            raise Exception("Corrupted PBZX payload?")
        decompressed_size, compressed_size = struct.unpack(">QQ", header)
        chunk = self.fileobj.read(compressed_size)
        if compressed_size == decompressed_size:
            self.chunk = chunk
        else:
            self.chunk = lzma.decompress(chunk)
            if len(self.chunk) != decompressed_size:
                raise Exception("Corrupted PBZX payload?")

    def read(self, length=None):
        if length == 0:
            return b""
        if length and len(self.chunk) >= self.offset + length:
            start = self.offset
            self.offset += length
            return self.chunk[start : self.offset]
        else:
            result = self.chunk[self.offset :]
            self._init_one_chunk()
            if self.chunk:
                # XXX: suboptimal if length is larger than the chunk size
                result += self.read(None if length is None else length - len(result))
            return result


def extract_pbzx(pbzx_path):
    with open(pbzx_path, "rb") as f:
        pbzx = Pbzx(f)
        with open(pbzx_path + ".cpio", "wb") as out:
            shutil.copyfileobj(pbzx, out)
