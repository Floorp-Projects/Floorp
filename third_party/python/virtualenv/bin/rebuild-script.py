#!/usr/bin/env python
"""
Helper script to rebuild virtualenv.py from virtualenv_support
"""
from __future__ import print_function

import os
import re
import codecs
from zlib import crc32

here = os.path.dirname(__file__)
script = os.path.join(here, '..', 'virtualenv.py')

gzip = codecs.lookup('zlib')
b64 = codecs.lookup('base64')

file_regex = re.compile(
    br'##file (.*?)\n([a-zA-Z][a-zA-Z0-9_]+)\s*=\s*convert\("""\n(.*?)"""\)',
    re.S)
file_template = b'##file %(filename)s\n%(varname)s = convert("""\n%(data)s""")'

def rebuild(script_path):
    with open(script_path, 'rb') as f:
        script_content = f.read()
    parts = []
    last_pos = 0
    match = None
    for match in file_regex.finditer(script_content):
        parts += [script_content[last_pos:match.start()]]
        last_pos = match.end()
        filename, fn_decoded = match.group(1), match.group(1).decode()
        varname = match.group(2)
        data = match.group(3)

        print('Found file %s' % fn_decoded)
        pathname = os.path.join(here, '..', 'virtualenv_embedded', fn_decoded)

        with open(pathname, 'rb') as f:
            embedded = f.read()
        new_crc = crc32(embedded)
        new_data = b64.encode(gzip.encode(embedded)[0])[0]

        if new_data == data:
            print('  File up to date (crc: %s)' % new_crc)
            parts += [match.group(0)]
            continue
        # Else: content has changed
        crc = crc32(gzip.decode(b64.decode(data)[0])[0])
        print('  Content changed (crc: %s -> %s)' %
              (crc, new_crc))
        new_match = file_template % {
            b'filename': filename,
            b'varname': varname,
            b'data': new_data
        }
        parts += [new_match]

    parts += [script_content[last_pos:]]
    new_content = b''.join(parts)

    if new_content != script_content:
        print('Content updated; overwriting... ', end='')
        with open(script_path, 'wb') as f:
            f.write(new_content)
        print('done.')
    else:
        print('No changes in content')
    if match is None:
        print('No variables were matched/found')

if __name__ == '__main__':
    rebuild(script)
