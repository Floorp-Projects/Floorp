# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import itertools
import json
import logging
import operator
import os
import re
import shutil
import subprocess
import sys
import tempfile

import mozpack.path as mozpath

from mach.decorators import (
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    Command,
    SettingsProvider,
    SubCommand,
)

from mozbuild.base import (
    BinaryNotFoundException,
    BuildEnvironmentNotFoundException,
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

here = os.path.abspath(os.path.dirname(__file__))

EXCESSIVE_SWAP_MESSAGE = '''
===================
PERFORMANCE WARNING

Your machine experienced a lot of swap activity during the build. This is
possibly a sign that your machine doesn't have enough physical memory or
not enough available memory to perform the build. It's also possible some
other system activity during the build is to blame.

If you feel this message is not appropriate for your machine configuration,
please file a Firefox Build System :: General bug at
https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox%20Build%20System&component=General
and tell us about your machine and build configuration so we can adjust the
warning heuristic.
===================
'''


class StoreDebugParamsAndWarnAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        sys.stderr.write('The --debugparams argument is deprecated. Please ' +
                         'use --debugger-args instead.\n\n')
        setattr(namespace, self.dest, values)


@CommandProvider
class Watch(MachCommandBase):
    """Interface to watch and re-build the tree."""

    @Command('watch', category='post-build', description='Watch and re-build the tree.',
             conditions=[conditions.is_firefox])
    @CommandArgument('-v', '--verbose', action='store_true',
                     help='Verbose output for what commands the watcher is running.')
    def watch(self, verbose=False):
        """Watch and re-build the source tree."""

        if not conditions.is_artifact_build(self):
            print('mach watch requires an artifact build. See '
                  'https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build')  # noqa
            return 1

        if not self.substs.get('WATCHMAN', None):
            print('mach watch requires watchman to be installed. See '
                  'https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Incremental_builds_with_filesystem_watching')  # noqa
            return 1

        self._activate_virtualenv()
        try:
            self.virtualenv_manager.install_pip_package('pywatchman==1.4.1')
        except Exception:
            print('Could not install pywatchman from pip. See '
                  'https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Incremental_builds_with_filesystem_watching')  # noqa
            return 1

        from mozbuild.faster_daemon import Daemon
        daemon = Daemon(self.config_environment)

        try:
            return daemon.watch()
        except KeyboardInterrupt:
            # Suppress ugly stack trace when user hits Ctrl-C.
            sys.exit(3)


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
    @CommandArgument('--all-crates', default=None, action='store_true',
                     help='Check all of the crates in the tree.')
    @CommandArgument('crates', default=None, nargs='*', help='The crate name(s) to check.')
    @CommandArgument('--jobs', '-j', default='1', nargs='?', metavar='jobs', type=int,
                     help='Run the tests in parallel using multiple processes.')
    @CommandArgument('-v', '--verbose', action='store_true',
                     help='Verbose output.')
    def check(self, all_crates=None, crates=None, jobs=0, verbose=False):
        # XXX duplication with `mach vendor rust`
        crates_and_roots = {
            'gkrust': 'toolkit/library/rust',
            'gkrust-gtest': 'toolkit/library/gtest/rust',
            'js': 'js/rust',
            'mozjs_sys': 'js/src',
            'baldrdash': 'js/src/wasm/cranelift',
            'geckodriver': 'testing/geckodriver',
        }

        if all_crates:
            crates = crates_and_roots.keys()
        elif crates is None or crates == []:
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
                                 ensure_exit_code=0, silent=not verbose,
                                 print_directory=False, target=check_targets,
                                 num_jobs=jobs)
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
    CLOBBER_CHOICES = ['objdir', 'python', 'gradle']

    @Command('clobber', category='build',
             description='Clobber the tree (delete the object directory).')
    @CommandArgument('what', default=['objdir'], nargs='*',
                     help='Target to clobber, must be one of {{{}}} (default objdir).'.format(
             ', '.join(CLOBBER_CHOICES)))
    @CommandArgument('--full', action='store_true',
                     help='Perform a full clobber')
    def clobber(self, what, full=False):
        """Clean up the source and object directories.

        Performing builds and running various commands generate various files.

        Sometimes it is necessary to clean up these files in order to make
        things work again. This command can be used to perform that cleanup.

        By default, this command removes most files in the current object
        directory (where build output is stored). Some files (like Visual
        Studio project files) are not removed by default. If you would like
        to remove the object directory in its entirety, run with `--full`.

        The `python` target will clean up various generated Python files from
        the source directory and will remove untracked files from well-known
        directories containing Python packages. Run this to remove .pyc files,
        compiled C extensions, etc. Note: all files not tracked or ignored by
        version control in well-known Python package directories will be
        deleted. Run the `status` command of your VCS to see if any untracked
        files you haven't committed yet will be deleted.

        The `gradle` target will remove the "gradle" subdirectory of the object
        directory.
        """
        invalid = set(what) - set(self.CLOBBER_CHOICES)
        if invalid:
            print('Unknown clobber target(s): {}'.format(', '.join(invalid)))
            return 1

        ret = 0
        if 'objdir' in what:
            from mozbuild.controller.clobber import Clobberer
            try:
                Clobberer(self.topsrcdir, self.topobjdir, self.substs).remove_objdir(full)
            except OSError as e:
                if sys.platform.startswith('win'):
                    if isinstance(e, WindowsError) and e.winerror in (5, 32):
                        self.log(logging.ERROR, 'file_access_error', {'error': e},
                                 "Could not clobber because a file was in use. If the "
                                 "application is running, try closing it. {error}")
                        return 1
                raise

        if 'python' in what:
            if conditions.is_hg(self):
                cmd = ['hg', 'purge', '--all', '-I', 'glob:**.py[cdo]',
                       '-I', 'path:python/', '-I', 'path:third_party/python/']
            elif conditions.is_git(self):
                cmd = ['git', 'clean', '-f', '-x', '*.py[cdo]', 'python/',
                       'third_party/python/']
            else:
                # We don't know what is tracked/untracked if we don't have VCS.
                # So we can't clean python/ and third_party/python/.
                cmd = ['find', '.', '-type', 'f', '-name', '*.py[cdo]',
                       '-delete']
            ret = subprocess.call(cmd, cwd=self.topsrcdir)

        if 'gradle' in what:
            shutil.rmtree(mozpath.join(self.topobjdir, 'gradle'))

        return ret

    @property
    def substs(self):
        try:
            return super(Clobber, self).substs
        except BuildEnvironmentNotFoundException:
            return {}


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
        sorted_counts = sorted(type_counts.items(),
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
                print('%s:%d:%d [%s] %s' % (
                    filename, warning['line'], warning['column'],
                    warning['flag'], warning['message']))
            else:
                print('%s:%d [%s] %s' % (filename, warning['line'],
                                         warning['flag'], warning['message']))

    def join_ensure_dir(self, dir1, dir2):
        dir1 = mozpath.normpath(dir1)
        dir2 = mozpath.normsep(dir2)
        joined_path = mozpath.join(dir1, dir2)
        if os.path.isdir(joined_path):
            return joined_path
        print('Specified directory not found.')
        return None


@CommandProvider
class GTestCommands(MachCommandBase):
    @Command('gtest', category='testing',
             description='Run GTest unit tests (C++ tests).')
    @CommandArgument('gtest_filter', default=b"*", nargs='?', metavar='gtest_filter',
                     help="test_filter is a ':'-separated list of wildcard patterns "
                     "(called the positive patterns), optionally followed by a '-' "
                     "and another ':'-separated pattern list (called the negative patterns).")
    @CommandArgument('--jobs', '-j', default='1', nargs='?', metavar='jobs', type=int,
                     help='Run the tests in parallel using multiple processes.')
    @CommandArgument('--tbpl-parser', '-t', action='store_true',
                     help='Output test results in a format that can be parsed by TBPL.')
    @CommandArgument('--shuffle', '-s', action='store_true',
                     help='Randomize the execution order of tests.')
    @CommandArgument('--enable-webrender', action='store_true',
                     default=False, dest='enable_webrender',
                     help='Enable the WebRender compositor in Gecko.')
    @CommandArgumentGroup('Android')
    @CommandArgument('--package',
                     default='org.mozilla.geckoview.test',
                     group='Android',
                     help='Package name of test app.')
    @CommandArgument('--adbpath',
                     dest='adb_path',
                     group='Android',
                     help='Path to adb binary.')
    @CommandArgument('--deviceSerial',
                     dest='device_serial',
                     group='Android',
                     help="adb serial number of remote device. "
                     "Required when more than one device is connected to the host. "
                     "Use 'adb devices' to see connected devices.")
    @CommandArgument('--remoteTestRoot',
                     dest='remote_test_root',
                     group='Android',
                     help='Remote directory to use as test root '
                     '(eg. /mnt/sdcard/tests or /data/local/tests).')
    @CommandArgument('--libxul',
                     dest='libxul_path',
                     group='Android',
                     help='Path to gtest libxul.so.')
    @CommandArgument('--no-install', action='store_true',
                     default=False,
                     group='Android',
                     help='Skip the installation of the APK.')
    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
                     help='Enable the debugger. Not specifying a --debugger option will result in '
                     'the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
                     help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
                     group='debugging',
                     help='Command-line arguments to pass to the debugger itself; '
                     'split as the Bourne shell would.')
    def gtest(self, shuffle, jobs, gtest_filter, tbpl_parser, enable_webrender,
              package, adb_path, device_serial, remote_test_root, libxul_path, no_install,
              debug, debugger, debugger_args):

        # We lazy build gtest because it's slow to link
        try:
            self.config_environment
        except Exception:
            print("Please run |./mach build| before |./mach gtest|.")
            return 1

        res = self._mach_context.commands.dispatch('build', self._mach_context,
                                                   what=['recurse_gtest'])
        if res:
            print("Could not build xul-gtest")
            return res

        if self.substs.get('MOZ_WIDGET_TOOLKIT') == 'cocoa':
            self._run_make(directory='browser/app', target='repackage',
                           ensure_exit_code=True)

        cwd = os.path.join(self.topobjdir, '_tests', 'gtest')

        if not os.path.isdir(cwd):
            os.makedirs(cwd)

        if conditions.is_android(self):
            if jobs != 1:
                print("--jobs is not supported on Android and will be ignored")
            if debug or debugger or debugger_args:
                print("--debug options are not supported on Android and will be ignored")
            from mozrunner.devices.android_device import InstallIntent
            return self.android_gtest(cwd, shuffle, gtest_filter,
                                      package, adb_path, device_serial,
                                      remote_test_root, libxul_path,
                                      enable_webrender,
                                      InstallIntent.NO if no_install else InstallIntent.YES)

        if package or adb_path or device_serial or remote_test_root or libxul_path or no_install:
            print("One or more Android-only options will be ignored")

        app_path = self.get_binary_path('app')
        args = [app_path, '-unittest', '--gtest_death_test_style=threadsafe']

        if sys.platform.startswith('win') and \
                'MOZ_LAUNCHER_PROCESS' in self.defines:
            args.append('--wait-for-browser')

        if debug or debugger or debugger_args:
            args = self.prepend_debugger_args(args, debugger, debugger_args)
            if not args:
                return 1

        # Use GTest environment variable to control test execution
        # For details see:
        # https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_Test_Programs:_Advanced_Options
        gtest_env = {b'GTEST_FILTER': gtest_filter}

        # Note: we must normalize the path here so that gtest on Windows sees
        # a MOZ_GMP_PATH which has only Windows dir seperators, because
        # nsIFile cannot open the paths with non-Windows dir seperators.
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

        if enable_webrender:
            gtest_env[b"MOZ_WEBRENDER"] = b"1"
            gtest_env[b"MOZ_ACCELERATED"] = b"1"
        else:
            gtest_env[b"MOZ_WEBRENDER"] = b"0"

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
                                               processOutputLine=[
                                                   functools.partial(handle_line, i)],
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

    def android_gtest(self, test_dir, shuffle, gtest_filter,
                      package, adb_path, device_serial, remote_test_root, libxul_path,
                      enable_webrender, install):
        # setup logging for mozrunner
        from mozlog.commandline import setup_logging
        format_args = {'level': self._mach_context.settings['test']['level']}
        default_format = self._mach_context.settings['test']['format']
        setup_logging('mach-gtest', {}, {default_format: sys.stdout}, format_args)

        # ensure that a device is available and test app is installed
        from mozrunner.devices.android_device import (verify_android_device, get_adb_path)
        verify_android_device(self, install=install, app=package, device_serial=device_serial)

        if not adb_path:
            adb_path = get_adb_path(self)
        if not libxul_path:
            libxul_path = os.path.join(self.topobjdir, "dist", "bin", "gtest", "libxul.so")

        # run gtest via remotegtests.py
        exit_code = 0
        import imp
        path = os.path.join('testing', 'gtest', 'remotegtests.py')
        with open(path, 'r') as fh:
            imp.load_module('remotegtests', fh, path,
                            ('.py', 'r', imp.PY_SOURCE))
        import remotegtests
        tester = remotegtests.RemoteGTests()
        if not tester.run_gtest(test_dir, shuffle, gtest_filter, package, adb_path, device_serial,
                                remote_test_root, libxul_path, None, enable_webrender):
            exit_code = 1
        tester.cleanup()

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

        if not debugger or not debuggerInfo:
            print("Could not find a suitable debugger in your PATH.")
            return None

        # Parameters come from the CLI. We need to convert them before
        # their use.
        if debugger_args:
            from mozbuild import shellutil
            try:
                debugger_args = shellutil.split(debugger_args)
            except shellutil.MetaCharacterException as e:
                print("The --debugger_args you passed require a real shell to parse them.")
                print("(We can't handle the %r character.)" % e.char)
                return None

        # Prepend the debugger args.
        args = [debuggerInfo.path] + debuggerInfo.args + args
        return args


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


