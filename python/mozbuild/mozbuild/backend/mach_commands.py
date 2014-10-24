# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import argparse
import glob
import logging
import mozpack.path
import os
import sys
import subprocess
import which

from mozbuild.backend.cpp_eclipse import CppEclipseBackend

from mozbuild.base import (
    MachCommandBase,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

@CommandProvider
class MachCommands(MachCommandBase):
    @Command('ide', category='devenv',
        description='Generate a project and launch an IDE.')
    @CommandArgument('ide', choices=['eclipse', 'visualstudio'])
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def eclipse(self, ide, args):
        if ide == 'eclipse':
            backend = 'CppEclipse'
        elif ide == 'visualstudio':
            backend = 'VisualStudio'

        if backend == 'CppEclipse':
            try:
                which.which('eclipse')
            except which.WhichError:
                print('Eclipse CDT 8.4 or later must be installed in your PATH.')
                print('Download: http://www.eclipse.org/cdt/downloads.php')
                return 1

        # Here we refresh the whole build. 'build export' is sufficient here and is probably more
        # correct but it's also nice having a single target to get a fully built and indexed
        # project (gives a easy target to use before go out to lunch).
        res = self._mach_context.commands.dispatch('build', self._mach_context)
        if res != 0:
            return 1

        # Generate or refresh the eclipse workspace
        python = self.virtualenv_manager.python_path
        config_status = os.path.join(self.topobjdir, 'config.status')
        args = [python, config_status, '--backend=%s' % backend]
        res = self._run_command_in_objdir(args=args, pass_thru=True, ensure_exit_code=False)
        if res != 0:
            return 1

        if backend == 'CppEclipse':
            eclipse_workspace_dir = self.get_eclipse_workspace_path()
            process = subprocess.check_call(['eclipse', '-data', eclipse_workspace_dir])
        elif backend == 'VisualStudio':
            visual_studio_workspace_dir = self.get_visualstudio_workspace_path()
            process = subprocess.check_call(['explorer.exe', visual_studio_workspace_dir])

    def get_eclipse_workspace_path(self):
        return CppEclipseBackend.get_workspace_path(self.topsrcdir, self.topobjdir)

    def get_visualstudio_workspace_path(self):
        return os.path.join(self.topobjdir, 'msvc', 'mozilla.sln')

