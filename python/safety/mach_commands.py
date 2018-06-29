# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
import json

from mozbuild.base import (
    MachCommandBase,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

import mozpack.path as mozpath
from mozpack.files import FileFinder

from mozlog import commandline  # , get_default_logger

here = os.path.abspath(os.path.dirname(__file__))


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('python-safety', category='testing',
             description='Run python requirements safety checks')
    @CommandArgument('--python',
                     help='Version of Python for Pipenv to use. When given a '
                          'Python version, Pipenv will automatically scan your '
                          'system for a Python that matches that given version.')
    def python_safety(self, python=None, **kwargs):
        self.logger = commandline.setup_logging(
            "python-safety", {"raw": sys.stdout})

        python = python or self.virtualenv_manager.python_path
        self.activate_pipenv(pipfile=os.path.join(here, 'Pipfile'), args=[
                             '--python', python], populate=True)

        pattern = '**/*requirements*.txt'
        path = mozpath.normsep(os.path.dirname(os.path.dirname(here)))
        finder = FileFinder(path)
        files = [os.path.join(path, p) for p, f in finder.find(pattern)]

        return_code = 0

        self.logger.suite_start(tests=files)
        for filepath in files:
            self._run_python_safety(filepath)

        self.logger.suite_end()
        return return_code

    def _run_python_safety(self, test_path):
        from mozprocess import ProcessHandler

        output = []
        self.logger.test_start(test_path)

        def _line_handler(line):
            output.append(line)

        cmd = ['safety', 'check', '--cache', '--json', '-r', test_path]
        env = os.environ.copy()
        env[b'PYTHONDONTWRITEBYTECODE'] = b'1'

        proc = ProcessHandler(
            cmd, env=env, processOutputLine=_line_handler, storeOutput=False)
        proc.run()

        return_code = proc.wait()

        """Example output for an error in json.
        [
            "pycrypto",
            "<=2.6.1",
            "2.6",
            "Heap-based buffer overflow in the ALGnew...",
            "35015"
        ]
        """
        # Warnings are currently interleaved with json, see
        # https://github.com/pyupio/safety/issues/133
        for warning in output:
            if warning.startswith('Warning'):
                self.logger.warning(warning)
        output = [line for line in output if not line.startswith('Warning')]
        if output:
            output_json = json.loads("".join(output))
            affected = set()
            for entry in output_json:
                affected.add(entry[0])
                message = "{0} installed:{2} affected:{1} description:{3}\n".format(
                    *entry)
                self.logger.test_status(test=test_path,
                                        subtest=entry[0],
                                        status='FAIL',
                                        message=message
                                        )

        if return_code != 0:
            status = 'FAIL'
        else:
            status = 'PASS'
        self.logger.test_end(test_path, status=status,
                             expected='PASS', message=" ".join(affected))

        return return_code
