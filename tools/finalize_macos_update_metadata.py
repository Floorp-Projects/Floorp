#!/usr/bin/env python3
"""Bind macOS full-update metadata to the actual signed app and MAR payload."""

import argparse
import configparser
import hashlib
import json
import lzma
import re
import struct
from pathlib import Path

FILES = (
    "Contents/Resources/application.ini",
    "Contents/Resources/platform.ini",
    "Contents/Resources/browser/buildid2",
)
MAX_FILE = 1024 * 1024


def mar_files(path):
    result = {}
    total = path.stat().st_size
    with path.open("rb") as src:
        if src.read(4) != b"MAR1":
            raise ValueError("invalid MAR header")
        offset_data = src.read(4)
        if len(offset_data) != 4:
            raise ValueError("missing MAR index offset")
        index = struct.unpack(">I", offset_data)[0]
        if index + 4 > total:
            raise ValueError("invalid MAR index offset")
        src.seek(index)
        index_size = struct.unpack(">I", src.read(4))[0]
        if index_size > 64 * 1024 * 1024 or index + 4 + index_size > total:
            raise ValueError("invalid MAR index size")
        entries = src.read(index_size)
        pos = 0
        while pos < len(entries):
            if pos + 12 > len(entries):
                raise ValueError("truncated MAR index")
            start, length, _ = struct.unpack_from(">III", entries, pos)
            pos += 12
            end = entries.find(b"\0", pos)
            if end < 0:
                raise ValueError("unterminated MAR entry")
            name = entries[pos:end].decode("utf-8")
            pos = end + 1
            if name not in FILES:
                continue
            if name in result or length > MAX_FILE or start + length > index:
                raise ValueError("duplicate or invalid MAR entry: " + name)
            src.seek(start)
            decoder = lzma.LZMADecompressor()
            content = decoder.decompress(src.read(length), max_length=MAX_FILE + 1)
            if len(content) > MAX_FILE or not decoder.eof or decoder.unused_data:
                raise ValueError("invalid compressed MAR entry: " + name)
            result[name] = content
    if set(result) != set(FILES):
        raise ValueError("missing required MAR entries")
    return result


def finalize(app, mar, metadata):
    payload = mar_files(mar)
    for name in FILES:
        if (app / name).read_bytes() != payload[name]:
            raise ValueError("signed app and MAR differ: " + name)
    app_ini = configparser.ConfigParser(interpolation=None, strict=True)
    app_ini.read_string(payload[FILES[0]].decode("utf-8"))
    platform_ini = configparser.ConfigParser(interpolation=None, strict=True)
    platform_ini.read_string(payload[FILES[1]].decode("utf-8"))
    buildid = app_ini["App"]["BuildID"]
    if (
        not re.fullmatch(r"[0-9]{14}", buildid)
        or buildid != platform_ini["Build"]["BuildID"]
    ):
        raise ValueError("invalid or inconsistent packaged BuildID")
    data = json.loads(metadata.read_text(encoding="utf-8"))
    if (
        data["version_display"] != app_ini["App"]["Version"]
        or data["version"] != platform_ini["Build"]["Milestone"]
        or data["noraneko_buildid"] != payload[FILES[2]].decode("utf-8").strip()
    ):
        raise ValueError("packaged versions or Noraneko build ID do not match metadata")
    digest = hashlib.sha512()
    with mar.open("rb") as src:
        for block in iter(lambda: src.read(1024 * 1024), b""):
            digest.update(block)
    data.update(
        buildid=buildid, mar_size=mar.stat().st_size, mar_shasum=digest.hexdigest()
    )
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("mar", type=Path)
    parser.add_argument("metadata", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(finalize(args.app, args.mar, args.metadata), indent=2) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
