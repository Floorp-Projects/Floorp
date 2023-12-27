# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# TODO: Eventually consolidate with mozpack.pkg module. This is kept separate
# for now because of the vast difference in API, and to avoid churn for the
# users of this module (docker images, macos SDK artifacts) when changes are
# necessary in mozpack.pkg
import bz2
import concurrent.futures
import io
import lzma
import os
import struct
import zlib
from collections import deque, namedtuple
from xml.etree.ElementTree import XML


class ZlibFile(object):
    def __init__(self, fileobj):
        self.fileobj = fileobj
        self.decompressor = zlib.decompressobj()
        self.buf = b""

    def read(self, length):
        cutoff = min(length, len(self.buf))
        result = self.buf[:cutoff]
        self.buf = self.buf[cutoff:]
        while len(result) < length:
            buf = self.fileobj.read(io.DEFAULT_BUFFER_SIZE)
            if not buf:
                break
            buf = self.decompressor.decompress(buf)
            cutoff = min(length - len(result), len(buf))
            result += buf[:cutoff]
            self.buf += buf[cutoff:]
        return result


def unxar(fileobj):
    magic = fileobj.read(4)
    if magic != b"xar!":
        raise Exception("Not a XAR?")

    header_size = fileobj.read(2)
    header_size = struct.unpack(">H", header_size)[0]
    if header_size > 64:
        raise Exception(
            f"Don't know how to handle a {header_size} bytes XAR header size"
        )
    header_size -= 6  # what we've read so far.
    header = fileobj.read(header_size)
    if len(header) != header_size:
        raise Exception("Failed to read XAR header")
    (
        version,
        compressed_toc_len,
        uncompressed_toc_len,
        checksum_type,
    ) = struct.unpack(">HQQL", header[:22])
    if version != 1:
        raise Exception(f"XAR version {version} not supported")
    toc = fileobj.read(compressed_toc_len)
    base = fileobj.tell()
    if len(toc) != compressed_toc_len:
        raise Exception("Failed to read XAR TOC")
    toc = zlib.decompress(toc)
    if len(toc) != uncompressed_toc_len:
        raise Exception("Corrupted XAR?")
    toc = XML(toc).find("toc")
    queue = deque(toc.findall("file"))
    while queue:
        f = queue.pop()
        queue.extend(f.iterfind("file"))
        if f.find("type").text != "file":
            continue
        filename = f.find("name").text
        data = f.find("data")
        length = int(data.find("length").text)
        size = int(data.find("size").text)
        offset = int(data.find("offset").text)
        encoding = data.find("encoding").get("style")
        fileobj.seek(base + offset, os.SEEK_SET)
        content = Take(fileobj, length)
        if encoding == "application/octet-stream":
            if length != size:
                raise Exception(f"{length} != {size}")
        elif encoding == "application/x-bzip2":
            content = bz2.BZ2File(content)
        elif encoding == "application/x-gzip":
            # Despite the encoding saying gzip, it is in fact, a raw zlib stream.
            content = ZlibFile(content)
        else:
            raise Exception(f"XAR encoding {encoding} not supported")

        yield filename, content


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
        executor = concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count())
        self.chunk_getter = executor.map(self._uncompress_chunk, self._chunker(fileobj))
        self._init_one_chunk()

    @staticmethod
    def _chunker(fileobj):
        while True:
            header = fileobj.read(16)
            if header == b"":
                break
            if len(header) != 16:
                raise Exception("Corrupted PBZX payload?")
            decompressed_size, compressed_size = struct.unpack(">QQ", header)
            chunk = fileobj.read(compressed_size)
            yield decompressed_size, compressed_size, chunk

    @staticmethod
    def _uncompress_chunk(data):
        decompressed_size, compressed_size, chunk = data
        if compressed_size != decompressed_size:
            chunk = lzma.decompress(chunk)
            if len(chunk) != decompressed_size:
                raise Exception("Corrupted PBZX payload?")
        return chunk

    def _init_one_chunk(self):
        self.offset = 0
        self.chunk = next(self.chunk_getter, "")

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


class Take(object):
    """
    File object wrapper that allows to read at most a certain length.
    """

    def __init__(self, fileobj, limit):
        self.fileobj = fileobj
        self.limit = limit

    def read(self, length=None):
        if length is None:
            length = self.limit
        else:
            length = min(length, self.limit)
        result = self.fileobj.read(length)
        self.limit -= len(result)
        return result


CpioInfo = namedtuple("CpioInfo", ["mode", "nlink", "dev", "ino"])


def uncpio(fileobj):
    while True:
        magic = fileobj.read(6)
        # CPIO payloads in mac pkg files are using the portable ASCII format.
        if magic != b"070707":
            if magic.startswith(b"0707"):
                raise Exception("Unsupported CPIO format")
            raise Exception("Not a CPIO header")
        header = fileobj.read(70)
        (
            dev,
            ino,
            mode,
            uid,
            gid,
            nlink,
            rdev,
            mtime,
            namesize,
            filesize,
        ) = struct.unpack(">6s6s6s6s6s6s6s11s6s11s", header)
        dev = int(dev, 8)
        ino = int(ino, 8)
        mode = int(mode, 8)
        nlink = int(nlink, 8)
        namesize = int(namesize, 8)
        filesize = int(filesize, 8)
        name = fileobj.read(namesize)
        if name[-1] != 0:
            raise Exception("File name is not NUL terminated")
        name = name[:-1]
        if name == b"TRAILER!!!":
            break

        if b"/../" in name or name.startswith(b"../") or name == b"..":
            raise Exception(".. is forbidden in file name")
        if name.startswith(b"."):
            name = name[1:]
        if name.startswith(b"/"):
            name = name[1:]
        content = Take(fileobj, filesize)
        yield name, CpioInfo(mode=mode, nlink=nlink, dev=dev, ino=ino), content
        # Ensure the content is totally consumed
        while content.read(4096):
            pass
