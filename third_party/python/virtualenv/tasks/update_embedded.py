#!/usr/bin/env python
"""
Helper script to rebuild virtualenv.py from virtualenv_support
"""
from __future__ import print_function, unicode_literals

import codecs
import os
import re
import sys
from zlib import crc32 as _crc32

if sys.version_info < (3,):
    print("requires Python 3 (use tox from Python 3 if invoked via tox)")
    raise SystemExit(1)


def crc32(data):
    """Python version idempotent"""
    return _crc32(data.encode()) & 0xFFFFFFFF


here = os.path.realpath(os.path.dirname(__file__))
script = os.path.realpath(os.path.join(here, "..", "virtualenv.py"))

gzip = codecs.lookup("zlib")
b64 = codecs.lookup("base64")

file_regex = re.compile(r'# file (.*?)\n([a-zA-Z][a-zA-Z0-9_]+) = convert\(\n    """\n(.*?)"""\n\)', re.S)
file_template = '# file {filename}\n{variable} = convert(\n    """\n{data}"""\n)'


def rebuild(script_path):
    with open(script_path, "rt") as current_fh:
        script_content = current_fh.read()
    script_parts = []
    match_end = 0
    next_match = None
    _count, did_update = 0, False
    for _count, next_match in enumerate(file_regex.finditer(script_content)):
        script_parts += [script_content[match_end : next_match.start()]]
        match_end = next_match.end()
        filename, variable_name, previous_encoded = next_match.group(1), next_match.group(2), next_match.group(3)
        differ, content = handle_file(next_match.group(0), filename, variable_name, previous_encoded)
        script_parts.append(content)
        if differ:
            did_update = True

    script_parts += [script_content[match_end:]]
    new_content = "".join(script_parts)

    report(1 if not _count or did_update else 0, new_content, next_match, script_content, script_path)


def handle_file(previous_content, filename, variable_name, previous_encoded):
    print("Found file {}".format(filename))
    current_path = os.path.realpath(os.path.join(here, "..", "virtualenv_embedded", filename))
    _, file_type = os.path.splitext(current_path)
    keep_line_ending = file_type in (".bat",)
    with open(current_path, "rt", encoding="utf-8", newline="" if keep_line_ending else None) as current_fh:
        current_text = current_fh.read()
    current_crc = crc32(current_text)
    current_encoded = b64.encode(gzip.encode(current_text.encode())[0])[0].decode()
    if current_encoded == previous_encoded:
        print("  File up to date (crc: {:08x})".format(current_crc))
        return False, previous_content
    # Else: content has changed
    previous_text = gzip.decode(b64.decode(previous_encoded.encode())[0])[0].decode()
    previous_crc = crc32(previous_text)
    print("  Content changed (crc: {:08x} -> {:08x})".format(previous_crc, current_crc))
    new_part = file_template.format(filename=filename, variable=variable_name, data=current_encoded)
    return True, new_part


def report(exit_code, new, next_match, current, script_path):
    if new != current:
        print("Content updated; overwriting... ", end="")
        with open(script_path, "wt") as current_fh:
            current_fh.write(new)
        print("done.")
    else:
        print("No changes in content")
    if next_match is None:
        print("No variables were matched/found")
    raise SystemExit(exit_code)


if __name__ == "__main__":
    rebuild(script)
