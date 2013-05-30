# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import logging
import operator
import os
import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mach.mixin.logging import LoggingMixin

from mozbuild.base import MachCommandBase


BUILD_WHAT_HELP = '''
What to build. Can be a top-level make target or a relative directory. If
multiple options are provided, they will be built serially. Takes dependency
information from `topsrcdir/build/dumbmake-dependencies` to build additional
targets as needed. BUILDING ONLY PARTS OF THE TREE CAN RESULT IN BAD TREE
STATE. USE AT YOUR OWN RISK.
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


class TerminalLoggingHandler(logging.Handler):
    """Custom logging handler that works with terminal window dressing.

    This class should probably live elsewhere, like the mach core. Consider
    this a proving ground for its usefulness.
    """
    def __init__(self):
        logging.Handler.__init__(self)

        self.fh = sys.stdout
        self.footer = None

    def flush(self):
        self.acquire()

        try:
            self.fh.flush()
        finally:
            self.release()

    def emit(self, record):
        msg = self.format(record)

        if self.footer:
            self.footer.clear()

        self.fh.write(msg)
        self.fh.write('\n')

        if self.footer:
            self.footer.draw()

        # If we don't flush, the footer may not get drawn.
        self.flush()


class BuildProgressFooter(object):
    """Handles display of a build progress indicator in a terminal.

    When mach builds inside a blessings-supported terminal, it will render
    progress information collected from a BuildMonitor. This class converts the
    state of BuildMonitor into terminal output.
    """

    def __init__(self, terminal, monitor):
        # terminal is a blessings.Terminal.
        self._t = terminal
        self._fh = sys.stdout
        self._monitor = monitor

    def _clear_lines(self, n):
        for i in range(n):
            self._fh.write(self._t.move_x(0))
            self._fh.write(self._t.clear_eol())
            self._fh.write(self._t.move_up())

        self._fh.write(self._t.move_down())
        self._fh.write(self._t.move_x(0))

    def clear(self):
        """Removes the footer from the current terminal."""
        self._clear_lines(1)

    def draw(self):
        """Draws this footer in the terminal."""
        if not self._monitor.tiers:
            return

        # The drawn terminal looks something like:
        # TIER: base nspr nss js platform app SUBTIER: static export libs tools DIRECTORIES: 06/09 (memory)

        # This is a list of 2-tuples of (encoding function, input). None means
        # no encoding. For a full reason on why we do things this way, read the
        # big comment below.
        parts = [('bold', 'TIER'), ':', ' ']

        current_encountered = False
        for tier in self._monitor.tiers:
            if tier == self._monitor.current_tier:
                parts.extend([('yellow', tier), ' '])
                current_encountered = True
            elif not current_encountered:
                parts.extend([('green', tier), ' '])
            else:
                parts.extend([tier, ' '])

        current_encountered = False
        parts.extend([('bold', 'SUBTIER'), ':', ' '])
        for subtier in self._monitor.subtiers:
            if subtier == self._monitor.current_subtier:
                parts.extend([('yellow', subtier), ' '])
                current_encountered = True
            elif not current_encountered:
                parts.extend([('green', subtier), ' '])
            else:
                parts.extend([subtier, ' '])

        if self._monitor.current_subtier_dirs and self._monitor.current_tier_dir:
            parts.extend([
                ('bold', 'DIRECTORIES'), ': ',
                '%02d' % self._monitor.current_tier_dir_index,
                '/',
                '%02d' % len(self._monitor.current_subtier_dirs),
                ' ',
                '(', ('magenta', self._monitor.current_tier_dir), ')',
            ])

        # We don't want to write more characters than the current width of the
        # terminal otherwise wrapping may result in weird behavior. We can't
        # simply truncate the line at terminal width characters because a)
        # non-viewable escape characters count towards the limit and b) we
        # don't want to truncate in the middle of an escape sequence because
        # subsequent output would inherit the escape sequence.
        max_width = self._t.width
        written = 0
        write_pieces = []
        for part in parts:
            if isinstance(part, tuple):
                func, arg = part

                if written + len(arg) > max_width:
                    write_pieces.append(arg[0:max_width - written])
                    written += len(arg)
                    break

                encoded = getattr(self._t, func)(arg)

                write_pieces.append(encoded)
                written += len(arg)
            else:
                if written + len(part) > max_width:
                    write_pieces.append(arg[0:max_width - written])
                    written += len(part)
                    break

                write_pieces.append(part)
                written += len(part)

        self._fh.write(''.join(write_pieces))
        self._fh.flush()


class BuildOutputManager(LoggingMixin):
    """Handles writing build output to a terminal, to logs, etc."""

    def __init__(self, log_manager, monitor):
        self.populate_logger()

        self.monitor = monitor
        self.footer = None

        terminal = log_manager.terminal

        # TODO convert terminal footer to config file setting.
        if not terminal or os.environ.get('MACH_NO_TERMINAL_FOOTER', None):
            return

        self.t = terminal
        self.footer = BuildProgressFooter(terminal, monitor)

        handler = TerminalLoggingHandler()
        handler.setFormatter(log_manager.terminal_formatter)
        handler.footer = self.footer

        old = log_manager.replace_terminal_handler(handler)
        handler.level = old.level

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.footer:
            self.footer.clear()

    def write_line(self, line):
        if self.footer:
            self.footer.clear()

        print(line)

        if self.footer:
            self.footer.draw()

    def refresh(self):
        if not self.footer:
            return

        self.footer.clear()
        self.footer.draw()

    def on_line(self, line):
        warning, state_changed, relevant = self.monitor.on_line(line)

        if warning:
            self.log(logging.INFO, 'compiler_warning', warning,
                'Warning: {flag} in {filename}: {message}')

        if relevant:
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')
        elif state_changed:
            self.refresh()


@CommandProvider
class Build(MachCommandBase):
    """Interface to build the tree."""

    @Command('build', category='build', description='Build the tree.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
        help='Number of concurrent jobs to run. Default is the number of CPUs.')
    @CommandArgument('what', default=None, nargs='*', help=BUILD_WHAT_HELP)
    @CommandArgument('-X', '--disable-extra-make-dependencies',
                     default=False, action='store_true',
                     help='Do not add extra make dependencies.')
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output for what commands the build is running.')
    def build(self, what=None, disable_extra_make_dependencies=None, jobs=0, verbose=False):
        from mozbuild.controller.building import BuildMonitor
        from mozbuild.util import resolve_target_to_make

        warnings_path = self._get_state_filename('warnings.json')
        monitor = BuildMonitor(self.topobjdir, warnings_path)

        with BuildOutputManager(self.log_manager, monitor) as output:
            monitor.start()

            if what:
                top_make = os.path.join(self.topobjdir, 'Makefile')
                if not os.path.exists(top_make):
                    print('Your tree has not been configured yet. Please run '
                        '|mach build| with no arguments.')
                    return 1

                # Collect target pairs.
                target_pairs = []
                for target in what:
                    path_arg = self._wrap_path_argument(target)

                    make_dir, make_target = resolve_target_to_make(self.topobjdir,
                        path_arg.relpath())

                    if make_dir is None and make_target is None:
                        return 1

                    target_pairs.append((make_dir, make_target))

                # Possibly add extra make depencies using dumbmake.
                if not disable_extra_make_dependencies:
                    from dumbmake.dumbmake import (dependency_map,
                                                   add_extra_dependencies)
                    depfile = os.path.join(self.topsrcdir, 'build',
                                           'dumbmake-dependencies')
                    with open(depfile) as f:
                        dm = dependency_map(f.readlines())
                    new_pairs = list(add_extra_dependencies(target_pairs, dm))
                    self.log(logging.DEBUG, 'dumbmake',
                             {'target_pairs': target_pairs,
                              'new_pairs': new_pairs},
                             'Added extra dependencies: will build {new_pairs} ' +
                             'instead of {target_pairs}.')
                    target_pairs = new_pairs

                # Build target pairs.
                for make_dir, make_target in target_pairs:
                    status = self._run_make(directory=make_dir, target=make_target,
                        line_handler=output.on_line, log=False, print_directory=False,
                        ensure_exit_code=False, num_jobs=jobs, silent=not verbose)

                    if status != 0:
                        break
            else:
                status = self._run_make(srcdir=True, filename='client.mk',
                    line_handler=output.on_line, log=False, print_directory=False,
                    allow_parallel=False, ensure_exit_code=False, num_jobs=jobs,
                    silent=not verbose)

                self.log(logging.WARNING, 'warning_summary',
                    {'count': len(monitor.warnings_database)},
                    '{count} compiler warnings present.')

            monitor.finish()

        high_finder, finder_percent = monitor.have_high_finder_usage()
        if high_finder:
            print(FINDER_SLOW_MESSAGE % finder_percent)

        long_build = monitor.elapsed > 600

        if status:
            return status

        if long_build:
            print('We know it took a while, but your build finally finished successfully!')
        else:
            print('Your build was successful!')

        # Only for full builds because incremental builders likely don't
        # need to be burdened with this.
        if not what:
            # Fennec doesn't have useful output from just building. We should
            # arguably make the build action useful for Fennec. Another day...
            if self.substs['MOZ_BUILD_APP'] != 'mobile/android':
                app_path = self.get_binary_path('app')
                print('To take your build for a test drive, run: %s' % app_path)
            app = self.substs['MOZ_BUILD_APP']
            if app in ('browser', 'mobile/android'):
                print('For more information on what to do now, see '
                    'https://developer.mozilla.org/docs/Developer_Guide/So_You_Just_Built_Firefox')

        return status

    @Command('configure', category='build',
        description='Configure the tree (run configure and config.status).')
    def configure(self):
        def on_line(line):
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        status = self._run_make(srcdir=True, filename='client.mk',
            target='configure', line_handler=on_line, log=False,
            print_directory=False, allow_parallel=False, ensure_exit_code=False)

        if not status:
            print('Configure complete!')
            print('Be sure to run |mach build| to pick up any changes');

        return status


    @Command('clobber', category='build',
        description='Clobber the tree (delete the object directory).')
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

    @Command('warnings-summary', category='post-build',
        description='Show a summary of compiler warnings.')
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

    @Command('warnings-list', category='post-build',
        description='Show a list of compiler warnings.')
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
    @Command('gtest', category='testing',
        description='Run GTest unit tests.')
    @CommandArgument('gtest_filter', default=b"*", nargs='?', metavar='gtest_filter',
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
    @Command('clang-complete', category='devenv',
        description='Generate a .clang_complete file.')
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

    @Command('package', category='post-build',
        description='Package the built product for distribution as an APK, DMG, etc.')
    def package(self):
        return self._run_make(directory=".", target='package', ensure_exit_code=False)

@CommandProvider
class Install(MachCommandBase):
    """Install a package."""

    @Command('install', category='post-build',
        description='Install the package on the machine, or on a device.')
    def install(self):
        return self._run_make(directory=".", target='install', ensure_exit_code=False)

@CommandProvider
class RunProgram(MachCommandBase):
    """Launch the compiled binary"""

    @Command('run', category='post-build', allow_all_args=True,
        description='Run the compiled program.')
    @CommandArgument('params', default=None, nargs='...',
        help='Command-line arguments to pass to the program.')
    @CommandArgument('+remote', '+r', action='store_true',
        help='Do not pass the -no-remote argument by default.')
    def run(self, params, remote):
        try:
            args = [self.get_binary_path('app')]
        except Exception as e:
            print("It looks like your program isn't built.",
                "You can run |mach build| to build it.")
            print(e)
            return 1
        if not remote:
            args.append('-no-remote')
        if params:
            args.extend(params)
        return self.run_process(args=args, ensure_exit_code=False,
            pass_thru=True)

@CommandProvider
class DebugProgram(MachCommandBase):
    """Debug the compiled binary"""

    @Command('debug', category='post-build', allow_all_args=True,
        description='Debug the compiled program.')
    @CommandArgument('params', default=None, nargs='...',
        help='Command-line arguments to pass to the program.')
    @CommandArgument('+remote', '+r', action='store_true',
        help='Do not pass the -no-remote argument by default')
    def debug(self, params, remote):
        import which
        try:
            debugger = which.which('gdb')
        except Exception as e:
            print("You don't have gdb in your PATH")
            print(e)
            return 1
        try:
            args = [debugger, '--args', self.get_binary_path('app')]
        except Exception as e:
            print("It looks like your program isn't built.",
                "You can run |mach build| to build it.")
            print(e)
            return 1
        if not remote:
            args.append('-no-remote')
        if params:
            args.extend(params)
        return self.run_process(args=args, ensure_exit_code=False,
            pass_thru=True)

@CommandProvider
class Buildsymbols(MachCommandBase):
    """Produce a package of debug symbols suitable for use with Breakpad."""

    @Command('buildsymbols', category='post-build',
        description='Produce a package of Breakpad-format symbols.')
    def buildsymbols(self):
        return self._run_make(directory=".", target='buildsymbols', ensure_exit_code=False)

@CommandProvider
class Makefiles(MachCommandBase):
    @Command('empty-makefiles', category='build-dev',
        description='Find empty Makefile.in in the tree.')
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

