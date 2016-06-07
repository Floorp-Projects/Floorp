# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import errno
import itertools
import json
import logging
import operator
import os
import subprocess
import sys

import mozpack.path as mozpath

from mach.decorators import (
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    Command,
    SubCommand,
)

from mach.mixin.logging import LoggingMixin

from mozbuild.base import (
    BuildEnvironmentNotFoundException,
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
    MozconfigFindException,
    MozconfigLoadException,
    ObjdirMismatchException,
)

from mozpack.manifests import (
    InstallManifest,
)

from mozbuild.backend import backends
from mozbuild.shellutil import quote as shell_quote


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

EXCESSIVE_SWAP_MESSAGE = '''
===================
PERFORMANCE WARNING

Your machine experienced a lot of swap activity during the build. This is
possibly a sign that your machine doesn't have enough physical memory or
not enough available memory to perform the build. It's also possible some
other system activity during the build is to blame.

If you feel this message is not appropriate for your machine configuration,
please file a Core :: Build Config bug at
https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Build%20Config
and tell us about your machine and build configuration so we can adjust the
warning heuristic.
===================
'''


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

        self.acquire()

        try:
            if self.footer:
                self.footer.clear()

            self.fh.write(msg)
            self.fh.write('\n')

            if self.footer:
                self.footer.draw()

            # If we don't flush, the footer may not get drawn.
            self.fh.flush()
        finally:
            self.release()


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
        self.tiers = monitor.tiers.tier_status.viewitems()

    def clear(self):
        """Removes the footer from the current terminal."""
        self._fh.write(self._t.move_x(0))
        self._fh.write(self._t.clear_eos())

    def draw(self):
        """Draws this footer in the terminal."""

        if not self.tiers:
            return

        # The drawn terminal looks something like:
        # TIER: base nspr nss js platform app SUBTIER: static export libs tools DIRECTORIES: 06/09 (memory)

        # This is a list of 2-tuples of (encoding function, input). None means
        # no encoding. For a full reason on why we do things this way, read the
        # big comment below.
        parts = [('bold', 'TIER:')]
        append = parts.append
        for tier, status in self.tiers:
            if status is None:
                append(tier)
            elif status == 'finished':
                append(('green', tier))
            else:
                append(('underline_yellow', tier))

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
            try:
                func, part = part
                encoded = getattr(self._t, func)(part)
            except ValueError:
                encoded = part

            len_part = len(part)
            len_spaces = len(write_pieces)
            if written + len_part + len_spaces > max_width:
                write_pieces.append(part[0:max_width - written - len_spaces])
                written += len_part
                break

            write_pieces.append(encoded)
            written += len_part

        with self._t.location():
            self._t.move(self._t.height-1,0)
            self._fh.write(' '.join(write_pieces))


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

        self._handler = TerminalLoggingHandler()
        self._handler.setFormatter(log_manager.terminal_formatter)
        self._handler.footer = self.footer

        old = log_manager.replace_terminal_handler(self._handler)
        self._handler.level = old.level

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.footer:
            self.footer.clear()
            # Prevents the footer from being redrawn if logging occurs.
            self._handler.footer = None

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
            have_handler = hasattr(self, 'handler')
            if have_handler:
                self.handler.acquire()
            try:
                self.refresh()
            finally:
                if have_handler:
                    self.handler.release()