def _get_android_install_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', default='org.mozilla.geckoview_example',
                        help='Android package to install '
                             '(default: org.mozilla.geckoview_example)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Print verbose output when installing.')
    return parser


def setup_install_parser():
    build = MozbuildObject.from_environment(cwd=here)
    if conditions.is_android(build):
        return _get_android_install_parser()
    return argparse.ArgumentParser()


@CommandProvider
class Install(MachCommandBase):
    """Install a package."""

    @Command('install', category='post-build',
             conditions=[conditions.has_build],
             parser=setup_install_parser,
             description='Install the package on the machine (or device in the case of Android).')
    def install(self, **kwargs):
        if conditions.is_android(self):
            from mozrunner.devices.android_device import (verify_android_device, InstallIntent)
            ret = verify_android_device(self, install=InstallIntent.YES, **kwargs) == 0
        else:
            ret = self._run_make(directory=".", target='install', ensure_exit_code=False)

        if ret == 0:
            self.notify('Install complete')
        return ret


@SettingsProvider
class RunSettings():
    config_settings = [
        ('runprefs.*', 'string', """
Pass a pref into Firefox when using `mach run`, of the form `foo.bar=value`.
Prefs will automatically be cast into the appropriate type. Integers can be
single quoted to force them to be strings.
""".strip()),
    ]


