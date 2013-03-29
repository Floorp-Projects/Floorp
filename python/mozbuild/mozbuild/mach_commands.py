# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import logging
import operator
import os
import sys
import time

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

FINDER_SLOW_MESSAGE = '''
===================
PERFORMANCE WARNING

The OS X Finder application (file indexing used by Spotlight) used a lot of CPU
during the build - an average of %f%% (100%% is 1 core). This made your build
slower.

Consider adding ".noindex" to the end of your object directory name to have
Finder ignore it. Or, add an indexing exclusion through the Spotlight System
Preferences.
===================
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
        from mozbuild.util import resolve_target_to_make

        warnings_path = self._get_state_filename('warnings.json')
        warnings_database = WarningsDatabase()

        if os.path.exists(warnings_path):
            try:
                warnings_database.load_from_file(warnings_path)
            except ValueError:
                os.remove(warnings_path)

        warnings_collector = WarningsCollector(database=warnings_database,
            objdir=self.topobjdir)

        have_silliness = time.localtime()[1:3] == (4, 1)
        performed_silliness = []

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

            if have_silliness and not performed_silliness:
                import random
                if random.randint(0, 1000) != 42:
                    return

                performed_silliness.append(True)

                try:
                    import webbrowser
                    webbrowser.open_new_tab(
                        'https://www.youtube.com/watch?v=oHg5SJYRHA0')
                except Exception:
                    pass

        finder_start_cpu = self._get_finder_cpu_usage()
        time_start = time.time()

        if what:
            top_make = os.path.join(self.topobjdir, 'Makefile')
            if not os.path.exists(top_make):
                print('Your tree has not been configured yet. Please run '
                    '|mach build| with no arguments.')
                return 1

            for target in what:
                path_arg = self._wrap_path_argument(target)

                make_dir, make_target = resolve_target_to_make(self.topobjdir,
                    path_arg.relpath())

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

        time_end = time.time()
        time_elapsed = time_end - time_start
        self._handle_finder_cpu_usage(time_elapsed, finder_start_cpu)

        long_build = time_elapsed > 600

        if status:
            return status

        if long_build:
            print('We know it took a while, but your build finally finished successfully!')
        else:
            print('Your build was successful!')

        # Fennec doesn't have useful output from just building. We should
        # arguably make the build action useful for Fennec. Another day...
        if self.substs['MOZ_BUILD_APP'] != 'mobile/android':
            app_path = self.get_binary_path('app')
            print('To take your build for a test drive, run: %s' % app_path)

        # Only for full builds because incremental builders likely don't
        # need to be burdened with this.
        if not what:
            app = self.substs['MOZ_BUILD_APP']
            if app in ('browser', 'mobile/android'):
                print('For more information on what to do now, see '
                    'https://developer.mozilla.org/docs/Developer_Guide/So_You_Just_Built_Firefox')

        return status

    def _get_finder_cpu_usage(self):
        """Obtain the CPU usage of the Finder app on OS X.

        This is used to detect high CPU usage.
        """
        if not sys.platform.startswith('darwin'):
            return None

        try:
            import psutil
        except ImportError:
            return None

        for proc in psutil.process_iter():
            if proc.name != 'Finder':
                continue

            # Try to isolate system finder as opposed to other "Finder"
            # processes.
            if not proc.exe.endswith('CoreServices/Finder.app/Contents/MacOS/Finder'):
                continue

            return proc.get_cpu_times()

        return None

    def _handle_finder_cpu_usage(self, elapsed, start):
        if not start:
            return

        # We only measure if the measured range is sufficiently long.
        if elapsed < 15:
            return

        end = self._get_finder_cpu_usage()
        if not end:
            return

        start_total = start.user + start.system
        end_total = end.user + end.system

        cpu_seconds = end_total - start_total

        # If Finder used more than 25% of 1 core during the build, report an
        # error.
        finder_percent = cpu_seconds / elapsed * 100
        if finder_percent < 25:
            return

        print(FINDER_SLOW_MESSAGE % finder_percent)


    @Command('clobber', help='Clobber the tree (delete the object directory).')
    def clobber(self):
        try:
            self.remove_objdir()
            return 0
        except WindowsError as e:
            if e.winerror in (5, 32):
                self.log(logging.ERROR, 'file_access_error', {'error': e},
                    "Could not clobber because a file was in use. If the "
                    "application is running, try closing it. {error}")
                return 1
            else:
                raise


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
class GTestCommands(MachCommandBase):
    @Command('gtest', help='Run GTest unit tests.')
    @CommandArgument('gtest_filter', default='*', nargs='?', metavar='gtest_filter',
        help="test_filter is a ':'-separated list of wildcard patterns (called the positive patterns),"
             "optionally followed by a '-' and another ':'-separated pattern list (called the negative patterns).")
    @CommandArgument('--jobs', '-j', default='1', nargs='?', metavar='jobs', type=int,
        help='Run the tests in parallel using multiple processes.')
    @CommandArgument('--tbpl-parser', '-t', action='store_true',
        help='Output test results in a format that can be parsed by TBPL.')
    @CommandArgument('--shuffle', '-s', action='store_true',
        help='Randomize the execution order of tests.')
    def gtest(self, shuffle, jobs, gtest_filter, tbpl_parser):
        app_path = self.get_binary_path('app')

        # Use GTest environment variable to control test execution
        # For details see:
        # https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_Test_Programs:_Advanced_Options
        gtest_env = {b'GTEST_FILTER': gtest_filter}

        if shuffle:
            gtest_env[b"GTEST_SHUFFLE"] = b"True"

        if tbpl_parser:
            gtest_env[b"MOZ_TBPL_PARSER"] = b"True"

        if jobs == 1:
            return self.run_process([app_path, "-unittest"],
                                    append_env=gtest_env,
                                    ensure_exit_code=False,
                                    pass_thru=True)

        from mozprocess import ProcessHandlerMixin
        import functools
        def handle_line(job_id, line):
            # Prepend the jobId
            line = '[%d] %s' % (job_id + 1, line.strip())
            self.log(logging.INFO, "GTest", {'line': line}, '{line}')

        gtest_env["GTEST_TOTAL_SHARDS"] = str(jobs)
        processes = {}
        for i in range(0, jobs):
            gtest_env["GTEST_SHARD_INDEX"] = str(i)
            processes[i] = ProcessHandlerMixin([app_path, "-unittest"],
                             env=gtest_env,
                             processOutputLine=[functools.partial(handle_line, i)],
                             universal_newlines=True)
            processes[i].run()

        exit_code = 0
        for process in processes.values():
            status = process.wait()
            if status:
                exit_code = status

        # Clamp error code to 255 to prevent overflowing multiple of
        # 256 into 0
        if exit_code > 255:
            exit_code = 255

        return exit_code

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

@CommandProvider
class Install(MachCommandBase):
    """Install a package."""

    @Command('install', help='Install the package on the machine, or on a device.')
    def install(self):
        return self._run_make(directory=".", target='install', ensure_exit_code=False)

@CommandProvider
class RunProgram(MachCommandBase):
    """Launch the compiled binary"""

    @Command('run', help='Run the compiled program.', prefix_chars='+')
    @CommandArgument('params', default=None, nargs='*',
        help='Command-line arguments to pass to the program.')
    def run(self, params):
        try:
            args = [self.get_binary_path('app')]
        except Exception as e:
            print("It looks like your program isn't built.",
                "You can run |mach build| to build it.")
            print(e)
            return 1
        if params:
            args.extend(params)
        return self.run_process(args=args, ensure_exit_code=False,
            pass_thru=True)

@CommandProvider
class Buildsymbols(MachCommandBase):
    """Produce a package of debug symbols suitable for use with Breakpad."""

    @Command('buildsymbols', help='Produce a package of Breakpad-format symbols.')
    def buildsymbols(self):
        return self._run_make(directory=".", target='buildsymbols', ensure_exit_code=False)

@CommandProvider
class Makefiles(MachCommandBase):
    @Command('empty-makefiles', help='Find empty Makefile.in in the tree.')
    def empty(self):
        import pymake.parser
        import pymake.parserdata

        IGNORE_VARIABLES = {
            'DEPTH': ('@DEPTH@',),
            'topsrcdir': ('@top_srcdir@',),
            'srcdir': ('@srcdir@',),
            'relativesrcdir': ('@relativesrcdir@',),
            'VPATH': ('@srcdir@',),
        }

        IGNORE_INCLUDES = [
            'include $(DEPTH)/config/autoconf.mk',
            'include $(topsrcdir)/config/config.mk',
            'include $(topsrcdir)/config/rules.mk',
        ]

        def is_statement_relevant(s):
            if isinstance(s, pymake.parserdata.SetVariable):
                exp = s.vnameexp
                if not exp.is_static_string:
                    return True

                if exp.s not in IGNORE_VARIABLES:
                    return True

                return s.value not in IGNORE_VARIABLES[exp.s]

            if isinstance(s, pymake.parserdata.Include):
                if s.to_source() in IGNORE_INCLUDES:
                    return False

            return True

        for path in self._makefile_ins():
            statements = [s for s in pymake.parser.parsefile(path)
                if is_statement_relevant(s)]

            if not statements:
                print(os.path.relpath(path, self.topsrcdir))

    def _makefile_ins(self):
        for root, dirs, files in os.walk(self.topsrcdir):
            for f in files:
                if f == 'Makefile.in':
                    yield os.path.join(root, f)

