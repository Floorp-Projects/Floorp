# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import signal
import subprocess
from collections import defaultdict

import which
from mozprocess import ProcessHandlerMixin

from mozlint import result
from mozlint.pathutils import get_ancestors_by_name


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
to the lineoffset property of `ResultContainer`.
"""

results = []


class Flake8Process(ProcessHandlerMixin):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs['processOutputLine'] = [self.process_line]
        ProcessHandlerMixin.__init__(self, *args, **kwargs)

    def process_line(self, line):
        # Escape slashes otherwise JSON conversion will not work
        line = line.replace('\\', '\\\\')
        try:
            res = json.loads(line)
        except ValueError:
            print('Non JSON output from linter, will not be processed: {}'.format(line))
            return

        if 'code' in res:
            if res['code'].startswith('W'):
                res['level'] = 'warning'

            if res['code'] in LINE_OFFSETS:
                res['lineoffset'] = LINE_OFFSETS[res['code']]

        results.append(result.from_config(self.config, **res))

    def run(self, *args, **kwargs):
        # flake8 seems to handle SIGINT poorly. Handle it here instead
        # so we can kill the process without a cryptic traceback.
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandlerMixin.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def get_flake8_binary():
    """
    Returns the path of the first flake8 binary available
    if not found returns None
    """
    binary = os.environ.get('FLAKE8')
    if binary:
        return binary

    try:
        return which.which('flake8')
    except which.WhichError:
        return None


def _run_pip(*args):
    """
    Helper function that runs pip with subprocess
    """
    try:
        subprocess.check_output(['pip'] + list(args),
                                stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError as e:
        print(e.output)
        return False


def reinstall_flake8():
    """
    Try to install flake8 at the target version, returns True on success
    otherwise prints the otuput of the pip command and returns False
    """
    if _run_pip('install', '-U',
                '--require-hashes', '-r',
                FLAKE8_REQUIREMENTS_PATH):
        return True

    return False


def run_process(config, cmd):
    proc = Flake8Process(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()


def lint(paths, config, **lintargs):

    if not reinstall_flake8():
        print(FLAKE8_INSTALL_ERROR)
        return 1

    binary = get_flake8_binary()

    cmdargs = [
        binary,
        '--format', '{"path":"%(path)s","lineno":%(row)s,'
                    '"column":%(col)s,"rule":"%(code)s","message":"%(text)s"}',
    ]

    # Run any paths with a .flake8 file in the directory separately so
    # it gets picked up. This means only .flake8 files that live in
    # directories that are explicitly included will be considered.
    # See bug 1277851
    paths_by_config = defaultdict(list)
    for path in paths:
        configs = get_ancestors_by_name('.flake8', path, lintargs['root'])
        paths_by_config[os.pathsep.join(configs) if configs else 'default'].append(path)

    for configs, paths in paths_by_config.items():
        cmd = cmdargs[:]

        if configs != 'default':
            configs = reversed(configs.split(os.pathsep))
            cmd.extend(['--append-config={}'.format(c) for c in configs])

        cmd.extend(paths)
        run_process(config, cmd)

    return results