def _get_android_run_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', default='org.mozilla.geckoview_example',
                        help='Android package to run '
                             '(default: org.mozilla.geckoview_example)')
    parser.add_argument('--intent', default='android.intent.action.VIEW',
                        help='Android intent action to launch with '
                             '(default: android.intent.action.VIEW)')
    parser.add_argument('--setenv', dest='env', action='append', default=[],
                        help='Set target environment variable, like FOO=BAR')
    parser.add_argument('--profile', '-P', default=None,
                        help='Path to Gecko profile, like /path/to/host/profile '
                             'or /path/to/target/profile')
    parser.add_argument('--url', default=None,
                        help='URL to open')
    parser.add_argument('--no-install', action='store_true', default=False,
                        help='Do not try to install application on device before running '
                             '(default: False)')
    parser.add_argument('--no-wait', action='store_true', default=False,
                        help='Do not wait for application to start before returning '
                             '(default: False)')
    parser.add_argument('--fail-if-running', action='store_true', default=False,
                        help='Fail if application is already running (default: False)')
    parser.add_argument('--restart', action='store_true', default=False,
                        help='Stop the application if it is already running (default: False)')
    return parser


def _get_jsshell_run_parser():
    parser = argparse.ArgumentParser()
    group = parser.add_argument_group('the compiled program')
    group.add_argument('params', nargs='...', default=[],
                       help='Command-line arguments to be passed through to the program. Not '
                       'specifying a --profile or -P option will result in a temporary profile '
                       'being used.')

    group = parser.add_argument_group('debugging')
    group.add_argument('--debug', action='store_true',
                       help='Enable the debugger. Not specifying a --debugger option will result '
                       'in the default debugger being used.')
    group.add_argument('--debugger', default=None, type=str,
                       help='Name of debugger to use.')
    group.add_argument('--debugger-args', default=None, metavar='params', type=str,
                       help='Command-line arguments to pass to the debugger itself; '
                       'split as the Bourne shell would.')
    group.add_argument('--debugparams', action=StoreDebugParamsAndWarnAction,
                       default=None, type=str, dest='debugger_args',
                       help=argparse.SUPPRESS)

    return parser


