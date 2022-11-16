#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import lzma
import struct
import sys


def extract_pbzx(pbzx_path):
    with open(pbzx_path, "rb") as f:
        magic = f.read(4)
        if magic != b"pbzx":
            raise Exception("Not a PBZX payload?")
        # The first thing in the file looks like the size of each
        # decompressed chunk except the last one. It should match
        # decompressed_size in all cases except last, but we don't
        # check.
        chunk_size = f.read(8)
        chunk_size = struct.unpack(">Q", chunk_size)[0]
        with open(pbzx_path + ".cpio", "wb") as out:
            while True:
                header = f.read(16)
                if header == b"":
                    break
                if len(header) != 16:
                    raise Exception("Corrupted PBZX payload?")
                decompressed_size, compressed_size = struct.unpack(">QQ", header)
                if compressed_size == decompressed_size:
                    out.write(f.read(decompressed_size))
                else:
                    data = lzma.decompress(f.read(compressed_size))
                    if len(data) != decompressed_size:
                        raise Exception("Corrupted PBZX payload?")
                    out.write(data)


if __name__ == "__main__":
    extract_pbzx(sys.argv[1])
