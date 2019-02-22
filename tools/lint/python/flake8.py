# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import platform
import subprocess
import sys

import mozfile

from mozlint import result
from mozlint.util import pip

here = os.path.abspath(os.path.dirname(__file__))
FLAKE8_REQUIREMENTS_PATH = os.path.join(here, 'flake8_requirements.txt')

FLAKE8_NOT_FOUND = """
Could not find flake8! Install flake8 and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(FLAKE8_REQUIREMENTS_PATH)


FLAKE8_INSTALL_ERROR = """
Unable to install correct version of flake8
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(FLAKE8_REQUIREMENTS_PATH)

LINE_OFFSETS = {
    # continuation line under-indented for hanging indent
    'E121': (-1, 2),
    # continuation line missing indentation or outdented
    'E122': (-1, 2),
    # continuation line over-indented for hanging indent
    'E126': (-1, 2),
    # continuation line over-indented for visual indent
    'E127': (-1, 2),
    # continuation line under-indented for visual indent
    'E128': (-1, 2),
    # continuation line unaligned for hanging indend
    'E131': (-1, 2),
    # expected 1 blank line, found 0
    'E301': (-1, 2),
    # expected 2 blank lines, found 1
    'E302': (-2, 3),
}
"""Maps a flake8 error to a lineoffset tuple.

The offset is of the form (lineno_offset, num_lines) and is passed
to the lineoffset property of an `Issue`.
"""

# We use sys.prefix to find executables as that gets modified with
# virtualenv's activate_this.py, whereas sys.executable doesn't.
if platform.system() == 'Windows':
    bindir = os.path.join(sys.prefix, 'Scripts')
else:
    bindir = os.path.join(sys.prefix, 'bin')


def setup(root):
    if not pip.reinstall_program(FLAKE8_REQUIREMENTS_PATH):
        print(FLAKE8_INSTALL_ERROR)
        return 1


def lint(paths, config, **lintargs):
    from flake8.main.application import Application

    config_path = os.path.join(lintargs['root'], '.flake8')
    exclude = config.get('exclude', [])

    if lintargs.get('fix'):
        fix_cmd = [
            os.path.join(bindir, 'autopep8'),
            '--global-config', config_path,
            '--in-place', '--recursive',
        ]

        if exclude:
            fix_cmd.extend(['--exclude', ','.join(exclude)])

        subprocess.call(fix_cmd + paths)

    output_file = mozfile.NamedTemporaryFile()
    flake8_cmd = [
        os.path.join(bindir, 'flake8'),
        '--config', config_path,
        '--output-file', output_file.name,
        '--format', '{"path":"%(path)s","lineno":%(row)s,'
                    '"column":%(col)s,"rule":"%(code)s","message":"%(text)s"}',
        '--filename', ','.join(['*.{}'.format(e) for e in config['extensions']]),
    ] + paths

    # Run flake8.
    results = []
    app = Application()
    app.run(flake8_cmd)

    def process_line(line):
        # Escape slashes otherwise JSON conversion will not work
        line = line.replace('\\', '\\\\')
        try:
            res = json.loads(line)
        except ValueError:
            print('Non JSON output from linter, will not be processed: {}'.format(line))
            return

        if res.get('code') in LINE_OFFSETS:
            res['lineoffset'] = LINE_OFFSETS[res['code']]

        results.append(result.from_config(config, **res))

    map(process_line, output_file.readlines())
    return results