def _get_desktop_run_parser():
    parser = argparse.ArgumentParser()
    group = parser.add_argument_group('the compiled program')
    group.add_argument('params', nargs='...', default=[],
                       help='Command-line arguments to be passed through to the program. Not '
                       'specifying a --profile or -P option will result in a temporary profile '
                       'being used.')
    group.add_argument('--remote', '-r', action='store_true',
                       help='Do not pass the --no-remote argument by default.')
    group.add_argument('--background', '-b', action='store_true',
                       help='Do not pass the --foreground argument by default on Mac.')
    group.add_argument('--noprofile', '-n', action='store_true',
                       help='Do not pass the --profile argument by default.')
    group.add_argument('--disable-e10s', action='store_true',
                       help='Run the program with electrolysis disabled.')
    group.add_argument('--enable-crash-reporter', action='store_true',
                       help='Run the program with the crash reporter enabled.')
    group.add_argument('--setpref', action='append', default=[],
                       help='Set the specified pref before starting the program. Can be set '
                       'multiple times. Prefs can also be set in ~/.mozbuild/machrc in the '
                       '[runprefs] section - see `./mach settings` for more information.')
    group.add_argument('--temp-profile', action='store_true',
                       help='Run the program using a new temporary profile created inside '
                       'the objdir.')
    group.add_argument('--macos-open', action='store_true',
                       help="On macOS, run the program using the open(1) command. Per open(1), "
                       "the browser is launched \"just as if you had double-clicked the file's "
                       "icon\". The browser can not be launched under a debugger with this "
                       "option.")

    group = parser.add_argument_group('debugging')
    group.add_argument('--debug', action='store_true',
                       help='Enable the debugger. Not specifying a --debugger option will result '
                       'in the default debugger being used.')
    group.add_argument('--debugger', default=None, type=str,
                       help='Name of debugger to use.')
    group.add_argument('--debugger-args', default=None, metavar='params', type=str,
                       help='Command-line arguments to pass to the debugger itself; '
                       'split as the Bourne shell would.')
    group.add_argument('--debugparams', action=StoreDebugParamsAndWarnAction,
                       default=None, type=str, dest='debugger_args',
                       help=argparse.SUPPRESS)

    group = parser.add_argument_group('DMD')
    group.add_argument('--dmd', action='store_true',
                       help='Enable DMD. The following arguments have no effect without this.')
    group.add_argument('--mode', choices=['live', 'dark-matter', 'cumulative', 'scan'],
                       help='Profiling mode. The default is \'dark-matter\'.')
    group.add_argument('--stacks', choices=['partial', 'full'],
                       help='Allocation stack trace coverage. The default is \'partial\'.')
    group.add_argument('--show-dump-stats', action='store_true',
                       help='Show stats when doing dumps.')

    return parser


