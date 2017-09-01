# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import tempfile
from distutils.spawn import find_executable

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
            print('Non JSON output from linter, will not be processed: {}'.format(line))
            return

        res['level'] = 'error'
        results.append(result.from_config(self.config, **res))


def run_linter(python, paths, config, **lintargs):
    binary = find_executable(python)
    if not binary:
        # TODO bootstrap python3 if not available
        print('error: {} not detected, aborting py-compat check'.format(python))
        if 'MOZ_AUTOMATION' in os.environ:
            return 1
        return []

    pattern = "**/*.py"
    exclude = lintargs.get('exclude', [])
    files = []
    for path in paths:
        if os.path.isfile(path):
            files.append(path)
            continue

        finder = FileFinder(path, ignore=exclude)
        files.extend([os.path.join(path, p) for p, f in finder.find(pattern)])

    with tempfile.NamedTemporaryFile(mode='w') as fh:
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


def lintpy2(*args, **kwargs):
    return run_linter('python2', *args, **kwargs)


def lintpy3(*args, **kwargs):
    return run_linter('python3', *args, **kwargs)
