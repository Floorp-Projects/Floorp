# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import collections
import errno
import itertools
import json
import logging
import operator
import os
import subprocess
import sys

from collections import OrderedDict

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

from mozbuild.backend import (
    backends,
    get_backend_class,
)
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


class Footer(object):
    """Handles display of a footer in a terminal.

    This class implements the functionality common to all mach commands
    that render a footer.
    """

    def __init__(self, terminal):
        # terminal is a blessings.Terminal.
        self._t = terminal
        self._fh = sys.stdout

    def clear(self):
        """Removes the footer from the current terminal."""
        self._fh.write(self._t.move_x(0))
        self._fh.write(self._t.clear_eol())

    def write(self, parts):
        """Write some output in the footer, accounting for terminal width.

        parts is a list of 2-tuples of (encoding_function, input).
        None means no encoding."""

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


class BuildProgressFooter(Footer):
    """Handles display of a build progress indicator in a terminal.

    When mach builds inside a blessings-supported terminal, it will render
    progress information collected from a BuildMonitor. This class converts the
    state of BuildMonitor into terminal output.
    """

    def __init__(self, terminal, monitor):
        Footer.__init__(self, terminal)
        self.tiers = monitor.tiers.tier_status.viewitems()

    def draw(self):
        """Draws this footer in the terminal."""

        if not self.tiers:
            return

        # The drawn terminal looks something like:
        # TIER: static export libs tools

        parts = [('bold', 'TIER:')]
        append = parts.append
        for tier, status in self.tiers:
            if status is None:
                append(tier)
            elif status == 'finished':
                append(('green', tier))
            else:
                append(('underline_yellow', tier))

        self.write(parts)


class OutputManager(LoggingMixin):
    """Handles writing job output to a terminal or log."""

    def __init__(self, log_manager, footer):
        self.populate_logger()

        self.footer = None
        terminal = log_manager.terminal

        # TODO convert terminal footer to config file setting.
        if not terminal or os.environ.get('MACH_NO_TERMINAL_FOOTER', None):
            return
        if os.environ.get('INSIDE_EMACS', None):
            return

        self.t = terminal
        self.footer = footer

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

class BuildOutputManager(OutputManager):
    """Handles writing build output to a terminal, to logs, etc."""

    def __init__(self, log_manager, monitor, footer):
        self.monitor = monitor
        OutputManager.__init__(self, log_manager, footer)

    def __exit__(self, exc_type, exc_value, traceback):
        OutputManager.__exit__(self, exc_type, exc_value, traceback)

        # Ensure the resource monitor is stopped because leaving it running
        # could result in the process hanging on exit because the resource
        # collection child process hasn't been told to stop.
        self.monitor.stop_resource_recording()


    def on_line(self, line):
        warning, state_changed, relevant = self.monitor.on_line(line)

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


class StoreDebugParamsAndWarnAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        sys.stderr.write('The --debugparams argument is deprecated. Please ' +
                         'use --debugger-args instead.\n\n')
        setattr(namespace, self.dest, values)


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
    @CommandArgument('--keep-going', action='store_true',
                     help='Keep building after an error has occurred')
    def build(self, what=None, disable_extra_make_dependencies=None, jobs=0,
        directory=None, verbose=False, keep_going=False):
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
        footer = BuildProgressFooter(self.log_manager.terminal, monitor)

        # Disable indexing in objdir because it is not necessary and can slow
        # down builds.
        mkdir(self.topobjdir, not_indexed=True)

        with BuildOutputManager(self.log_manager, monitor, footer) as output:
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
                    print_directory=False, keep_going=keep_going)

                # Build target pairs.
                for make_dir, make_target in target_pairs:
                    # We don't display build status messages during partial
                    # tree builds because they aren't reliable there. This
                    # could potentially be fixed if the build monitor were more
                    # intelligent about encountering undefined state.
                    status = self._run_make(directory=make_dir, target=make_target,
                        line_handler=output.on_line, log=False, print_directory=False,
                        ensure_exit_code=False, num_jobs=jobs, silent=not verbose,
                        append_env={b'NO_BUILDSTATUS_MESSAGES': b'1'},
                        keep_going=keep_going)

                    if status != 0:
                        break
            else:
                # Try to call the default backend's build() method. This will
                # run configure to determine BUILD_BACKENDS if it hasn't run
                # yet.
                config = None
                try:
                    config = self.config_environment
                except Exception:
                    config_rc = self.configure(buildstatus_messages=True,
                                               line_handler=output.on_line)
                    if config_rc != 0:
                        return config_rc

                    # Even if configure runs successfully, we may have trouble
                    # getting the config_environment for some builds, such as
                    # OSX Universal builds. These have to go through client.mk
                    # regardless.
                    try:
                        config = self.config_environment
                    except Exception:
                        pass

                if config:
                    active_backend = config.substs.get('BUILD_BACKENDS', [None])[0]
                    if active_backend:
                        backend_cls = get_backend_class(active_backend)(config)
                        status = backend_cls.build(self, output, jobs, verbose)

                # If the backend doesn't specify a build() method, then just
                # call client.mk directly.
                if status is None:
                    status = self._run_make(srcdir=True, filename='client.mk',
                        line_handler=output.on_line, log=False, print_directory=False,
                        allow_parallel=False, ensure_exit_code=False, num_jobs=jobs,
                        silent=not verbose, keep_going=keep_going)

                self.log(logging.WARNING, 'warning_summary',
                    {'count': len(monitor.warnings_database)},
                    '{count} compiler warnings present.')

            # Print the collected compiler warnings. This is redundant with
            # inline output from the compiler itself. However, unlike inline
            # output, this list is sorted and grouped by file, making it
            # easier to triage output.
            #
            # Only do this if we had a successful build. If the build failed,
            # there are more important things in the log to look for than
            # whatever code we warned about.
            if not status:
                # Suppress warnings for 3rd party projects in local builds
                # until we suppress them for real.
                # TODO remove entries/feature once we stop generating warnings
                # in these directories.
                pathToThirdparty = os.path.join(self.topsrcdir,
                                                "tools",
                                               "rewriting",
                                               "ThirdPartyPaths.txt")

                if os.path.exists(pathToThirdparty):
                    with open(pathToThirdparty) as f:
                        # Normalize the path (no trailing /)
                        LOCAL_SUPPRESS_DIRS = tuple(d.rstrip('/') for d in f.read().splitlines())
                else:
                    # For application based on gecko like thunderbird
                    LOCAL_SUPPRESS_DIRS = ()

                suppressed_by_dir = collections.Counter()

                for warning in sorted(monitor.instance_warnings):
                    path = mozpath.normsep(warning['filename'])
                    if path.startswith(self.topsrcdir):
                        path = path[len(self.topsrcdir) + 1:]

                    warning['normpath'] = path

                    if (path.startswith(LOCAL_SUPPRESS_DIRS) and
                            'MOZ_AUTOMATION' not in os.environ):
                        for d in LOCAL_SUPPRESS_DIRS:
                            if path.startswith(d):
                                suppressed_by_dir[d] += 1
                                break

                        continue

                    if warning['column'] is not None:
                        self.log(logging.WARNING, 'compiler_warning', warning,
                                 'warning: {normpath}:{line}:{column} [{flag}] '
                                 '{message}')
                    else:
                        self.log(logging.WARNING, 'compiler_warning', warning,
                                 'warning: {normpath}:{line} [{flag}] {message}')

                for d, count in sorted(suppressed_by_dir.items()):
                    self.log(logging.WARNING, 'suppressed_warning',
                             {'dir': d, 'count': count},
                             '(suppressed {count} warnings in {dir})')

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
                for key in ['MOZ_ARTIFACT_BUILDS', 'MOZ_USING_CCACHE', 'MOZ_USING_SCCACHE']:
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
    def configure(self, options=None, buildstatus_messages=False, line_handler=None):
        def on_line(line):
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        line_handler = line_handler or on_line

        options = ' '.join(shell_quote(o) for o in options or ())
        append_env = {b'CONFIGURE_ARGS': options.encode('utf-8')}

        # Only print build status messages when we have an active
        # monitor.
        if not buildstatus_messages:
            append_env[b'NO_BUILDSTATUS_MESSAGES'] =  b'1'
        status = self._run_make(srcdir=True, filename='client.mk',
            target='configure', line_handler=line_handler, log=False,
            print_directory=False, allow_parallel=False, ensure_exit_code=False,
            append_env=append_env)

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