def setup_run_parser():
    build = MozbuildObject.from_environment(cwd=here)
    if conditions.is_android(build):
        return _get_android_run_parser()
    if conditions.is_jsshell(build):
        return _get_jsshell_run_parser()
    return _get_desktop_run_parser()


@CommandProvider
class RunProgram(MachCommandBase):
    """Run the compiled program."""

    @Command('run', category='post-build',
             conditions=[conditions.has_build_or_shell],
             parser=setup_run_parser,
             description='Run the compiled program, possibly under a debugger or DMD.')
    def run(self, **kwargs):
        if conditions.is_android(self):
            return self._run_android(**kwargs)
        if conditions.is_jsshell(self):
            return self._run_jsshell(**kwargs)
        return self._run_desktop(**kwargs)

    def _run_android(self, app='org.mozilla.geckoview_example', intent=None, env=[], profile=None,
                     url=None, no_install=None, no_wait=None, fail_if_running=None, restart=None):
        from mozrunner.devices.android_device import (verify_android_device,
                                                      _get_device,
                                                      InstallIntent)
        from six.moves import shlex_quote

        if app == 'org.mozilla.geckoview_example':
            activity_name = 'org.mozilla.geckoview_example.GeckoViewActivity'
        elif app == 'org.mozilla.geckoview.test':
            activity_name = 'org.mozilla.geckoview.test.TestRunnerActivity'
        elif 'fennec' in app or 'firefox' in app:
            activity_name = 'org.mozilla.gecko.BrowserApp'
        else:
            raise RuntimeError('Application not recognized: {}'.format(app))

        # `verify_android_device` respects `DEVICE_SERIAL` if it is set and sets it otherwise.
        verify_android_device(self, app=app,
                              install=InstallIntent.NO if no_install else InstallIntent.YES)
        device_serial = os.environ.get('DEVICE_SERIAL')
        if not device_serial:
            print('No ADB devices connected.')
            return 1

        device = _get_device(self.substs, device_serial=device_serial)

        args = []
        if profile:
            if os.path.isdir(profile):
                host_profile = profile
                # Always /data/local/tmp, rather than `device.test_root`, because GeckoView only
                # takes its configuration file from /data/local/tmp, and we want to follow suit.
                target_profile = '/data/local/tmp/{}-profile'.format(app)
                device.rm(target_profile, recursive=True, force=True)
                device.push(host_profile, target_profile)
                self.log(logging.INFO, "run",
                         {'host_profile': host_profile, 'target_profile': target_profile},
                         'Pushed profile from host "{host_profile}" to target "{target_profile}"')
            else:
                target_profile = profile
                self.log(logging.INFO, "run",
                         {'target_profile': target_profile},
                         'Using profile from target "{target_profile}"')

            args = ['--profile', shlex_quote(target_profile)]

        extras = {}
        for i, e in enumerate(env):
            extras['env{}'.format(i)] = e
        if args:
            extras['args'] = " ".join(args)
        extras['use_multiprocess'] = True  # Only GVE and TRA process this extra.

        if env or args:
            restart = True

        if restart:
            fail_if_running = False
            self.log(logging.INFO, "run",
                     {'app': app},
                     'Stopping {app} to ensure clean restart.')
            device.stop_application(app)

        # We'd prefer to log the actual `am start ...` command, but it's not trivial to wire the
        # device's logger to mach's logger.
        self.log(logging.INFO, "run",
                 {'app': app, 'activity_name': activity_name},
                 'Starting {app}/{activity_name}.')

        device.launch_application(
            app_name=app,
            activity_name=activity_name,
            intent=intent,
            extras=extras,
            url=url,
            wait=not no_wait,
            fail_if_running=fail_if_running)

        return 0

    def _run_jsshell(self, params, debug, debugger, debugger_args):
        try:
            binpath = self.get_binary_path('app')
        except BinaryNotFoundException as e:
            self.log(logging.ERROR, 'run',
                     {'error': str(e)},
                     'ERROR: {error}')
            self.log(logging.INFO, 'run',
                     {'help': e.help()},
                     '{help}')
            return 1

        args = [binpath]

        if params:
            args.extend(params)

        extra_env = {
            'RUST_BACKTRACE': 'full',
        }

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

            if not debugger or not self.debuggerInfo:
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

        return self.run_process(args=args, ensure_exit_code=False,
                                pass_thru=True, append_env=extra_env)

    def _run_desktop(self, params, remote, background, noprofile, disable_e10s,
                     enable_crash_reporter, setpref, temp_profile, macos_open, debug, debugger,
                     debugger_args, dmd, mode, stacks, show_dump_stats):
        from mozprofile import Profile, Preferences

        try:
            binpath = self.get_binary_path('app')
        except BinaryNotFoundException as e:
            self.log(logging.ERROR, 'run',
                     {'error': str(e)},
                     'ERROR: {error}')
            self.log(logging.INFO, 'run',
                     {'help': e.help()},
                     '{help}')
            return 1

        args = []
        if macos_open:
            if debug:
                print("The browser can not be launched in the debugger "
                      "when using the macOS open command.")
                return 1
            try:
                m = re.search(r'^.+\.app', binpath)
                apppath = m.group(0)
                args = ['open', apppath, '--args']
            except Exception as e:
                print("Couldn't get the .app path from the binary path. "
                      "The macOS open option can only be used on macOS")
                print(e)
                return 1
        else:
            args = [binpath]

        if params:
            args.extend(params)

        if not remote:
            args.append('-no-remote')

        if not background and sys.platform == 'darwin':
            args.append('-foreground')

        if sys.platform.startswith('win') and \
           'MOZ_LAUNCHER_PROCESS' in self.defines:
            args.append('-wait-for-browser')

        no_profile_option_given = \
            all(p not in params for p in ['-profile', '--profile', '-P'])
        if no_profile_option_given and not noprofile:
            prefs = {
               'browser.aboutConfig.showWarning': False,
               'browser.shell.checkDefaultBrowser': False,
               'general.warnOnAboutConfig': False,
            }
            prefs.update(self._mach_context.settings.runprefs)
            prefs.update([p.split('=', 1) for p in setpref])
            for pref in prefs:
                prefs[pref] = Preferences.cast(prefs[pref])

            tmpdir = os.path.join(self.topobjdir, 'tmp')
            if not os.path.exists(tmpdir):
                os.makedirs(tmpdir)

            if (temp_profile):
                path = tempfile.mkdtemp(dir=tmpdir, prefix='profile-')
            else:
                path = os.path.join(tmpdir, 'profile-default')

            profile = Profile(path, preferences=prefs)
            args.append('-profile')
            args.append(profile.profile)

        if not no_profile_option_given and setpref:
            print("setpref is only supported if a profile is not specified")
            return 1

            if not no_profile_option_given:
                # The profile name may be non-ascii, but come from the
                # commandline as str, so convert here with a better guess at
                # an encoding than the default.
                encoding = (sys.getfilesystemencoding() or
                            sys.getdefaultencoding())
                args = [unicode(a, encoding) if not isinstance(a, unicode) else a
                        for a in args]

        extra_env = {
            'MOZ_DEVELOPER_REPO_DIR': self.topsrcdir,
            'MOZ_DEVELOPER_OBJ_DIR': self.topobjdir,
            'RUST_BACKTRACE': 'full',
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

            if not debugger or not self.debuggerInfo:
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

            if dmd_params:
                extra_env['DMD'] = ' '.join(dmd_params)
            else:
                extra_env['DMD'] = '1'

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
                     choices=['pretty', 'json'],
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
                description='Vendor av1 video codec reference implementation into the '
                'source repository.')
    @CommandArgument('-r', '--revision',
                     help='Repository tag or commit to update to.')
    @CommandArgument('--repo',
                     help='Repository url to pull a snapshot from. '
                     'Supports github and googlesource.')
    @CommandArgument('--ignore-modified', action='store_true',
                     help='Ignore modified files in current checkout',
                     default=False)
    def vendor_aom(self, **kwargs):
        from mozbuild.vendor_aom import VendorAOM
        vendor_command = self._spawn(VendorAOM)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'dav1d',
                description='Vendor dav1d implementation of AV1 into the source repository.')
    @CommandArgument('-r', '--revision',
                     help='Repository tag or commit to update to.')
    @CommandArgument('--repo',
                     help='Repository url to pull a snapshot from. Supports gitlab.')
    @CommandArgument('--ignore-modified', action='store_true',
                     help='Ignore modified files in current checkout',
                     default=False)
    def vendor_dav1d(self, **kwargs):
        from mozbuild.vendor_dav1d import VendorDav1d
        vendor_command = self._spawn(VendorDav1d)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'python',
                description='Vendor Python packages from pypi.org into third_party/python')
    @CommandArgument('--with-windows-wheel', action='store_true',
                     help='Vendor a wheel for Windows along with the source package',
                     default=False)
    @CommandArgument('packages', default=None, nargs='*',
                     help='Packages to vendor. If omitted, packages and their dependencies '
                     'defined in Pipfile.lock will be vendored. If Pipfile has been modified, '
                     'then Pipfile.lock will be regenerated. Note that transient dependencies '
                     'may be updated when running this command.')
    def vendor_python(self, **kwargs):
        from mozbuild.vendor_python import VendorPython
        vendor_command = self._spawn(VendorPython)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'manifest',
                description='Vendor externally hosted repositories into this '
                            'repository.')
    @CommandArgument('files', nargs='+',
                     help='Manifest files to work on')
    @CommandArgumentGroup('verify')
    @CommandArgument('--verify', '-v', action='store_true', group='verify',
                     required=True, help='Verify manifest')
    def vendor_manifest(self, files, verify):
        from mozbuild.vendor_manifest import verify_manifests
        verify_manifests(files)