@CommandProvider
class Build(MachCommandBase):
    """Interface to build the tree."""

    @Command('build', category='build', description='Build the tree.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
        help='Number of concurrent jobs to run. Default is the number of CPUs.')
    @CommandArgument('-C', '--directory', default=None,
        help='Change to a subdirectory of the build directory first.')
    @CommandArgument('what', default=None, nargs='*', help=BUILD_WHAT_HELP)
    @CommandArgument('-X', '--disable-extra-make-dependencies',
                     default=False, action='store_true',
                     help='Do not add extra make dependencies.')
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output for what commands the build is running.')
    def build(self, what=None, disable_extra_make_dependencies=None, jobs=0,
        directory=None, verbose=False):
        """Build the source tree.

        With no arguments, this will perform a full build.

        Positional arguments define targets to build. These can be make targets
        or patterns like "<dir>/<target>" to indicate a make target within a
        directory.

        There are a few special targets that can be used to perform a partial
        build faster than what `mach build` would perform:

        * binaries - compiles and links all C/C++ sources and produces shared
          libraries and executables (binaries).

        * faster - builds JavaScript, XUL, CSS, etc files.

        "binaries" and "faster" almost fully complement each other. However,
        there are build actions not captured by either. If things don't appear to
        be rebuilding, perform a vanilla `mach build` to rebuild the world.
        """
        import which
        from mozbuild.controller.building import BuildMonitor
        from mozbuild.util import (
            mkdir,
            resolve_target_to_make,
        )

        self.log_manager.register_structured_logger(logging.getLogger('mozbuild'))

        warnings_path = self._get_state_filename('warnings.json')
        monitor = self._spawn(BuildMonitor)
        monitor.init(warnings_path)
        ccache_start = monitor.ccache_stats()

        # Disable indexing in objdir because it is not necessary and can slow
        # down builds.
        mkdir(self.topobjdir, not_indexed=True)

        with BuildOutputManager(self.log_manager, monitor) as output:
            monitor.start()

            if directory is not None and not what:
                print('Can only use -C/--directory with an explicit target '
                    'name.')
                return 1

            if directory is not None:
                disable_extra_make_dependencies=True
                directory = mozpath.normsep(directory)
                if directory.startswith('/'):
                    directory = directory[1:]

            status = None
            monitor.start_resource_recording()
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

                    if directory is not None:
                        make_dir = os.path.join(self.topobjdir, directory)
                        make_target = target
                    else:
                        make_dir, make_target = \
                            resolve_target_to_make(self.topobjdir,
                                path_arg.relpath())

                    if make_dir is None and make_target is None:
                        return 1

                    # See bug 886162 - we don't want to "accidentally" build
                    # the entire tree (if that's really the intent, it's
                    # unlikely they would have specified a directory.)
                    if not make_dir and not make_target:
                        print("The specified directory doesn't contain a "
                              "Makefile and the first parent with one is the "
                              "root of the tree. Please specify a directory "
                              "with a Makefile or run |mach build| if you "
                              "want to build the entire tree.")
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

                # Ensure build backend is up to date. The alternative is to
                # have rules in the invoked Makefile to rebuild the build
                # backend. But that involves make reinvoking itself and there
                # are undesired side-effects of this. See bug 877308 for a
                # comprehensive history lesson.
                self._run_make(directory=self.topobjdir, target='backend',
                    line_handler=output.on_line, log=False,
                    print_directory=False)

                if self.substs.get('MOZ_ARTIFACT_BUILDS', False):
                    self._run_mach_artifact_install()

                # Build target pairs.
                for make_dir, make_target in target_pairs:
                    # We don't display build status messages during partial
                    # tree builds because they aren't reliable there. This
                    # could potentially be fixed if the build monitor were more
                    # intelligent about encountering undefined state.
                    status = self._run_make(directory=make_dir, target=make_target,
                        line_handler=output.on_line, log=False, print_directory=False,
                        ensure_exit_code=False, num_jobs=jobs, silent=not verbose,
                        append_env={b'NO_BUILDSTATUS_MESSAGES': b'1'})

                    if status != 0:
                        break
            else:
                try:
                    if self.substs.get('MOZ_ARTIFACT_BUILDS', False):
                        self._run_mach_artifact_install()
                except BuildEnvironmentNotFoundException:
                    # Can't read self.substs from config.status?  That means we
                    # need to run configure.  The client.mk invocation below
                    # will configure, which will run config.status, which will
                    # invoke |mach artifact install| itself before continuing
                    # the build.  Therefore, we needn't install artifacts
                    # ourselves.
                    self.log(logging.DEBUG, 'artifact',
                             {}, "Not running |mach artifact install| -- it will be run by client.mk.")

                status = self._run_make(srcdir=True, filename='client.mk',
                    line_handler=output.on_line, log=False, print_directory=False,
                    allow_parallel=False, ensure_exit_code=False, num_jobs=jobs,
                    silent=not verbose)

                self.log(logging.WARNING, 'warning_summary',
                    {'count': len(monitor.warnings_database)},
                    '{count} compiler warnings present.')

            monitor.finish(record_usage=status==0)

        high_finder, finder_percent = monitor.have_high_finder_usage()
        if high_finder:
            print(FINDER_SLOW_MESSAGE % finder_percent)

        ccache_end = monitor.ccache_stats()

        ccache_diff = None
        if ccache_start and ccache_end:
            ccache_diff = ccache_end - ccache_start
            if ccache_diff:
                self.log(logging.INFO, 'ccache',
                         {'msg': ccache_diff.hit_rate_message()}, "{msg}")

        notify_minimum_time = 300
        try:
            notify_minimum_time = int(os.environ.get('MACH_NOTIFY_MINTIME', '300'))
        except ValueError:
            # Just stick with the default
            pass

        if monitor.elapsed > notify_minimum_time:
            # Display a notification when the build completes.
            self.notify('Build complete' if not status else 'Build failed')

        if status:
            return status

        long_build = monitor.elapsed > 600

        if long_build:
            output.on_line('We know it took a while, but your build finally finished successfully!')
        else:
            output.on_line('Your build was successful!')

        if monitor.have_resource_usage:
            excessive, swap_in, swap_out = monitor.have_excessive_swapping()
            # if excessive:
            #    print(EXCESSIVE_SWAP_MESSAGE)

            print('To view resource usage of the build, run |mach '
                'resource-usage|.')

            telemetry_handler = getattr(self._mach_context,
                                        'telemetry_handler', None)
            telemetry_data = monitor.get_resource_usage()

            # Record build configuration data. For now, we cherry pick
            # items we need rather than grabbing everything, in order
            # to avoid accidentally disclosing PII.
            telemetry_data['substs'] = {}
            try:
                for key in ['MOZ_ARTIFACT_BUILDS', 'MOZ_USING_CCACHE']:
                    value = self.substs.get(key, False)
                    telemetry_data['substs'][key] = value
            except BuildEnvironmentNotFoundException:
                pass

            # Grab ccache stats if available. We need to be careful not
            # to capture information that can potentially identify the
            # user (such as the cache location)
            if ccache_diff:
                telemetry_data['ccache'] = {}
                for key in [key[0] for key in ccache_diff.STATS_KEYS]:
                    try:
                        telemetry_data['ccache'][key] = ccache_diff._values[key]
                    except KeyError:
                        pass

            telemetry_handler(self._mach_context, telemetry_data)

        # Only for full builds because incremental builders likely don't
        # need to be burdened with this.
        if not what:
            try:
                # Fennec doesn't have useful output from just building. We should
                # arguably make the build action useful for Fennec. Another day...
                if self.substs['MOZ_BUILD_APP'] != 'mobile/android':
                    print('To take your build for a test drive, run: |mach run|')
                app = self.substs['MOZ_BUILD_APP']
                if app in ('browser', 'mobile/android'):
                    print('For more information on what to do now, see '
                        'https://developer.mozilla.org/docs/Developer_Guide/So_You_Just_Built_Firefox')
            except Exception:
                # Ignore Exceptions in case we can't find config.status (such
                # as when doing OSX Universal builds)
                pass

        return status

    @Command('configure', category='build',
        description='Configure the tree (run configure and config.status).')
    @CommandArgument('options', default=None, nargs=argparse.REMAINDER,
                     help='Configure options')
    def configure(self, options=None):
        def on_line(line):
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        options = ' '.join(shell_quote(o) for o in options or ())
        status = self._run_make(srcdir=True, filename='client.mk',
            target='configure', line_handler=on_line, log=False,
            print_directory=False, allow_parallel=False, ensure_exit_code=False,
            append_env={b'CONFIGURE_ARGS': options.encode('utf-8'),
                        b'NO_BUILDSTATUS_MESSAGES': b'1',})

        if not status:
            print('Configure complete!')
            print('Be sure to run |mach build| to pick up any changes');

        return status

    @Command('resource-usage', category='post-build',
        description='Show information about system resource usage for a build.')
    @CommandArgument('--address', default='localhost',
        help='Address the HTTP server should listen on.')
    @CommandArgument('--port', type=int, default=0,
        help='Port number the HTTP server should listen on.')
    @CommandArgument('--browser', default='firefox',
        help='Web browser to automatically open. See webbrowser Python module.')
    @CommandArgument('--url',
        help='URL of JSON document to display')
    def resource_usage(self, address=None, port=None, browser=None, url=None):
        import webbrowser
        from mozbuild.html_build_viewer import BuildViewerServer

        server = BuildViewerServer(address, port)

        if url:
            server.add_resource_json_url('url', url)
        else:
            last = self._get_state_filename('build_resources.json')
            if not os.path.exists(last):
                print('Build resources not available. If you have performed a '
                    'build and receive this message, the psutil Python package '
                    'likely failed to initialize properly.')
                return 1

            server.add_resource_json_file('last', last)
        try:
            webbrowser.get(browser).open_new_tab(server.url)
        except Exception:
            print('Cannot get browser specified, trying the default instead.')
            try:
                browser = webbrowser.get().open_new_tab(server.url)
            except Exception:
                print('Please open %s in a browser.' % server.url)

        print('Hit CTRL+c to stop server.')
        server.run()

    CLOBBER_CHOICES = ['objdir', 'python']
    @Command('clobber', category='build',
        description='Clobber the tree (delete the object directory).')
    @CommandArgument('what', default=['objdir'], nargs='*',
        help='Target to clobber, must be one of {{{}}} (default objdir).'.format(
             ', '.join(CLOBBER_CHOICES)))
    def clobber(self, what):
        invalid = set(what) - set(self.CLOBBER_CHOICES)
        if invalid:
            print('Unknown clobber target(s): {}'.format(', '.join(invalid)))
            return 1

        ret = 0
        if 'objdir' in what:
            try:
                self.remove_objdir()
            except OSError as e:
                if sys.platform.startswith('win'):
                    if isinstance(e, WindowsError) and e.winerror in (5,32):
                        self.log(logging.ERROR, 'file_access_error', {'error': e},
                            "Could not clobber because a file was in use. If the "
                            "application is running, try closing it. {error}")
                        return 1
                raise

        if 'python' in what:
            if os.path.isdir(mozpath.join(self.topsrcdir, '.hg')):
                cmd = ['hg', 'purge', '--all', '-I', 'glob:**.py[co]']
            elif os.path.isdir(mozpath.join(self.topsrcdir, '.git')):
                cmd = ['git', 'clean', '-f', '-x', '*.py[co]']
            else:
                cmd = ['find', '.', '-type', 'f', '-name', '*.py[co]', '-delete']
            ret = subprocess.call(cmd, cwd=self.topsrcdir)
        return ret

    @Command('build-backend', category='build',
        description='Generate a backend used to build the tree.')
    @CommandArgument('-d', '--diff', action='store_true',
        help='Show a diff of changes.')
    # It would be nice to filter the choices below based on
    # conditions, but that is for another day.
    @CommandArgument('-b', '--backend', nargs='+', choices=sorted(backends),
        help='Which backend to build.')
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output.')
    @CommandArgument('-n', '--dry-run', action='store_true',
        help='Do everything except writing files out.')
    def build_backend(self, backend, diff=False, verbose=False, dry_run=False):
        python = self.virtualenv_manager.python_path
        config_status = os.path.join(self.topobjdir, 'config.status')

        if not os.path.exists(config_status):
            print('config.status not found.  Please run |mach configure| '
                  'or |mach build| prior to building the %s build backend.'
                  % backend)
            return 1

        args = [python, config_status]
        if backend:
            args.append('--backend')
            args.extend(backend)
        if diff:
            args.append('--diff')
        if verbose:
            args.append('--verbose')
        if dry_run:
            args.append('--dry-run')

        return self._run_command_in_objdir(args=args, pass_thru=True,
            ensure_exit_code=False)

    def _run_mach_artifact_install(self):
        # We'd like to launch artifact using
        # self._mach_context.commands.dispatch.  However, artifact activates
        # the virtualenv, which plays badly with the rest of this code.
        # Therefore, we run |mach artifact install| in a new process (and
        # throw an exception if it fails).
        self.log(logging.INFO, 'artifact',
                 {}, "Running |mach artifact install|.")
        args = [os.path.join(self.topsrcdir, 'mach'), 'artifact', 'install']
        self._run_command_in_srcdir(args=args, require_unix_environment=True,
            pass_thru=True, ensure_exit_code=True)