@CommandProvider
class CargoProvider(MachCommandBase):
    """Invoke cargo in useful ways."""

    @Command('cargo', category='build',
             description='Invoke cargo in useful ways.')
    def cargo(self):
        self.parser.print_usage()
        return 1

    @SubCommand('cargo', 'check',
                description='Run `cargo check` on a given crate.  Defaults to gkrust.')
    @CommandArgument('crates', default=None, nargs='*', help='The crate name(s) to check.')
    def check(self, crates=None):
        # XXX duplication with `mach vendor rust`
        crates_and_roots = {
            'gkrust': 'toolkit/library/rust',
            'gkrust-gtest': 'toolkit/library/gtest/rust',
            'mozjs_sys': 'js/src',
            'geckodriver': 'testing/geckodriver',
        }

        if crates == None:
            crates = ['gkrust']

        for crate in crates:
            root = crates_and_roots.get(crate, None)
            if not root:
                print('Cannot locate crate %s.  Please check your spelling or '
                      'add the crate information to the list.' % crate)
                return 1

            check_targets = [
                'force-cargo-library-check',
                'force-cargo-host-library-check',
                'force-cargo-program-check',
                'force-cargo-host-program-check',
            ]

            ret = self._run_make(srcdir=False, directory=root,
                                 ensure_exit_code=0, silent=True,
                                 print_directory=False, target=check_targets)
            if ret != 0:
                return ret

        return 0

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
class Clobber(MachCommandBase):
    NO_AUTO_LOG = True
    CLOBBER_CHOICES = ['objdir', 'python']
    @Command('clobber', category='build',
        description='Clobber the tree (delete the object directory).')
    @CommandArgument('what', default=['objdir'], nargs='*',
        help='Target to clobber, must be one of {{{}}} (default objdir).'.format(
             ', '.join(CLOBBER_CHOICES)))
    @CommandArgument('--full', action='store_true',
        help='Perform a full clobber')
    def clobber(self, what, full=False):
        invalid = set(what) - set(self.CLOBBER_CHOICES)
        if invalid:
            print('Unknown clobber target(s): {}'.format(', '.join(invalid)))
            return 1

        ret = 0
        if 'objdir' in what:
            from mozbuild.controller.clobber import Clobberer
            try:
                Clobberer(self.topsrcdir, self.topobjdir).remove_objdir(full)
            except OSError as e:
                if sys.platform.startswith('win'):
                    if isinstance(e, WindowsError) and e.winerror in (5,32):
                        self.log(logging.ERROR, 'file_access_error', {'error': e},
                            "Could not clobber because a file was in use. If the "
                            "application is running, try closing it. {error}")
                        return 1
                raise

        if 'python' in what:
            if conditions.is_hg(self):
                cmd = ['hg', 'purge', '--all', '-I', 'glob:**.py[co]']
            elif conditions.is_git(self):
                cmd = ['git', 'clean', '-f', '-x', '*.py[co]']
            else:
                cmd = ['find', '.', '-type', 'f', '-name', '*.py[co]', '-delete']
            ret = subprocess.call(cmd, cwd=self.topsrcdir)
        return ret

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

        if os.isatty(sys.stdout.fileno()):
            env = dict(os.environ)
            if 'LESS' not in env:
                # Sensible default flags if none have been set in the user
                # environment.
                env[b'LESS'] = b'FRX'
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
    @CommandArgument('--verbose', '-v', action='store_true',
        help='Print verbose output when installing to an Android emulator.')
    def install(self, verbose=False):
        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device
            verify_android_device(self, verbose=verbose)
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
    @CommandArgument('--disable-e10s', action='store_true', group=prog_group,
        help='Run the program with electrolysis disabled.')
    @CommandArgument('--enable-crash-reporter', action='store_true', group=prog_group,
        help='Run the program with the crash reporter enabled.')

    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')
    @CommandArgument('--debugparams', action=StoreDebugParamsAndWarnAction,
        default=None, type=str, dest='debugger_args', group='debugging',
        help=argparse.SUPPRESS)

    @CommandArgumentGroup('DMD')
    @CommandArgument('--dmd', action='store_true', group='DMD',
        help='Enable DMD. The following arguments have no effect without this.')
    @CommandArgument('--mode', choices=['live', 'dark-matter', 'cumulative', 'scan'], group='DMD',
         help='Profiling mode. The default is \'dark-matter\'.')
    @CommandArgument('--stacks', choices=['partial', 'full'], group='DMD',
        help='Allocation stack trace coverage. The default is \'partial\'.')
    @CommandArgument('--show-dump-stats', action='store_true', group='DMD',
        help='Show stats when doing dumps.')
    def run(self, params, remote, background, noprofile, disable_e10s,
        enable_crash_reporter, debug, debugger, debugger_args,
        dmd, mode, stacks, show_dump_stats):

        if conditions.is_android(self):
            # Running Firefox for Android is completely different
            if dmd:
                print("DMD is not supported for Firefox for Android")
                return 1
            from mozrunner.devices.android_device import verify_android_device, run_firefox_for_android
            if not (debug or debugger or debugger_args):
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

        extra_env = {
            'MOZ_DEVELOPER_REPO_DIR': self.topsrcdir,
            'MOZ_DEVELOPER_OBJ_DIR': self.topobjdir,
            'RUST_BACKTRACE': '1',
        }

        if not enable_crash_reporter:
            extra_env['MOZ_CRASHREPORTER_DISABLE'] = '1'
        else:
            extra_env['MOZ_CRASHREPORTER'] = '1'

        if disable_e10s:
            extra_env['MOZ_FORCE_DISABLE_E10S'] = '1'

        if debug or debugger or debugger_args:
            if 'INSIDE_EMACS' in os.environ:
                self.log_manager.terminal_handler.setLevel(logging.WARNING)

            import mozdebug
            if not debugger:
                # No debugger name was provided. Look for the default ones on
                # current OS.
                debugger = mozdebug.get_default_debugger_name(mozdebug.DebuggerSearch.KeepLooking)

            if debugger:
                self.debuggerInfo = mozdebug.get_debugger_info(debugger, debugger_args)
                if not self.debuggerInfo:
                    print("Could not find a suitable debugger in your PATH.")
                    return 1

            # Parameters come from the CLI. We need to convert them before
            # their use.
            if debugger_args:
                from mozbuild import shellutil
                try:
                    debugger_args = shellutil.split(debugger_args)
                except shellutil.MetaCharacterException as e:
                    print("The --debugger-args you passed require a real shell to parse them.")
                    print("(We can't handle the %r character.)" % e.char)
                    return 1

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

    def _make_artifacts(self, tree=None, job=None, skip_cache=False):
        state_dir = self._mach_context.state_dir
        cache_dir = os.path.join(state_dir, 'package-frontend')

        here = os.path.abspath(os.path.dirname(__file__))
        build_obj = MozbuildObject.from_environment(cwd=here)

        hg = None
        if conditions.is_hg(build_obj):
            hg = build_obj.substs['HG']

        git = None
        if conditions.is_git(build_obj):
            git = build_obj.substs['GIT']

        from mozbuild.artifacts import Artifacts
        artifacts = Artifacts(tree, self.substs, self.defines, job,
                              log=self.log, cache_dir=cache_dir,
                              skip_cache=skip_cache, hg=hg, git=git,
                              topsrcdir=self.topsrcdir)
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

    @ArtifactSubCommand('artifact', 'clear-cache',
        'Delete local artifacts and reset local artifact cache.')
    def artifact_clear_cache(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.clear_cache()
        return 0

    @SubCommand('artifact', 'toolchain')
    @CommandArgument('--verbose', '-v', action='store_true',
        help='Print verbose output.')
    @CommandArgument('--cache-dir', metavar='DIR',
        help='Directory where to store the artifacts cache')
    @CommandArgument('--skip-cache', action='store_true',
        help='Skip all local caches to force re-fetching remote artifacts.',
        default=False)
    @CommandArgument('--from-build', metavar='BUILD', nargs='+',
        help='Get toolchains resulting from the given build(s)')
    @CommandArgument('--tooltool-manifest', metavar='MANIFEST',
        help='Explicit tooltool manifest to process')
    @CommandArgument('--authentication-file', metavar='FILE',
        help='Use the RelengAPI token found in the given file to authenticate')
    @CommandArgument('--tooltool-url', metavar='URL',
        help='Use the given url as tooltool server')
    @CommandArgument('--no-unpack', action='store_true',
        help='Do not unpack any downloaded file')
    @CommandArgument('--retry', type=int, default=0,
        help='Number of times to retry failed downloads')
    @CommandArgument('files', nargs='*',
        help='Only download the given file names (you may use file name stems)')
    def artifact_toolchain(self, verbose=False, cache_dir=None,
                          skip_cache=False, from_build=(),
                          tooltool_manifest=None, authentication_file=None,
                          tooltool_url=None, no_unpack=False, retry=None,
                          files=()):
        '''Download, cache and install pre-built toolchains.
        '''
        from mozbuild.artifacts import ArtifactCache
        from mozbuild.action.tooltool import (
            FileRecord,
            open_manifest,
            unpack_file,
        )
        from requests.adapters import HTTPAdapter
        import redo
        import requests
        import shutil

        from taskgraph.generator import Kind
        from taskgraph.optimize import optimize_task
        from taskgraph.util.taskcluster import (
            get_artifact_url,
            list_artifacts,
        )
        import yaml

        self._set_log_level(verbose)
        # Normally, we'd use self.log_manager.enable_unstructured(),
        # but that enables all logging, while we only really want tooltool's
        # and it also makes structured log output twice.
        # So we manually do what it does, and limit that to the tooltool
        # logger.
        if self.log_manager.terminal_handler:
            logging.getLogger('mozbuild.action.tooltool').addHandler(
                self.log_manager.terminal_handler)
            logging.getLogger('redo').addHandler(
                self.log_manager.terminal_handler)
            self.log_manager.terminal_handler.addFilter(
                self.log_manager.structured_filter)
        if not cache_dir:
            cache_dir = os.path.join(self._mach_context.state_dir, 'toolchains')

        tooltool_url = (tooltool_url or
                        'https://api.pub.build.mozilla.org/tooltool').rstrip('/')

        cache = ArtifactCache(cache_dir=cache_dir, log=self.log,
                              skip_cache=skip_cache)

        if authentication_file:
            with open(authentication_file, 'rb') as f:
                token = f.read().strip()

            class TooltoolAuthenticator(HTTPAdapter):
                def send(self, request, *args, **kwargs):
                    request.headers['Authorization'] = \
                        'Bearer {}'.format(token)
                    return super(TooltoolAuthenticator, self).send(
                        request, *args, **kwargs)

            cache._download_manager.session.mount(
                tooltool_url, TooltoolAuthenticator())

        class DownloadRecord(FileRecord):
            def __init__(self, url, *args, **kwargs):
                super(DownloadRecord, self).__init__(*args, **kwargs)
                self.url = url
                self.basename = self.filename

            def fetch_with(self, cache):
                self.filename = cache.fetch(self.url)
                return self.filename

            def validate(self):
                if self.size is None and self.digest is None:
                    return True
                return super(DownloadRecord, self).validate()

        records = OrderedDict()
        downloaded = []

        if tooltool_manifest:
            manifest = open_manifest(tooltool_manifest)
            for record in manifest.file_records:
                url = '{}/{}/{}'.format(tooltool_url, record.algorithm,
                                        record.digest)
                records[record.filename] = DownloadRecord(
                    url, record.filename, record.size, record.digest,
                    record.algorithm, unpack=record.unpack,
                    version=record.version, visibility=record.visibility,
                    setup=record.setup)

        if from_build:
            params = {
                'message': '',
                'project': '',
                'level': os.environ.get('MOZ_SCM_LEVEL', '3'),
                'base_repository': '',
                'head_repository': '',
                'head_rev': '',
                'moz_build_date': '',
                'build_date': 0,
                'pushlog_id': 0,
                'owner': '',
            }

            # TODO: move to the taskcluster package
            def tasks(kind):
                kind_path = mozpath.join(self.topsrcdir, 'taskcluster', 'ci', kind)
                with open(mozpath.join(kind_path, 'kind.yml')) as f:
                    config = yaml.load(f)
                    tasks = Kind(kind, kind_path, config).load_tasks(params, {})
                    return {
                        task.task['metadata']['name']: task
                        for task in tasks
                    }

            toolchains = tasks('toolchain')

            for b in from_build:
                user_value = b

                if not b.startswith('toolchain-'):
                    b = 'toolchain-{}'.format(b)

                task = toolchains.get(b)
                if not task:
                    self.log(logging.ERROR, 'artifact', {'build': user_value},
                             'Could not find a toolchain build named `{build}`')
                    return 1

                task_id = optimize_task(task, {})
                if task_id in (True, False):
                    self.log(logging.ERROR, 'artifact', {'build': user_value},
                             'Could not find artifacts for a toolchain build '
                             'named `{build}`')
                    return 1

                for artifact in list_artifacts(task_id):
                    name = artifact['name']
                    if not name.startswith('public/'):
                        continue
                    name = name[len('public/'):]
                    if name.startswith('logs/'):
                        continue
                    name = os.path.basename(name)
                    records[name] = DownloadRecord(
                        get_artifact_url(task_id, artifact['name']),
                        name, None, None, None, unpack=True)

        for record in records.itervalues():
            if files and not any(record.basename == f or
                                      record.basename.startswith('%s.' % f)
                                      for f in files):
                continue

            self.log(logging.INFO, 'artifact', {'name': record.basename},
                     'Downloading {name}')
            valid = False
            # sleeptime is 60 per retry.py, used by tooltool_wrapper.sh
            for attempt, _ in enumerate(redo.retrier(attempts=retry+1,
                                                     sleeptime=60)):
                try:
                    record.fetch_with(cache)
                except (requests.exceptions.HTTPError,
                        requests.exceptions.ChunkedEncodingError,
                        requests.exceptions.ConnectionError) as e:

                    if isinstance(e, requests.exceptions.ConnectionError):
                        should_retry = True
                    else:
                        # The relengapi proxy likes to return error 400 bad request
                        # which seems improbably to be due to our (simple) GET
                        # being borked.
                        status = e.response.status_code
                        should_retry = status >= 500 or status == 400

                    if should_retry or attempt < retry:
                        level = logging.WARN
                    else:
                        level = logging.ERROR
                    # e.message is not always a string, so convert it first.
                    self.log(level, 'artifact', {}, str(e.message))
                    if not should_retry:
                        break
                    if attempt < retry:
                        self.log(logging.INFO, 'artifact', {},
                                 'Will retry in a moment...')
                    continue
                try:
                    valid = record.validate()
                except Exception:
                    pass
                if not valid:
                    os.unlink(record.filename)
                    if attempt < retry:
                        self.log(logging.INFO, 'artifact', {},
                                 'Will retry in a moment...')
                    continue

                downloaded.append(record)
                break

            if not valid:
                self.log(logging.ERROR, 'artifact', {'name': record.basename},
                         'Failed to download {name}')
                return 1

        for record in downloaded:
            local = os.path.join(os.getcwd(), record.basename)
            if os.path.exists(local):
                os.unlink(local)
            # unpack_file needs the file with its final name to work
            # (https://github.com/mozilla/build-tooltool/issues/38), so we
            # need to copy it, even though we remove it later. Use hard links
            # when possible.
            try:
                os.link(record.filename, local)
            except Exception:
                shutil.copy(record.filename, local)
            if record.unpack and not no_unpack:
                unpack_file(local, record.setup)
                os.unlink(local)

        if not downloaded:
            self.log(logging.ERROR, 'artifact', {}, 'Nothing to download')
            if files:
                return 1

        return 0


@CommandProvider
class Vendor(MachCommandBase):
    """Vendor third-party dependencies into the source repository."""

    @Command('vendor', category='misc',
             description='Vendor third-party dependencies into the source repository.')
    def vendor(self):
        self.parser.print_usage()
        sys.exit(1)

    @SubCommand('vendor', 'rust',
                description='Vendor rust crates from crates.io into third_party/rust')
    @CommandArgument('--ignore-modified', action='store_true',
        help='Ignore modified files in current checkout',
        default=False)
    @CommandArgument('--build-peers-said-large-imports-were-ok', action='store_true',
        help='Permit overly-large files to be added to the repository',
        default=False)
    def vendor_rust(self, **kwargs):
        from mozbuild.vendor_rust import VendorRust
        vendor_command = self._spawn(VendorRust)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'aom',
                description='Vendor av1 video codec reference implementation into the source repository.')
    @CommandArgument('-r', '--revision',
        help='Repository tag or commit to update to.')
    @CommandArgument('--ignore-modified', action='store_true',
        help='Ignore modified files in current checkout',
        default=False)
    def vendor_aom(self, **kwargs):
        from mozbuild.vendor_aom import VendorAOM
        vendor_command = self._spawn(VendorAOM)
        vendor_command.vendor(**kwargs)


