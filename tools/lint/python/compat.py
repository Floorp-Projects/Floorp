# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
from distutils.spawn import find_executable

import mozfile
import mozpack.path as mozpath
from mozpack.files import FileFinder
from mozprocess import ProcessHandlerMixin

from mozlint import result

here = os.path.abspath(os.path.dirname(__file__))

results = []


class PyCompatProcess(ProcessHandlerMixin):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs['processOutputLine'] = [self.process_line]
        ProcessHandlerMixin.__init__(self, *args, **kwargs)

    def process_line(self, line):
        try:
            res = json.loads(line)
        except ValueError:
            print('Non JSON output from {} linter: {}'.format(self.config['name'], line))
            return

        res['level'] = 'error'
        results.append(result.from_config(self.config, **res))


def setup(python):
    """Setup doesn't currently do any bootstrapping. For now, this function
    is only used to print the warning message.
    """
    binary = find_executable(python)
    if not binary:
        # TODO Bootstrap python2/python3 if not available
        print('warning: {} not detected, skipping py-compat check'.format(python))


def run_linter(python, paths, config, **lintargs):
    binary = find_executable(python)
    if not binary:
        # If we're in automation, this is fatal. Otherwise, the warning in the
        # setup method was already printed.
        if 'MOZ_AUTOMATION' in os.environ:
            return 1
        return []

    root = lintargs['root']
    pattern = "**/*.py"
    exclude = [mozpath.join(root, e) for e in config.get('exclude', [])]
    files = []
    for path in paths:
        path = mozpath.normsep(path)
        if os.path.isfile(path):
            files.append(path)
            continue

        ignore = [e[len(path):].lstrip('/') for e in exclude
                  if mozpath.commonprefix((path, e)) == path]
        finder = FileFinder(path, ignore=ignore)
        files.extend([os.path.join(path, p) for p, f in finder.find(pattern)])

    with mozfile.NamedTemporaryFile(mode='w') as fh:
        fh.write('\n'.join(files))
        fh.flush()

        cmd = [binary, os.path.join(here, 'check_compat.py'), fh.name]

        proc = PyCompatProcess(config, cmd)
        proc.run()
        try:
            proc.wait()
        except KeyboardInterrupt:
            proc.kill()

    return results


def setuppy2(root):
    return setup('python2')


def lintpy2(*args, **kwargs):
    return run_linter('python2', *args, **kwargs)


def setuppy3(root):
    return setup('python3')


def lintpy3(*args, **kwargs):
    return run_linter('python3', *args, **kwargs)