@CommandProvider
class Doctor(MachCommandBase):
    """Provide commands for diagnosing common build environment problems"""
    @Command('doctor', category='devenv',
        description='')
    @CommandArgument('--fix', default=None, action='store_true',
        help='Attempt to fix found problems.')
    def doctor(self, fix=None):
        self._activate_virtualenv()
        from mozbuild.doctor import Doctor
        doctor = Doctor(self.topsrcdir, self.topobjdir, fix)
        return doctor.check_all()

@CommandProvider
class Logs(MachCommandBase):
    """Provide commands to read mach logs."""
    NO_AUTO_LOG = True

    @Command('show-log', category='post-build',
        description='Display mach logs')
    @CommandArgument('log_file', nargs='?', type=argparse.FileType('rb'),
        help='Filename to read log data from. Defaults to the log of the last '
             'mach command.')
    def show_log(self, log_file=None):
        if not log_file:
            path = self._get_state_filename('last_log.json')
            log_file = open(path, 'rb')

        if self.log_manager.terminal:
            env = dict(os.environ)
            if 'LESS' not in env:
                # Sensible default flags if none have been set in the user
                # environment.
                env['LESS'] = 'FRX'
            less = subprocess.Popen(['less'], stdin=subprocess.PIPE, env=env)
            # Various objects already have a reference to sys.stdout, so we
            # can't just change it, we need to change the file descriptor under
            # it to redirect to less's input.
            # First keep a copy of the sys.stdout file descriptor.
            output_fd = os.dup(sys.stdout.fileno())
            os.dup2(less.stdin.fileno(), sys.stdout.fileno())

        startTime = 0
        for line in log_file:
            created, action, params = json.loads(line)
            if not startTime:
                startTime = created
                self.log_manager.terminal_handler.formatter.start_time = \
                    created
            if 'line' in params:
                record = logging.makeLogRecord({
                    'created': created,
                    'name': self._logger.name,
                    'levelno': logging.INFO,
                    'msg': '{line}',
                    'params': params,
                    'action': action,
                })
                self._logger.handle(record)

        if self.log_manager.terminal:
            # Close less's input so that it knows that we're done sending data.
            less.stdin.close()
            # Since the less's input file descriptor is now also the stdout
            # file descriptor, we still actually have a non-closed system file
            # descriptor for less's input. Replacing sys.stdout's file
            # descriptor with what it was before we replaced it will properly
            # close less's input.
            os.dup2(output_fd, sys.stdout.fileno())
            less.wait()


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
    @CommandArgument('-C', '--directory', default=None,
        help='Change to a subdirectory of the build directory first.')
    @CommandArgument('report', default=None, nargs='?',
        help='Warnings report to display. If not defined, show the most '
            'recent report.')
    def summary(self, directory=None, report=None):
        database = self.database

        if directory:
            dirpath = self.join_ensure_dir(self.topsrcdir, directory)
            if not dirpath:
                return 1
        else:
            dirpath = None

        type_counts = database.type_counts(dirpath)
        sorted_counts = sorted(type_counts.iteritems(),
            key=operator.itemgetter(1))

        total = 0
        for k, v in sorted_counts:
            print('%d\t%s' % (v, k))
            total += v

        print('%d\tTotal' % total)

    @Command('warnings-list', category='post-build',
        description='Show a list of compiler warnings.')
    @CommandArgument('-C', '--directory', default=None,
        help='Change to a subdirectory of the build directory first.')
    @CommandArgument('--flags', default=None, nargs='+',
        help='Which warnings flags to match.')
    @CommandArgument('report', default=None, nargs='?',
        help='Warnings report to display. If not defined, show the most '
            'recent report.')
    def list(self, directory=None, flags=None, report=None):
        database = self.database

        by_name = sorted(database.warnings)

        topsrcdir = mozpath.normpath(self.topsrcdir)

        if directory:
            directory = mozpath.normsep(directory)
            dirpath = self.join_ensure_dir(topsrcdir, directory)
            if not dirpath:
                return 1

        if flags:
            # Flatten lists of flags.
            flags = set(itertools.chain(*[flaglist.split(',') for flaglist in flags]))

        for warning in by_name:
            filename = mozpath.normsep(warning['filename'])

            if filename.startswith(topsrcdir):
                filename = filename[len(topsrcdir) + 1:]

            if directory and not filename.startswith(directory):
                continue

            if flags and warning['flag'] not in flags:
                continue

            if warning['column'] is not None:
                print('%s:%d:%d [%s] %s' % (filename, warning['line'],
                    warning['column'], warning['flag'], warning['message']))
            else:
                print('%s:%d [%s] %s' % (filename, warning['line'],
                    warning['flag'], warning['message']))

    def join_ensure_dir(self, dir1, dir2):
        dir1 = mozpath.normpath(dir1)
        dir2 = mozpath.normsep(dir2)
        joined_path = mozpath.join(dir1, dir2)
        if os.path.isdir(joined_path):
            return joined_path
        else:
            print('Specified directory not found.')
            return None