@CommandProvider
class WebRTCGTestCommands(GTestCommands):
    @Command('webrtc-gtest', category='testing',
        description='Run WebRTC.org GTest unit tests.')
    @CommandArgument('gtest_filter', default=b"*", nargs='?', metavar='gtest_filter',
        help="test_filter is a ':'-separated list of wildcard patterns (called the positive patterns),"
             "optionally followed by a '-' and another ':'-separated pattern list (called the negative patterns).")
    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')
    def gtest(self, gtest_filter, debug, debugger,
              debugger_args):
        app_path = self.get_binary_path('webrtc-gtest')
        args = [app_path]

        if debug or debugger or debugger_args:
            args = self.prepend_debugger_args(args, debugger, debugger_args)

        # Used to locate resources used by tests
        cwd = os.path.join(self.topsrcdir, 'media', 'webrtc', 'trunk')

        if not os.path.isdir(cwd):
            print('Unable to find working directory for tests: %s' % cwd)
            return 1

        gtest_env = {
            # These tests are not run under ASAN upstream, so we need to
            # disable some checks.
            b'ASAN_OPTIONS': 'alloc_dealloc_mismatch=0',
            # Use GTest environment variable to control test execution
            # For details see:
            # https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_Test_Programs:_Advanced_Options
            b'GTEST_FILTER': gtest_filter
        }

        return self.run_process(args=args,
                                append_env=gtest_env,
                                cwd=cwd,
                                ensure_exit_code=False,
                                pass_thru=True)

