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
from mozlint.pathutils import expand_exclusions
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


class NothingToLint(Exception):
    """Exception used to bail out of flake8's internals if all the specified
    files were excluded.
    """


def setup(root, **lintargs):
    if not pip.reinstall_program(FLAKE8_REQUIREMENTS_PATH):
        print(FLAKE8_INSTALL_ERROR)
        return 1


def lint(paths, config, **lintargs):
    from flake8.main.application import Application

    root = lintargs['root']
    config_path = os.path.join(root, '.flake8')

    if lintargs.get('fix'):
        fix_cmd = [
            os.path.join(bindir, 'autopep8'),
            '--global-config', config_path,
            '--in-place', '--recursive',
        ]

        if config.get('exclude'):
            fix_cmd.extend(['--exclude', ','.join(config['exclude'])])

        subprocess.call(fix_cmd + paths)

    # Run flake8.
    app = Application()

    output_file = mozfile.NamedTemporaryFile()
    flake8_cmd = [
        '--config', config_path,
        '--output-file', output_file.name,
        '--format', '{"path":"%(path)s","lineno":%(row)s,'
                    '"column":%(col)s,"rule":"%(code)s","message":"%(text)s"}',
        '--filename', ','.join(['*.{}'.format(e) for e in config['extensions']]),
    ]

    orig_make_file_checker_manager = app.make_file_checker_manager

    def wrap_make_file_checker_manager(self):
        """Flake8 is very inefficient when it comes to applying exclusion
        rules, using `expand_exclusions` to turn directories into a list of
        relevant python files is an order of magnitude faster.

        Hooking into flake8 here also gives us a convenient place to merge the
        `exclude` rules specified in the root .flake8 with the ones added by
        tools/lint/mach_commands.py.
        """
        config.setdefault('exclude', []).extend(self.options.exclude)
        self.options.exclude = None
        self.args = self.args + list(expand_exclusions(paths, config, root))

        if not self.args:
            raise NothingToLint
        return orig_make_file_checker_manager()

    app.make_file_checker_manager = wrap_make_file_checker_manager.__get__(app, Application)

    # Make sure to run from repository root so exclusions are joined to the
    # repository root and not the current working directory.
    oldcwd = os.getcwd()
    os.chdir(root)
    try:
        app.run(flake8_cmd)
    except NothingToLint:
        pass
    finally:
        os.chdir(oldcwd)

    results = []

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