@CommandProvider
class GTestCommands(MachCommandBase):
    @Command('gtest', category='testing',
        description='Run GTest unit tests (C++ tests).')
    @CommandArgument('gtest_filter', default=b"*", nargs='?', metavar='gtest_filter',
        help="test_filter is a ':'-separated list of wildcard patterns (called the positive patterns),"
             "optionally followed by a '-' and another ':'-separated pattern list (called the negative patterns).")
    @CommandArgument('--jobs', '-j', default='1', nargs='?', metavar='jobs', type=int,
        help='Run the tests in parallel using multiple processes.')
    @CommandArgument('--tbpl-parser', '-t', action='store_true',
        help='Output test results in a format that can be parsed by TBPL.')
    @CommandArgument('--shuffle', '-s', action='store_true',
        help='Randomize the execution order of tests.')

    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')

    def gtest(self, shuffle, jobs, gtest_filter, tbpl_parser, debug, debugger,
              debugger_args):

        # We lazy build gtest because it's slow to link
        self._run_make(directory="testing/gtest", target='gtest',
                       print_directory=False, ensure_exit_code=True)

        app_path = self.get_binary_path('app')
        args = [app_path, '-unittest'];

        if debug or debugger or debugger_args:
            args = self.prepend_debugger_args(args, debugger, debugger_args)

        cwd = os.path.join(self.topobjdir, '_tests', 'gtest')

        if not os.path.isdir(cwd):
            os.makedirs(cwd)

        # Use GTest environment variable to control test execution
        # For details see:
        # https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_Test_Programs:_Advanced_Options
        gtest_env = {b'GTEST_FILTER': gtest_filter}

        # Note: we must normalize the path here so that gtest on Windows sees
        # a MOZ_GMP_PATH which has only Windows dir seperators, because
        # nsILocalFile cannot open the paths with non-Windows dir seperators.
        xre_path = os.path.join(os.path.normpath(self.topobjdir), "dist", "bin")
        gtest_env["MOZ_XRE_DIR"] = xre_path
        gtest_env["MOZ_GMP_PATH"] = os.pathsep.join(
            os.path.join(xre_path, p, "1.0")
            for p in ('gmp-fake', 'gmp-fakeopenh264')
        )

        gtest_env[b"MOZ_RUN_GTEST"] = b"True"

        if shuffle:
            gtest_env[b"GTEST_SHUFFLE"] = b"True"

        if tbpl_parser:
            gtest_env[b"MOZ_TBPL_PARSER"] = b"True"

        if jobs == 1:
            return self.run_process(args=args,
                                    append_env=gtest_env,
                                    cwd=cwd,
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
                             cwd=cwd,
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

    def prepend_debugger_args(self, args, debugger, debugger_args):
        '''
        Given an array with program arguments, prepend arguments to run it under a
        debugger.

        :param args: The executable and arguments used to run the process normally.
        :param debugger: The debugger to use, or empty to use the default debugger.
        :param debugger_args: Any additional parameters to pass to the debugger.
        '''

        import mozdebug

        if not debugger:
            # No debugger name was provided. Look for the default ones on
            # current OS.
            debugger = mozdebug.get_default_debugger_name(mozdebug.DebuggerSearch.KeepLooking)

        if debugger:
            debuggerInfo = mozdebug.get_debugger_info(debugger, debugger_args)
            if not debuggerInfo:
                print("Could not find a suitable debugger in your PATH.")
                return 1

        # Parameters come from the CLI. We need to convert them before
        # their use.
        if debugger_args:
            from mozbuild import shellutil
            try:
                debugger_args = shellutil.split(debugger_args)
            except shellutil.MetaCharacterException as e:
                print("The --debugger_args you passed require a real shell to parse them.")
                print("(We can't handle the %r character.)" % e.char)
                return 1

        # Prepend the debugger args.
        args = [debuggerInfo.path] + debuggerInfo.args + args
        return args

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
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output for what commands the packaging process is running.')
    def package(self, verbose=False):
        ret = self._run_make(directory=".", target='package',
                             silent=not verbose, ensure_exit_code=False)
        if ret == 0:
            self.notify('Packaging complete')
        return ret

@CommandProvider
class Install(MachCommandBase):
    """Install a package."""

    @Command('install', category='post-build',
        description='Install the package on the machine, or on a device.')
    def install(self):
        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device
            verify_android_device(self)
        ret = self._run_make(directory=".", target='install', ensure_exit_code=False)
        if ret == 0:
            self.notify('Install complete')
        return ret

@CommandProvider
class RunProgram(MachCommandBase):
    """Run the compiled program."""

    prog_group = 'the compiled program'

    @Command('run', category='post-build',
        description='Run the compiled program, possibly under a debugger or DMD.')
    @CommandArgument('params', nargs='...', group=prog_group,
        help='Command-line arguments to be passed through to the program. Not specifying a --profile or -P option will result in a temporary profile being used.')
    @CommandArgumentGroup(prog_group)
    @CommandArgument('--remote', '-r', action='store_true', group=prog_group,
        help='Do not pass the --no-remote argument by default.')
    @CommandArgument('--background', '-b', action='store_true', group=prog_group,
        help='Do not pass the --foreground argument by default on Mac.')
    @CommandArgument('--noprofile', '-n', action='store_true', group=prog_group,
        help='Do not pass the --profile argument by default.')

    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugparams', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')
    # Bug 933807 introduced JS_DISABLE_SLOW_SCRIPT_SIGNALS to avoid clever
    # segfaults induced by the slow-script-detecting logic for Ion/Odin JITted
    # code.  If we don't pass this, the user will need to periodically type
    # "continue" to (safely) resume execution.  There are ways to implement
    # automatic resuming; see the bug.
    @CommandArgument('--slowscript', action='store_true', group='debugging',
        help='Do not set the JS_DISABLE_SLOW_SCRIPT_SIGNALS env variable; when not set, recoverable but misleading SIGSEGV instances may occur in Ion/Odin JIT code.')

    @CommandArgumentGroup('DMD')
    @CommandArgument('--dmd', action='store_true', group='DMD',
        help='Enable DMD. The following arguments have no effect without this.')
    @CommandArgument('--mode', choices=['live', 'dark-matter', 'cumulative', 'scan'], group='DMD',
         help='Profiling mode. The default is \'dark-matter\'.')
    @CommandArgument('--stacks', choices=['partial', 'full'], group='DMD',
        help='Allocation stack trace coverage. The default is \'partial\'.')
    @CommandArgument('--show-dump-stats', action='store_true', group='DMD',
        help='Show stats when doing dumps.')
    def run(self, params, remote, background, noprofile, debug, debugger,
        debugparams, slowscript, dmd, mode, stacks, show_dump_stats):

        if conditions.is_android(self):
            # Running Firefox for Android is completely different
            if dmd:
                print("DMD is not supported for Firefox for Android")
                return 1
            from mozrunner.devices.android_device import verify_android_device, run_firefox_for_android
            if not (debug or debugger or debugparams):
                verify_android_device(self, install=True)
                return run_firefox_for_android(self, params)
            verify_android_device(self, install=True, debugger=True)
            args = ['']

        else:

            try:
                binpath = self.get_binary_path('app')
            except Exception as e:
                print("It looks like your program isn't built.",
                    "You can run |mach build| to build it.")
                print(e)
                return 1

            args = [binpath]

            if params:
                args.extend(params)

            if not remote:
                args.append('-no-remote')

            if not background and sys.platform == 'darwin':
                args.append('-foreground')

            no_profile_option_given = \
                all(p not in params for p in ['-profile', '--profile', '-P'])
            if no_profile_option_given and not noprofile:
                path = os.path.join(self.topobjdir, 'tmp', 'scratch_user')
                if not os.path.isdir(path):
                    os.makedirs(path)
                args.append('-profile')
                args.append(path)

        extra_env = {'MOZ_CRASHREPORTER_DISABLE': '1'}

        if debug or debugger or debugparams:
            if 'INSIDE_EMACS' in os.environ:
                self.log_manager.terminal_handler.setLevel(logging.WARNING)

            import mozdebug
            if not debugger:
                # No debugger name was provided. Look for the default ones on
                # current OS.
                debugger = mozdebug.get_default_debugger_name(mozdebug.DebuggerSearch.KeepLooking)

            if debugger:
                self.debuggerInfo = mozdebug.get_debugger_info(debugger, debugparams)
                if not self.debuggerInfo:
                    print("Could not find a suitable debugger in your PATH.")
                    return 1

            # Parameters come from the CLI. We need to convert them before
            # their use.
            if debugparams:
                from mozbuild import shellutil
                try:
                    debugparams = shellutil.split(debugparams)
                except shellutil.MetaCharacterException as e:
                    print("The --debugparams you passed require a real shell to parse them.")
                    print("(We can't handle the %r character.)" % e.char)
                    return 1

            if not slowscript:
                extra_env['JS_DISABLE_SLOW_SCRIPT_SIGNALS'] = '1'

            # Prepend the debugger args.
            args = [self.debuggerInfo.path] + self.debuggerInfo.args + args

        if dmd:
            dmd_params = []

            if mode:
                dmd_params.append('--mode=' + mode)
            if stacks:
                dmd_params.append('--stacks=' + stacks)
            if show_dump_stats:
                dmd_params.append('--show-dump-stats=yes')

            bin_dir = os.path.dirname(binpath)
            lib_name = self.substs['DLL_PREFIX'] + 'dmd' + self.substs['DLL_SUFFIX']
            dmd_lib = os.path.join(bin_dir, lib_name)
            if not os.path.exists(dmd_lib):
                print("Please build with |--enable-dmd| to use DMD.")
                return 1

            env_vars = {
                "Darwin": {
                    "DYLD_INSERT_LIBRARIES": dmd_lib,
                    "LD_LIBRARY_PATH": bin_dir,
                },
                "Linux": {
                    "LD_PRELOAD": dmd_lib,
                    "LD_LIBRARY_PATH": bin_dir,
                },
                "WINNT": {
                    "MOZ_REPLACE_MALLOC_LIB": dmd_lib,
                },
            }

            arch = self.substs['OS_ARCH']

            if dmd_params:
                env_vars[arch]["DMD"] = " ".join(dmd_params)

            extra_env.update(env_vars.get(arch, {}))

        return self.run_process(args=args, ensure_exit_code=False,
            pass_thru=True, append_env=extra_env)

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
            relpath = os.path.relpath(path, self.topsrcdir)
            try:
                statements = [s for s in pymake.parser.parsefile(path)
                    if is_statement_relevant(s)]

                if not statements:
                    print(relpath)
            except pymake.parser.SyntaxError:
                print('Warning: Could not parse %s' % relpath, file=sys.stderr)

    def _makefile_ins(self):
        for root, dirs, files in os.walk(self.topsrcdir):
            for f in files:
                if f == 'Makefile.in':
                    yield os.path.join(root, f)

