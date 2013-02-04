# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import logging
import operator
import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase


BUILD_WHAT_HELP = '''
What to build. Can be a top-level make target or a relative directory. If
multiple options are provided, they will be built serially. BUILDING ONLY PARTS
OF THE TREE CAN RESULT IN BAD TREE STATE. USE AT YOUR OWN RISK.
'''.strip()


@CommandProvider
class Build(MachCommandBase):
    """Interface to build the tree."""

    @Command('build', help='Build the tree.')
    @CommandArgument('what', default=None, nargs='*', help=BUILD_WHAT_HELP)
    def build(self, what=None):
        # This code is only meant to be temporary until the more robust tree
        # building code in bug 780329 lands.
        from mozbuild.compilation.warnings import WarningsCollector
        from mozbuild.compilation.warnings import WarningsDatabase

        warnings_path = self._get_state_filename('warnings.json')
        warnings_database = WarningsDatabase()

        if os.path.exists(warnings_path):
            try:
                warnings_database.load_from_file(warnings_path)
            except ValueError:
                os.remove(warnings_path)

        warnings_collector = WarningsCollector(database=warnings_database,
            objdir=self.topobjdir)

        def on_line(line):
            try:
                warning = warnings_collector.process_line(line)
                if warning:
                    self.log(logging.INFO, 'compiler_warning', warning,
                        'Warning: {flag} in {filename}: {message}')
            except:
                # This will get logged in the more robust implementation.
                pass

            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        def resolve_target_to_make(target):
            if os.path.isabs(target):
                print('Absolute paths for make targets are not allowed.')
                return (None, None)

            target = target.replace(os.sep, '/')

            abs_target = os.path.join(self.topobjdir, target)

            # For directories, run |make -C dir|. If the directory does not
            # contain a Makefile, check parents until we find one. At worst,
            # this will terminate at the root.
            if os.path.isdir(abs_target):
                current = abs_target

                while True:
                    make_path = os.path.join(current, 'Makefile')
                    if os.path.exists(make_path):
                        return (current[len(self.topobjdir) + 1:], None)

                    current = os.path.dirname(current)

            # If it's not in a directory, this is probably a top-level make
            # target. Treat it as such.
            if '/' not in target:
                return (None, target)

            # We have a relative path within the tree. We look for a Makefile
            # as far into the path as possible. Then, we compute the make
            # target as relative to that directory.
            reldir = os.path.dirname(target)
            target = os.path.basename(target)

            while True:
                make_path = os.path.join(self.topobjdir, reldir, 'Makefile')

                if os.path.exists(make_path):
                    return (reldir, target)

                target = os.path.join(os.path.basename(reldir), target)
                reldir = os.path.dirname(reldir)

        # End of resolve_target_to_make.

        if what:
            top_make = os.path.join(self.topobjdir, 'Makefile')
            if not os.path.exists(top_make):
                print('Your tree has not been configured yet. Please run '
                    '|mach build| with no arguments.')
                return 1

            for target in what:
                make_dir, make_target = resolve_target_to_make(target)

                if make_dir is None and make_target is None:
                    return 1

                status = self._run_make(directory=make_dir, target=make_target,
                    line_handler=on_line, log=False, print_directory=False,
                    ensure_exit_code=False)

                if status != 0:
                    break
        else:
            status = self._run_make(srcdir=True, filename='client.mk',
                line_handler=on_line, log=False, print_directory=False,
                allow_parallel=False, ensure_exit_code=False)

            self.log(logging.WARNING, 'warning_summary',
                {'count': len(warnings_collector.database)},
                '{count} compiler warnings present.')

        warnings_database.prune()
        warnings_database.save_to_file(warnings_path)

        print('Finished building. Built files are in %s' % self.topobjdir)

        return status

    @Command('clobber', help='Clobber the tree (delete the object directory).')
    def clobber(self):
        self.remove_objdir()
        return 0


@CommandProvider
class Warnings(MachCommandBase):
    """Provide commands for inspecting warnings."""

    @property
    def database_path(self):
        return self._get_state_filename('warnings.json')

    @property
    def database(self):
        from mozbuild.compilation.warnings import WarningsDatabase

        path = self.database_path

        database = WarningsDatabase()

        if os.path.exists(path):
            database.load_from_file(path)

        return database

    @Command('warnings-summary',
        help='Show a summary of compiler warnings.')
    @CommandArgument('report', default=None, nargs='?',
        help='Warnings report to display. If not defined, show the most '
            'recent report.')
    def summary(self, report=None):
        database = self.database

        type_counts = database.type_counts
        sorted_counts = sorted(type_counts.iteritems(),
            key=operator.itemgetter(1))

        total = 0
        for k, v in sorted_counts:
            print('%d\t%s' % (v, k))
            total += v

        print('%d\tTotal' % total)

    @Command('warnings-list', help='Show a list of compiler warnings.')
    @CommandArgument('report', default=None, nargs='?',
        help='Warnings report to display. If not defined, show the most '
            'recent report.')
    def list(self, report=None):
        database = self.database

        by_name = sorted(database.warnings)

        for warning in by_name:
            filename = warning['filename']

            if filename.startswith(self.topsrcdir):
                filename = filename[len(self.topsrcdir) + 1:]

            if warning['column'] is not None:
                print('%s:%d:%d [%s] %s' % (filename, warning['line'],
                    warning['column'], warning['flag'], warning['message']))
            else:
                print('%s:%d [%s] %s' % (filename, warning['line'],
                    warning['flag'], warning['message']))

@CommandProvider
class ClangCommands(MachCommandBase):
    @Command('clang-complete', help='Generate a .clang_complete file.')
    def clang_complete(self):
        import shlex

        build_vars = {}

        def on_line(line):
            elements = [s.strip() for s in line.split('=', 1)]

            if len(elements) != 2:
                return

            build_vars[elements[0]] = elements[1]

        try:
            old_logger = self.log_manager.replace_terminal_handler(None)
            self._run_make(target='showbuild', log=False, line_handler=on_line)
        finally:
            self.log_manager.replace_terminal_handler(old_logger)

        def print_from_variable(name):
            if name not in build_vars:
                return

            value = build_vars[name]

            value = value.replace('-I.', '-I%s' % self.topobjdir)
            value = value.replace(' .', ' %s' % self.topobjdir)
            value = value.replace('-I..', '-I%s/..' % self.topobjdir)
            value = value.replace(' ..', ' %s/..' % self.topobjdir)

            args = shlex.split(value)
            for i in range(0, len(args) - 1):
                arg = args[i]

                if arg.startswith(('-I', '-D')):
                    print(arg)
                    continue

                if arg.startswith('-include'):
                    print(arg + ' ' + args[i + 1])
                    continue

        print_from_variable('COMPILE_CXXFLAGS')

        print('-I%s/ipc/chromium/src' % self.topsrcdir)
        print('-I%s/ipc/glue' % self.topsrcdir)
        print('-I%s/ipc/ipdl/_ipdlheaders' % self.topobjdir)


@CommandProvider
class Package(MachCommandBase):
    """Package the built product for distribution."""

    @Command('package', help='Package the built product for distribution as an APK, DMG, etc.')
    def package(self):
        return self._run_make(directory=".", target='package', ensure_exit_code=False)
