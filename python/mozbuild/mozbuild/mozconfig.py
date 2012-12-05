# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import subprocess

import pymake.parser
from pymake.data import Makefile

from mach.mixin.process import ProcessExecutionMixin


class MozconfigLoader(ProcessExecutionMixin):
    """Handles loading and parsing of mozconfig files."""

    def __init__(self, topsrcdir):
        self.topsrcdir = topsrcdir

    @property
    def _client_mk_loader_path(self):
        return os.path.join(self.topsrcdir, 'build', 'autoconf',
            'mozconfig2client-mk')

    def read_mozconfig(self, path=None):
        env = dict(os.environ)
        if path is not None:
            # Python < 2.7.3 doesn't like unicode strings in env keys.
            env[b'MOZCONFIG'] = path

        args = self._normalize_command([self._client_mk_loader_path,
            self.topsrcdir], True)

        output = subprocess.check_output(args, stderr=subprocess.PIPE,
            cwd=self.topsrcdir, env=env)

        # The output is make syntax. We parse this in a specialized make
        # context.
        statements = pymake.parser.parsestring(output, 'mozconfig')

        makefile = Makefile(workdir=self.topsrcdir, env={
            'TOPSRCDIR': self.topsrcdir})

        statements.execute(makefile)

        result = {
            'topobjdir': None,
        }

        def get_value(name):
            exp = makefile.variables.get(name)[2]

            return exp.resolvestr(makefile, makefile.variables)

        for name, flavor, source, value in makefile.variables:
            # We only care about variables that came from the parsed mozconfig.
            if source != pymake.data.Variables.SOURCE_MAKEFILE:
                continue

            # Ignore some pymake built-ins.
            if name in ('.PYMAKE', 'MAKELEVEL', 'MAKEFLAGS'):
                continue

            if name == 'MOZ_OBJDIR':
                result['topobjdir'] = get_value(name)

            # If we want to extract other variables defined by mozconfig, here
            # is where we'd do it.

        return result