@CommandProvider
class MachDebug(MachCommandBase):
    @Command('environment', category='build-dev',
        description='Show info about the mach and build environment.')
    @CommandArgument('--format', default='pretty',
        choices=['pretty', 'client.mk', 'configure', 'json'],
        help='Print data in the given format.')
    @CommandArgument('--output', '-o', type=str,
        help='Output to the given file.')
    @CommandArgument('--verbose', '-v', action='store_true',
        help='Print verbose output.')
    def environment(self, format, output=None, verbose=False):
        func = getattr(self, '_environment_%s' % format.replace('.', '_'))

        if output:
            # We want to preserve mtimes if the output file already exists
            # and the content hasn't changed.
            from mozbuild.util import FileAvoidWrite
            with FileAvoidWrite(output) as out:
                return func(out, verbose)
        return func(sys.stdout, verbose)

    def _environment_pretty(self, out, verbose):
        state_dir = self._mach_context.state_dir
        import platform
        print('platform:\n\t%s' % platform.platform(), file=out)
        print('python version:\n\t%s' % sys.version, file=out)
        print('python prefix:\n\t%s' % sys.prefix, file=out)
        print('mach cwd:\n\t%s' % self._mach_context.cwd, file=out)
        print('os cwd:\n\t%s' % os.getcwd(), file=out)
        print('mach directory:\n\t%s' % self._mach_context.topdir, file=out)
        print('state directory:\n\t%s' % state_dir, file=out)

        print('object directory:\n\t%s' % self.topobjdir, file=out)

        if self.mozconfig['path']:
            print('mozconfig path:\n\t%s' % self.mozconfig['path'], file=out)
            if self.mozconfig['configure_args']:
                print('mozconfig configure args:', file=out)
                for arg in self.mozconfig['configure_args']:
                    print('\t%s' % arg, file=out)

            if self.mozconfig['make_extra']:
                print('mozconfig extra make args:', file=out)
                for arg in self.mozconfig['make_extra']:
                    print('\t%s' % arg, file=out)

            if self.mozconfig['make_flags']:
                print('mozconfig make flags:', file=out)
                for arg in self.mozconfig['make_flags']:
                    print('\t%s' % arg, file=out)

        config = None

        try:
            config = self.config_environment

        except Exception:
            pass

        if config:
            print('config topsrcdir:\n\t%s' % config.topsrcdir, file=out)
            print('config topobjdir:\n\t%s' % config.topobjdir, file=out)

            if verbose:
                print('config substitutions:', file=out)
                for k in sorted(config.substs):
                    print('\t%s: %s' % (k, config.substs[k]), file=out)

                print('config defines:', file=out)
                for k in sorted(config.defines):
                    print('\t%s' % k, file=out)

    def _environment_client_mk(self, out, verbose):
        if self.mozconfig['make_extra']:
            for arg in self.mozconfig['make_extra']:
                print(arg, file=out)
        if self.mozconfig['make_flags']:
            print('MOZ_MAKE_FLAGS=%s' % ' '.join(self.mozconfig['make_flags']))
        objdir = mozpath.normsep(self.topobjdir)
        print('MOZ_OBJDIR=%s' % objdir, file=out)
        if 'MOZ_CURRENT_PROJECT' in os.environ:
            objdir = mozpath.join(objdir, os.environ['MOZ_CURRENT_PROJECT'])
        print('OBJDIR=%s' % objdir, file=out)
        if self.mozconfig['path']:
            print('FOUND_MOZCONFIG=%s' % mozpath.normsep(self.mozconfig['path']),
                file=out)

    def _environment_json(self, out, verbose):
        import json
        class EnvironmentEncoder(json.JSONEncoder):
            def default(self, obj):
                if isinstance(obj, MozbuildObject):
                    result = {
                        'topsrcdir': obj.topsrcdir,
                        'topobjdir': obj.topobjdir,
                        'mozconfig': obj.mozconfig,
                    }
                    if verbose:
                        result['substs'] = obj.substs
                        result['defines'] = obj.defines
                    return result
                elif isinstance(obj, set):
                    return list(obj)
                return json.JSONEncoder.default(self, obj)
        json.dump(self, cls=EnvironmentEncoder, sort_keys=True, fp=out)