@CommandProvider
class Repackage(MachCommandBase):
    '''Repackages artifacts into different formats.

    This is generally used after packages are signed by the signing
    scriptworkers in order to bundle things up into shippable formats, such as a
    .dmg on OSX or an installer exe on Windows.
    '''
    @Command('repackage', category='misc',
             description='Repackage artifacts into different formats.')
    def repackage(self):
        print("Usage: ./mach repackage [dmg|installer|mar] [args...]")

    @SubCommand('repackage', 'dmg',
                description='Repackage a tar file into a .dmg for OSX')
    @CommandArgument('--input', '-i', type=str, required=True,
        help='Input filename')
    @CommandArgument('--output', '-o', type=str, required=True,
        help='Output filename')
    def repackage_dmg(self, input, output):
        if not os.path.exists(input):
            print('Input file does not exist: %s' % input)
            return 1

        if not os.path.exists(os.path.join(self.topobjdir, 'config.status')):
            print('config.status not found.  Please run |mach configure| '
                  'prior to |mach repackage|.')
            return 1

        from mozbuild.repackaging.dmg import repackage_dmg
        repackage_dmg(input, output)

    @SubCommand('repackage', 'installer',
                description='Repackage into a Windows installer exe')
    @CommandArgument('--tag', type=str, required=True,
        help='The .tag file used to build the installer')
    @CommandArgument('--setupexe', type=str, required=True,
        help='setup.exe file inside the installer')
    @CommandArgument('--package', type=str, required=False,
        help='Optional package .zip for building a full installer')
    @CommandArgument('--output', '-o', type=str, required=True,
        help='Output filename')
    def repackage_installer(self, tag, setupexe, package, output):
        from mozbuild.repackaging.installer import repackage_installer
        repackage_installer(self.topsrcdir, tag, setupexe, package, output)

    @SubCommand('repackage', 'mar',
                description='Repackage into complete MAR file')
    @CommandArgument('--input', '-i', type=str, required=True,
        help='Input filename')
    @CommandArgument('--mar', type=str, required=True,
        help='Mar binary path')
    @CommandArgument('--output', '-o', type=str, required=True,
        help='Output filename')
    def repackage_mar(self, input, mar, output):
        from mozbuild.repackaging.mar import repackage_mar
        repackage_mar(self.topsrcdir, input, mar, output)
