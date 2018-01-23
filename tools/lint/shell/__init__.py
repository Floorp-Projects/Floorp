# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import json
import signal
import which

# Py3/Py2 compatibility.
try:
    from json.decoder import JSONDecodeError
except ImportError:
    JSONDecodeError = ValueError

import mozpack.path as mozpath
from mozpack.files import FileFinder
from mozlint import result
from mozprocess import ProcessHandlerMixin


SHELLCHECK_NOT_FOUND = """
Unable to locate shellcheck, please ensure it is installed and in
your PATH or set the SHELLCHECK environment variable.

https://shellcheck.net or your system's package manager.
""".strip()

results = []


class ShellcheckProcess(ProcessHandlerMixin):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs['processOutputLine'] = [self.process_line]
        ProcessHandlerMixin.__init__(self, *args, **kwargs)

    def process_line(self, line):
        try:
            data = json.loads(line)
        except JSONDecodeError as e:
            print('Unable to load shellcheck output: {}'.format(e))
            return

        for entry in data:
            res = {
                'path': entry['file'],
                'message': entry['message'],
                'level': 'error' if entry['level'] == 'error' else 'warning',
                'lineno': entry['line'],
                'column': entry['column'],
                'rule': entry['code'],
            }
            results.append(result.from_config(self.config, **res))

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandlerMixin.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def determine_shell_from_script(path):
    """Returns a string identifying the shell used.

    Returns None if not identifiable.

    Copes with the following styles:
    #!bash
    #!/bin/bash
    #!/usr/bin/env bash
    """
    with open(path, 'r') as f:
        head = f.readline()

        if not head.startswith('#!'):
            return

        # allow for parameters to the shell
        shebang = head.split()[0]

        # if the first entry is a variant of /usr/bin/env
        if 'env' in shebang:
            shebang = head.split()[1]

        if shebang.endswith('sh'):
            # Strip first to avoid issues with #!bash
            return shebang.strip('#!').split('/')[-1]
    # make it clear we return None, rather than fall through.
    return


def find_shell_scripts(config, paths):
    found = dict()

    root = config['root']
    exclude = [mozpath.join(root, e) for e in config.get('exclude', [])]

    if config.get('extensions'):
        pattern = '**/*.{}'.format(config.get('extensions')[0])
    else:
        pattern = '**/*.sh'

    files = []
    for path in paths:
        path = mozpath.normsep(path)
        ignore = [e[len(path):].lstrip('/') for e in exclude
                  if mozpath.commonprefix((path, e)) == path]
        finder = FileFinder(path, ignore=ignore)
        files.extend([os.path.join(path, p) for p, f in finder.find(pattern)])

    for filename in files:
        shell = determine_shell_from_script(filename)
        if shell:
            found[filename] = shell
    return found


def run_process(config, cmd):
    proc = ShellcheckProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()


def get_shellcheck_binary():
    """
    Returns the path of the first shellcheck binary available
    if not found returns None
    """
    binary = os.environ.get('SHELLCHECK')
    if binary:
        return binary

    try:
        return which.which('shellcheck')
    except which.WhichError:
        return None


def lint(paths, config, **lintargs):

    binary = get_shellcheck_binary()

    if not binary:
        print(SHELLCHECK_NOT_FOUND)
        if 'MOZ_AUTOMATION' in os.environ:
            return 1
        return []

    config['root'] = lintargs['root']

    files = find_shell_scripts(config, paths)

    base_command = [binary, '-x', '-f', 'json']
    if config.get('excludecodes'):
        base_command.extend(['-e', ','.join(config.get('excludecodes'))])

    for f in files:
        cmd = list(base_command)
        cmd.extend(['-s', files[f], f])
        run_process(config, cmd)
    return results
