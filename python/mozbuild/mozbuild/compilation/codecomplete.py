# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This modules provides functionality for dealing with code completion.

import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase

@CommandProvider
class Introspection(MachCommandBase):
    """Instropection commands."""

    @Command('compileflags', category='devenv',
        description='Display the compilation flags for a given source file')
    @CommandArgument('what', default=None,
        help='Source file to display compilation flags for')
    def compileflags(self, what):
        from mozbuild.util import resolve_target_to_make
        import shlex

        top_make = os.path.join(self.topobjdir, 'Makefile')
        if not os.path.exists(top_make):
            print('Your tree has not been built yet. Please run '
                '|mach build| with no arguments.')
            return 1

        path_arg = self._wrap_path_argument(what)

        make_dir, make_target = resolve_target_to_make(self.topobjdir,
            path_arg.relpath())

        if make_dir is None and make_target is None:
            return 1

        build_vars = {}

        def on_line(line):
            elements = [s.strip() for s in line.split('=', 1)]

            if len(elements) != 2:
                return

            build_vars[elements[0]] = elements[1]

        try:
            old_logger = self.log_manager.replace_terminal_handler(None)
            self._run_make(directory=make_dir, target='showbuild', log=False,
                    print_directory=False, allow_parallel=False, silent=True,
                    line_handler=on_line)
        finally:
            self.log_manager.replace_terminal_handler(old_logger)

        if what.endswith('.c'):
            name = 'COMPILE_CFLAGS'
        else:
            name = 'COMPILE_CXXFLAGS'

        if name not in build_vars:
            return

        flags = ['-isystem', '-I', '-include', '-MF']
        new_args = []
        path = os.path.join(self.topobjdir, make_dir)
        for arg in shlex.split(build_vars[name]):
            if new_args and new_args[-1] in flags:
                arg = os.path.normpath(os.path.join(path, arg))
            else:
                flag = [(f, arg[len(f):]) for f in flags + ['--sysroot=']
                        if arg.startswith(f)]
                if flag:
                    flag, val = flag[0]
                    if val:
                        arg = flag + os.path.normpath(os.path.join(path, val))
            new_args.append(arg)

        print(' '.join(new_args))