@CommandProvider
class WebRTCGTestCommands(GTestCommands):
    @Command('webrtc-gtest', category='testing',
             description='Run WebRTC.org GTest unit tests.')
    @CommandArgument('gtest_filter', default=b"*", nargs='?', metavar='gtest_filter',
                     help="test_filter is a ':'-separated list of wildcard patterns "
                     "(called the positive patterns), optionally followed by a '-' and "
                     "another ':'-separated pattern list (called the negative patterns).")
    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
                     help='Enable the debugger. Not specifying a --debugger option will '
                     'result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
                     help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
                     group='debugging',
                     help='Command-line arguments to pass to the debugger itself; '
                     'split as the Bourne shell would.')
    def gtest(self, gtest_filter, debug, debugger,
              debugger_args):
        try:
            app_path = self.get_binary_path('webrtc-gtest')
        except BinaryNotFoundException as e:
            self.log(logging.ERROR, 'webrtc-gtest',
                     {'error': str(e)},
                     'ERROR: {error}')
            self.log(logging.INFO, 'webrtc-gtest',
                     {'help': e.help()},
                     '{help}')
            return 1

        args = [app_path]

        if debug or debugger or debugger_args:
            args = self.prepend_debugger_args(args, debugger, debugger_args)
            if not args:
                return 1

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
    @CommandArgument('--package-name', type=str, required=False,
                     help='Name of the package being rebuilt')
    @CommandArgument('--sfx-stub', type=str, required=True,
                     help='Path to the self-extraction stub.')
    @CommandArgument('--use-upx', required=False, action='store_true',
                     help='Run UPX on the self-extraction stub.')
    def repackage_installer(self, tag, setupexe, package, output, package_name, sfx_stub, use_upx):
        from mozbuild.repackaging.installer import repackage_installer
        repackage_installer(
            topsrcdir=self.topsrcdir,
            tag=tag,
            setupexe=setupexe,
            package=package,
            output=output,
            package_name=package_name,
            sfx_stub=sfx_stub,
            use_upx=use_upx,
        )

    @SubCommand('repackage', 'msi',
                description='Repackage into a MSI')
    @CommandArgument('--wsx', type=str, required=True,
                     help='The wsx file used to build the installer')
    @CommandArgument('--version', type=str, required=True,
                     help='The Firefox version used to create the installer')
    @CommandArgument('--locale', type=str, required=True,
                     help='The locale of the installer')
    @CommandArgument('--arch', type=str, required=True,
                     help='The archtecture you are building.')
    @CommandArgument('--setupexe', type=str, required=True,
                     help='setup.exe installer')
    @CommandArgument('--candle', type=str, required=False,
                     help='location of candle binary')
    @CommandArgument('--light', type=str, required=False,
                     help='location of light binary')
    @CommandArgument('--output', '-o', type=str, required=True,
                     help='Output filename')
    def repackage_msi(self, wsx, version, locale, arch, setupexe, candle, light, output):
        from mozbuild.repackaging.msi import repackage_msi
        repackage_msi(
            topsrcdir=self.topsrcdir,
            wsx=wsx,
            version=version,
            locale=locale,
            arch=arch,
            setupexe=setupexe,
            candle=candle,
            light=light,
            output=output,
        )

    @SubCommand('repackage', 'mar',
                description='Repackage into complete MAR file')
    @CommandArgument('--input', '-i', type=str, required=True,
                     help='Input filename')
    @CommandArgument('--mar', type=str, required=True,
                     help='Mar binary path')
    @CommandArgument('--output', '-o', type=str, required=True,
                     help='Output filename')
    @CommandArgument('--format', type=str, default='lzma',
                     choices=('lzma', 'bz2'),
                     help='Mar format')
    @CommandArgument('--arch', type=str, required=True,
                     help='The archtecture you are building.')
    @CommandArgument('--mar-channel-id', type=str,
                     help='Mar channel id')
    def repackage_mar(self, input, mar, output, format, arch, mar_channel_id):
        from mozbuild.repackaging.mar import repackage_mar
        repackage_mar(
            self.topsrcdir,
            input,
            mar,
            output,
            format,
            arch=arch,
            mar_channel_id=mar_channel_id,
        )


