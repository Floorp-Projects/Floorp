# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import signal
import which
import re

# Py3/Py2 compatibility.
try:
    from json.decoder import JSONDecodeError
except ImportError:
    JSONDecodeError = ValueError

from mozlint import result
from mozlint.util import pip
from mozprocess import ProcessHandlerMixin

here = os.path.abspath(os.path.dirname(__file__))
CODESPELL_REQUIREMENTS_PATH = os.path.join(here, 'codespell_requirements.txt')

CODESPELL_NOT_FOUND = """
Could not find codespell! Install codespell and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(CODESPELL_REQUIREMENTS_PATH)


CODESPELL_INSTALL_ERROR = """
Unable to install correct version of codespell
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(CODESPELL_REQUIREMENTS_PATH)

results = []

CODESPELL_FORMAT_REGEX = re.compile(r'(.*):(.*): (.*) ==> (.*)$')


class CodespellProcess(ProcessHandlerMixin):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs['processOutputLine'] = [self.process_line]
        ProcessHandlerMixin.__init__(self, *args, **kwargs)

    def process_line(self, line):
        try:
            match = CODESPELL_FORMAT_REGEX.match(line)
            abspath, line, typo, correct = match.groups()
        except AttributeError:
            print('Unable to match regex against output: {}'.format(line))
            return

        # Ignore false positive like aParent (which would be fixed to apparent)
        # See https://github.com/lucasdemarchi/codespell/issues/314
        m = re.match(r'^[a-z][A-Z][a-z]*', typo)
        if m:
            return
        res = {'path': os.path.relpath(abspath, self.config['root']),
               'message': typo + " ==> " + correct,
               'level': "warning",
               'lineno': line,
               }
        results.append(result.from_config(self.config, **res))

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandlerMixin.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def run_process(config, cmd):
    proc = CodespellProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()


def get_codespell_binary():
    """
    Returns the path of the first codespell binary available
    if not found returns None
    """
    binary = os.environ.get('CODESPELL')
    if binary:
        return binary

    try:
        return which.which('codespell')
    except which.WhichError:
        return None


def lint(paths, config, fix=None, **lintargs):

    if not pip.reinstall_program(CODESPELL_REQUIREMENTS_PATH):
        print(CODESPELL_INSTALL_ERROR)
        return 1

    binary = get_codespell_binary()

    if not binary:
        print(CODESPELL_NOT_FOUND)
        if 'MOZ_AUTOMATION' in os.environ:
            return 1
        return []

    config['root'] = lintargs['root']
    exclude_list = os.path.join(here, 'exclude-list.txt')
    cmd_args = [binary,
                '--disable-colors',
                # Silence some warnings:
                # 1: disable warnings about wrong encoding
                # 2: disable warnings about binary file
                # 4: shut down warnings about automatic fixes
                #    that were disabled in dictionary.
                '--quiet-level=7',
                '--ignore-words=' + exclude_list,
                # Ignore dictonnaries
                '--skip=*.dic',
                ]

    if fix:
        cmd_args.append('--write-changes')

    base_command = cmd_args + paths

    run_process(config, base_command)
    return results