class ArtifactSubCommand(SubCommand):
    def __call__(self, func):
        after = SubCommand.__call__(self, func)
        jobchoices = {
            'android-api-15',
            'android-x86',
            'linux',
            'linux64',
            'macosx64',
            'win32',
            'win64'
        }
        args = [
            CommandArgument('--tree', metavar='TREE', type=str,
                help='Firefox tree.'),
            CommandArgument('--job', metavar='JOB', choices=jobchoices,
                help='Build job.'),
            CommandArgument('--verbose', '-v', action='store_true',
                help='Print verbose output.'),
        ]
        for arg in args:
            after = arg(after)
        return after


@CommandProvider
class PackageFrontend(MachCommandBase):
    """Fetch and install binary artifacts from Mozilla automation."""

    @Command('artifact', category='post-build',
        description='Use pre-built artifacts to build Firefox.')
    def artifact(self):
        '''Download, cache, and install pre-built binary artifacts to build Firefox.

        Use |mach build| as normal to freshen your installed binary libraries:
        artifact builds automatically download, cache, and install binary
        artifacts from Mozilla automation, replacing whatever may be in your
        object directory.  Use |mach artifact last| to see what binary artifacts
        were last used.

        Never build libxul again!

        '''
        pass

    def _set_log_level(self, verbose):
        self.log_manager.terminal_handler.setLevel(logging.INFO if not verbose else logging.DEBUG)

    def _install_pip_package(self, package):
        if os.environ.get('MOZ_AUTOMATION'):
            self.virtualenv_manager._run_pip([
                'install',
                package,
                '--no-index',
                '--find-links',
                'http://pypi.pub.build.mozilla.org/pub',
                '--trusted-host',
                'pypi.pub.build.mozilla.org',
            ])
            return
        self.virtualenv_manager.install_pip_package(package)

    def _make_artifacts(self, tree=None, job=None, skip_cache=False):
        # Undo PATH munging that will be done by activating the virtualenv,
        # so that invoked subprocesses expecting to find system python
        # (git cinnabar, in particular), will not find virtualenv python.
        original_path = os.environ.get('PATH', '')
        self._activate_virtualenv()
        os.environ['PATH'] = original_path

        for package in ('pylru==1.0.9',
                        'taskcluster==0.0.32',
                        'mozregression==1.0.2'):
            self._install_pip_package(package)

        state_dir = self._mach_context.state_dir
        cache_dir = os.path.join(state_dir, 'package-frontend')

        try:
            os.makedirs(cache_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

        import which

        here = os.path.abspath(os.path.dirname(__file__))
        build_obj = MozbuildObject.from_environment(cwd=here)

        hg = None
        if conditions.is_hg(build_obj):
            if self._is_windows():
                hg = which.which('hg.exe')
            else:
                hg = which.which('hg')

        git = None
        if conditions.is_git(build_obj):
            if self._is_windows():
                git = which.which('git.exe')
            else:
                git = which.which('git')

        # Absolutely must come after the virtualenv is populated!
        from mozbuild.artifacts import Artifacts
        artifacts = Artifacts(tree, self.substs, self.defines, job,
                              log=self.log, cache_dir=cache_dir,
                              skip_cache=skip_cache, hg=hg, git=git)
        return artifacts

    @ArtifactSubCommand('artifact', 'install',
        'Install a good pre-built artifact.')
    @CommandArgument('source', metavar='SRC', nargs='?', type=str,
        help='Where to fetch and install artifacts from.  Can be omitted, in '
            'which case the current hg repository is inspected; an hg revision; '
            'a remote URL; or a local file.',
        default=None)
    @CommandArgument('--skip-cache', action='store_true',
        help='Skip all local caches to force re-fetching remote artifacts.',
        default=False)
    def artifact_install(self, source=None, skip_cache=False, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job, skip_cache=skip_cache)

        return artifacts.install_from(source, self.distdir)

    @ArtifactSubCommand('artifact', 'last',
        'Print the last pre-built artifact installed.')
    def artifact_print_last(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.print_last()
        return 0

    @ArtifactSubCommand('artifact', 'print-cache',
        'Print local artifact cache for debugging.')
    def artifact_print_cache(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.print_cache()
        return 0

    @ArtifactSubCommand('artifact', 'clear-cache',
        'Delete local artifacts and reset local artifact cache.')
    def artifact_clear_cache(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.clear_cache()
        return 0