@SettingsProvider
class TelemetrySettings():
    config_settings = [
        ('build.telemetry', 'boolean', """
Enable submission of build system telemetry.
        """.strip(), False),
    ]


@CommandProvider
class L10NCommands(MachCommandBase):
    @Command('package-multi-locale', category='post-build',
             description='Package a multi-locale version of the built product '
                         'for distribution as an APK, DMG, etc.')
    @CommandArgument('--locales', metavar='LOCALES', nargs='+',
                     required=True,
                     help='List of locales to package, including "en-US"')
    @CommandArgument('--verbose', action='store_true',
                     help='Log informative status messages.')
    def package_l10n(self, verbose=False, locales=[]):
        backends = self.substs['BUILD_BACKENDS']
        if 'RecursiveMake' not in backends:
            self.log(logging.ERROR, 'package-multi-locale', {'backends': backends},
                     "Multi-locale packaging requires the full (non-artifact) "
                     "'RecursiveMake' build backend; got {backends}.")
            return 1

        if 'en-US' not in locales:
            self.log(logging.WARN, 'package-multi-locale', {'locales': locales},
                     'List of locales does not include default locale "en-US": '
                     '{locales}; adding "en-US"')
            locales.append('en-US')
        locales = list(sorted(locales))

        append_env = {
            'MOZ_CHROME_MULTILOCALE': ' '.join(locales),
        }

        for locale in locales:
            if locale == 'en-US':
                self.log(logging.INFO, 'package-multi-locale', {'locale': locale},
                         'Skipping default locale {locale}')
                continue

            self.log(logging.INFO, 'package-multi-locale', {'locale': locale},
                     'Processing chrome Gecko resources for locale {locale}')
            self.run_process(
                [mozpath.join(self.topsrcdir, 'mach'), 'build', 'chrome-{}'.format(locale)],
                append_env=append_env,
                pass_thru=True,
                ensure_exit_code=True,
                cwd=mozpath.join(self.topsrcdir))

        if self.substs['MOZ_BUILD_APP'] == 'mobile/android':
            self.log(logging.INFO, 'package-multi-locale', {},
                     'Invoking `mach android assemble-app`')
            self.run_process(
                [mozpath.join(self.topsrcdir, 'mach'), 'android', 'assemble-app'],
                append_env=append_env,
                pass_thru=True,
                ensure_exit_code=True,
                cwd=mozpath.join(self.topsrcdir))

        self.log(logging.INFO, 'package-multi-locale', {},
                 'Invoking multi-locale `mach package`')
        self._run_make(
            directory=self.topobjdir,
            target=['package', 'AB_CD=multi'],
            append_env=append_env,
            pass_thru=True,
            ensure_exit_code=True)

        if self.substs['MOZ_BUILD_APP'] == 'mobile/android':
            self.log(logging.INFO, 'package-multi-locale', {},
                     'Invoking `mach android archive-geckoview`')
            self.run_process(
                [mozpath.join(self.topsrcdir, 'mach'), 'android',
                 'archive-geckoview'.format(locale)],
                append_env=append_env,
                pass_thru=True,
                ensure_exit_code=True,
                cwd=mozpath.join(self.topsrcdir))

        return 0
