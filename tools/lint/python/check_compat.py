#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import ast
import json
import sys


def parse_file(f):
    with open(f, 'rb') as fh:
        content = fh.read()
    try:
        return ast.parse(content)
    except SyntaxError as e:
        err = {
            'path': f,
            'message': e.msg,
            'lineno': e.lineno,
            'column': e.offset,
            'source': e.text,
            'rule': 'is-parseable',
        }
        print(json.dumps(err))


def check_compat_py2(f):
    """Check Python 2 and Python 3 compatibility for a file with Python 2"""
    root = parse_file(f)

    # Ignore empty or un-parseable files.
    if not root or not root.body:
        return

    futures = set()
    haveprint = False
    future_lineno = 1
    for node in ast.walk(root):
        if isinstance(node, ast.ImportFrom):
            if node.module == '__future__':
                future_lineno = node.lineno
                futures |= set(n.name for n in node.names)
        elif isinstance(node, ast.Print):
            haveprint = True

    err = {
        'path': f,
        'lineno': future_lineno,
        'column': 1,
    }

    if 'absolute_import' not in futures:
        err['rule'] = 'require absolute_import'
        err['message'] = 'Missing from __future__ import absolute_import'
        print(json.dumps(err))

    if haveprint and 'print_function' not in futures:
        err['rule'] = 'require print_function'
        err['message'] = 'Missing from __future__ import print_function'
        print(json.dumps(err))


def check_compat_py3(f):
    """Check Python 3 compatibility of a file with Python 3."""
    parse_file(f)


if __name__ == '__main__':
    if sys.version_info[0] == 2:
        fn = check_compat_py2
    else:
        fn = check_compat_py3

    manifest = sys.argv[1]
    with open(manifest, 'r') as fh:
        files = fh.read().splitlines()

    for f in files:
        fn(f)

    sys.exit(0)
