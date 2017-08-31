# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import os
import signal
import subprocess
from collections import defaultdict

import which
from mozprocess import ProcessHandlerMixin

from mozlint import result
from mozlint.pathutils import get_ancestors_by_name


here = os.path.abspath(os.path.dirname(__file__))
YAMLLINT_REQUIREMENTS_PATH = os.path.join(here, 'yamllint_requirements.txt')


YAMLLINT_INSTALL_ERROR = """
Unable to install correct version of yamllint
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(YAMLLINT_REQUIREMENTS_PATH)

YAMLLINT_FORMAT_REGEX = re.compile(r'(.*):(.*):(.*): \[(error|warning)\] (.*) \((.*)\)$')

results = []


class YAMLLintProcess(ProcessHandlerMixin):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs['processOutputLine'] = [self.process_line]
        ProcessHandlerMixin.__init__(self, *args, **kwargs)

    def process_line(self, line):
        try:
            match = YAMLLINT_FORMAT_REGEX.match(line)
            abspath, line, col, level, message, code = match.groups()
        except AttributeError:
            print('Unable to match yaml regex against output: {}'.format(line))
            return

        res = {'path': os.path.relpath(abspath, self.config['root']),
               'message': message,
               'level': level,
               'lineno': line,
               'column': col,
               'rule': code,
               }

        results.append(result.from_config(self.config, **res))

    def run(self, *args, **kwargs):
        # protect against poor SIGINT handling. Handle it here instead
        # so we can kill the process without a cryptic traceback.
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandlerMixin.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def get_yamllint_binary():
    """
    Returns the path of the first yamllint binary available
    if not found returns None
    """
    binary = os.environ.get('YAMLLINT')
    if binary:
        return binary

    try:
        return which.which('yamllint')
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


def reinstall_yamllint():
    """
    Try to install yamllint at the target version, returns True on success
    otherwise prints the otuput of the pip command and returns False
    """
    if _run_pip('install', '-U',
                '--require-hashes', '-r',
                YAMLLINT_REQUIREMENTS_PATH):
        return True

    return False


def run_process(config, cmd):
    proc = YAMLLintProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()


def gen_yamllint_args(cmdargs, paths=None, conf_file=None):
    args = cmdargs[:]
    if isinstance(paths, basestring):
        paths = [paths]
    if conf_file and conf_file != 'default':
        return args + ['-c', conf_file] + paths
    return args + paths


def lint(files, config, **lintargs):
    if not reinstall_yamllint():
        print(YAMLLINT_INSTALL_ERROR)
        return 1

    binary = get_yamllint_binary()

    cmdargs = [
        binary,
        '-f', 'parsable'
    ]

    config = config.copy()
    config['root'] = lintargs['root']

    # Run any paths with a .yamllint file in the directory separately so
    # it gets picked up. This means only .yamllint files that live in
    # directories that are explicitly included will be considered.
    paths_by_config = defaultdict(list)
    for f in files:
        conf_files = get_ancestors_by_name('.yamllint', f, config['root'])
        paths_by_config[conf_files[0] if conf_files else 'default'].append(f)

    for conf_file, paths in paths_by_config.items():
        run_process(config, gen_yamllint_args(cmdargs, conf_file=conf_file, paths=paths))

    return results
