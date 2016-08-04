# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import mozpack.path as mozpath
import multiprocessing
import os
import subprocess
import sys
import which

from mach.mixin.logging import LoggingMixin
from mach.mixin.process import ProcessExecutionMixin

from mozfile.mozfile import rmtree

from .backend.configenvironment import ConfigEnvironment
from .controller.clobber import Clobberer
from .mozconfig import (
    MozconfigFindException,
    MozconfigLoadException,
    MozconfigLoader,
)
from .util import memoized_property
from .virtualenv import VirtualenvManager


_config_guess_output = []


def ancestors(path):
    """Emit the parent directories of a path."""
    while path:
        yield path
        newpath = os.path.dirname(path)
        if newpath == path:
            break
        path = newpath

def samepath(path1, path2):
    if hasattr(os.path, 'samefile'):
        return os.path.samefile(path1, path2)
    return os.path.normcase(os.path.realpath(path1)) == \
        os.path.normcase(os.path.realpath(path2))

class BadEnvironmentException(Exception):
    """Base class for errors raised when the build environment is not sane."""


class BuildEnvironmentNotFoundException(BadEnvironmentException):
    """Raised when we could not find a build environment."""


class ObjdirMismatchException(BadEnvironmentException):
    """Raised when the current dir is an objdir and doesn't match the mozconfig."""
    def __init__(self, objdir1, objdir2):
        self.objdir1 = objdir1
        self.objdir2 = objdir2

    def __str__(self):
        return "Objdir mismatch: %s != %s" % (self.objdir1, self.objdir2)


class MozbuildObject(ProcessExecutionMixin):
    """Base class providing basic functionality useful to many modules.

    Modules in this package typically require common functionality such as
    accessing the current config, getting the location of the source directory,
    running processes, etc. This classes provides that functionality. Other
    modules can inherit from this class to obtain this functionality easily.
    """
    def __init__(self, topsrcdir, settings, log_manager, topobjdir=None,
                 mozconfig=MozconfigLoader.AUTODETECT):
        """Create a new Mozbuild object instance.

        Instances are bound to a source directory, a ConfigSettings instance,
        and a LogManager instance. The topobjdir may be passed in as well. If
        it isn't, it will be calculated from the active mozconfig.
        """
        self.topsrcdir = mozpath.normsep(topsrcdir)
        self.settings = settings

        self.populate_logger()
        self.log_manager = log_manager

        self._make = None
        self._topobjdir = mozpath.normsep(topobjdir) if topobjdir else topobjdir
        self._mozconfig = mozconfig
        self._config_environment = None
        self._virtualenv_manager = None

    @classmethod
    def from_environment(cls, cwd=None, detect_virtualenv_mozinfo=True):
        """Create a MozbuildObject by detecting the proper one from the env.

        This examines environment state like the current working directory and
        creates a MozbuildObject from the found source directory, mozconfig, etc.

        The role of this function is to identify a topsrcdir, topobjdir, and
        mozconfig file.

        If the current working directory is inside a known objdir, we always
        use the topsrcdir and mozconfig associated with that objdir.

        If the current working directory is inside a known srcdir, we use that
        topsrcdir and look for mozconfigs using the default mechanism, which
        looks inside environment variables.

        If the current Python interpreter is running from a virtualenv inside
        an objdir, we use that as our objdir.

        If we're not inside a srcdir or objdir, an exception is raised.

        detect_virtualenv_mozinfo determines whether we should look for a
        mozinfo.json file relative to the virtualenv directory. This was
        added to facilitate testing. Callers likely shouldn't change the
        default.
        """

        cwd = cwd or os.getcwd()
        topsrcdir = None
        topobjdir = None
        mozconfig = MozconfigLoader.AUTODETECT

        def load_mozinfo(path):
            info = json.load(open(path, 'rt'))
            topsrcdir = info.get('topsrcdir')
            topobjdir = os.path.dirname(path)
            mozconfig = info.get('mozconfig')
            return topsrcdir, topobjdir, mozconfig

        for dir_path in ancestors(cwd):
            # If we find a mozinfo.json, we are in the objdir.
            mozinfo_path = os.path.join(dir_path, 'mozinfo.json')
            if os.path.isfile(mozinfo_path):
                topsrcdir, topobjdir, mozconfig = load_mozinfo(mozinfo_path)
                break

            # We choose an arbitrary file as an indicator that this is a
            # srcdir. We go with ourself because why not!
            our_path = os.path.join(dir_path, 'python', 'mozbuild', 'mozbuild', 'base.py')
            if os.path.isfile(our_path):
                topsrcdir = dir_path
                break

        # See if we're running from a Python virtualenv that's inside an objdir.
        mozinfo_path = os.path.join(os.path.dirname(sys.prefix), "mozinfo.json")
        if detect_virtualenv_mozinfo and os.path.isfile(mozinfo_path):
            topsrcdir, topobjdir, mozconfig = load_mozinfo(mozinfo_path)

        # If we were successful, we're only guaranteed to find a topsrcdir. If
        # we couldn't find that, there's nothing we can do.
        if not topsrcdir:
            raise BuildEnvironmentNotFoundException(
                'Could not find Mozilla source tree or build environment.')

        topsrcdir = mozpath.normsep(topsrcdir)
        if topobjdir:
            topobjdir = mozpath.normsep(os.path.normpath(topobjdir))

            if topsrcdir == topobjdir:
                raise BadEnvironmentException('The object directory appears '
                    'to be the same as your source directory (%s). This build '
                    'configuration is not supported.' % topsrcdir)

        # If we can't resolve topobjdir, oh well. We'll figure out when we need
        # one.
        return cls(topsrcdir, None, None, topobjdir=topobjdir,
                   mozconfig=mozconfig)

    def resolve_mozconfig_topobjdir(self, default=None):
        topobjdir = self.mozconfig['topobjdir'] or default
        if not topobjdir:
            return None

        if '@CONFIG_GUESS@' in topobjdir:
            topobjdir = topobjdir.replace('@CONFIG_GUESS@',
                self.resolve_config_guess())

        if not os.path.isabs(topobjdir):
            topobjdir = os.path.abspath(os.path.join(self.topsrcdir, topobjdir))

        return mozpath.normsep(os.path.normpath(topobjdir))

    @property
    def topobjdir(self):
        if self._topobjdir is None:
            self._topobjdir = self.resolve_mozconfig_topobjdir(
                default='obj-@CONFIG_GUESS@')

        return self._topobjdir

    @property
    def virtualenv_manager(self):
        if self._virtualenv_manager is None:
            self._virtualenv_manager = VirtualenvManager(self.topsrcdir,
                self.topobjdir, os.path.join(self.topobjdir, '_virtualenv'),
                sys.stdout, os.path.join(self.topsrcdir, 'build',
                'virtualenv_packages.txt'))

        return self._virtualenv_manager

    @property
    def mozconfig(self):
        """Returns information about the current mozconfig file.

        This a dict as returned by MozconfigLoader.read_mozconfig()
        """
        if not isinstance(self._mozconfig, dict):
            loader = MozconfigLoader(self.topsrcdir)
            self._mozconfig = loader.read_mozconfig(path=self._mozconfig,
                moz_build_app=os.environ.get('MOZ_CURRENT_PROJECT'))

        return self._mozconfig

    @property
    def config_environment(self):
        """Returns the ConfigEnvironment for the current build configuration.

        This property is only available once configure has executed.

        If configure's output is not available, this will raise.
        """
        if self._config_environment:
            return self._config_environment

        config_status = os.path.join(self.topobjdir, 'config.status')

        if not os.path.exists(config_status):
            raise BuildEnvironmentNotFoundException('config.status not available. Run configure.')

        self._config_environment = \
            ConfigEnvironment.from_config_status(config_status)

        return self._config_environment

    @property
    def defines(self):
        return self.config_environment.defines

    @property
    def non_global_defines(self):
        return self.config_environment.non_global_defines

    @property
    def substs(self):
        return self.config_environment.substs

    @property
    def distdir(self):
        return os.path.join(self.topobjdir, 'dist')

    @property
    def bindir(self):
        return os.path.join(self.topobjdir, 'dist', 'bin')

    @property
    def includedir(self):
        return os.path.join(self.topobjdir, 'dist', 'include')

    @property
    def statedir(self):
        return os.path.join(self.topobjdir, '.mozbuild')

    @memoized_property
    def extra_environment_variables(self):
        '''Some extra environment variables are stored in .mozconfig.mk.
        This functions extracts and returns them.'''
        from mozbuild import shellutil
        mozconfig_mk = os.path.join(self.topobjdir, '.mozconfig.mk')
        env = {}
        with open(mozconfig_mk) as fh:
            for line in fh:
                if line.startswith('export '):
                    exports = shellutil.split(line)[1:]
                    for e in exports:
                        if '=' in e:
                            key, value = e.split('=')
                            env[key] = value
        return env

    def is_clobber_needed(self):
        if not os.path.exists(self.topobjdir):
            return False
        return Clobberer(self.topsrcdir, self.topobjdir).clobber_needed()

    def have_winrm(self):
        # `winrm -h` should print 'winrm version ...' and exit 1
        try:
            p = subprocess.Popen(['winrm.exe', '-h'],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT)
            return p.wait() == 1 and p.stdout.read().startswith('winrm')
        except:
            return False

    def remove_objdir(self):
        """Remove the entire object directory."""

        if sys.platform.startswith('win') and self.have_winrm():
            subprocess.check_call(['winrm', '-rf', self.topobjdir])
        else:
            # We use mozfile because it is faster than shutil.rmtree().
            # mozfile doesn't like unicode arguments (bug 818783).
            rmtree(self.topobjdir.encode('utf-8'))

    def get_binary_path(self, what='app', validate_exists=True, where='default'):
        """Obtain the path to a compiled binary for this build configuration.

        The what argument is the program or tool being sought after. See the
        code implementation for supported values.

        If validate_exists is True (the default), we will ensure the found path
        exists before returning, raising an exception if it doesn't.

        If where is 'staged-package', we will return the path to the binary in
        the package staging directory.

        If no arguments are specified, we will return the main binary for the
        configured XUL application.
        """

        if where not in ('default', 'staged-package'):
            raise Exception("Don't know location %s" % where)

        substs = self.substs

        stem = self.distdir
        if where == 'staged-package':
            stem = os.path.join(stem, substs['MOZ_APP_NAME'])

        if substs['OS_ARCH'] == 'Darwin':
            if substs['MOZ_BUILD_APP'] == 'xulrunner':
                stem = os.path.join(stem, 'XUL.framework');
            else:
                stem = os.path.join(stem, substs['MOZ_MACBUNDLE_NAME'], 'Contents',
                    'MacOS')
        elif where == 'default':
            stem = os.path.join(stem, 'bin')

        leaf = None

        leaf = (substs['MOZ_APP_NAME'] if what == 'app' else what) + substs['BIN_SUFFIX']
        path = os.path.join(stem, leaf)

        if validate_exists and not os.path.exists(path):
            raise Exception('Binary expected at %s does not exist.' % path)

        return path

    def resolve_config_guess(self):
        make_extra = self.mozconfig['make_extra'] or []
        make_extra = dict(m.split('=', 1) for m in make_extra)

        config_guess = make_extra.get('CONFIG_GUESS', None)

        if config_guess:
            return config_guess

        # config.guess results should be constant for process lifetime. Cache
        # it.
        if _config_guess_output:
            return _config_guess_output[0]

        p = os.path.join(self.topsrcdir, 'build', 'autoconf', 'config.guess')

        # This is a little kludgy. We need access to the normalize_command
        # function. However, that's a method of a mach mixin, so we need a
        # class instance. Ideally the function should be accessible as a
        # standalone function.
        o = MozbuildObject(self.topsrcdir, None, None, None)
        args = o._normalize_command([p], True)

        _config_guess_output.append(
                subprocess.check_output(args, cwd=self.topsrcdir).strip())
        return _config_guess_output[0]

    def notify(self, msg):
        """Show a desktop notification with the supplied message

        On Linux and Mac, this will show a desktop notification with the message,
        but on Windows we can only flash the screen.
        """
        moz_nospam = os.environ.get('MOZ_NOSPAM')
        if moz_nospam:
            return

        try:
            if sys.platform.startswith('darwin'):
                try:
                    notifier = which.which('terminal-notifier')
                except which.WhichError:
                    raise Exception('Install terminal-notifier to get '
                        'a notification when the build finishes.')
                self.run_process([notifier, '-title',
                    'Mozilla Build System', '-group', 'mozbuild',
                    '-message', msg], ensure_exit_code=False)
            elif sys.platform.startswith('linux'):
                try:
                    import dbus
                except ImportError:
                    raise Exception('Install the python dbus module to '
                        'get a notification when the build finishes.')
                bus = dbus.SessionBus()
                notify = bus.get_object('org.freedesktop.Notifications',
                                        '/org/freedesktop/Notifications')
                method = notify.get_dbus_method('Notify',
                                                'org.freedesktop.Notifications')
                method('Mozilla Build System', 0, '', msg, '', [], [], -1)
            elif sys.platform.startswith('win'):
                from ctypes import Structure, windll, POINTER, sizeof
                from ctypes.wintypes import DWORD, HANDLE, WINFUNCTYPE, BOOL, UINT
                class FLASHWINDOW(Structure):
                    _fields_ = [("cbSize", UINT),
                                ("hwnd", HANDLE),
                                ("dwFlags", DWORD),
                                ("uCount", UINT),
                                ("dwTimeout", DWORD)]
                FlashWindowExProto = WINFUNCTYPE(BOOL, POINTER(FLASHWINDOW))
                FlashWindowEx = FlashWindowExProto(("FlashWindowEx", windll.user32))
                FLASHW_CAPTION = 0x01
                FLASHW_TRAY = 0x02
                FLASHW_TIMERNOFG = 0x0C

                # GetConsoleWindows returns NULL if no console is attached. We
                # can't flash nothing.
                console = windll.kernel32.GetConsoleWindow()
                if not console:
                    return

                params = FLASHWINDOW(sizeof(FLASHWINDOW),
                                    console,
                                    FLASHW_CAPTION | FLASHW_TRAY | FLASHW_TIMERNOFG, 3, 0)
                FlashWindowEx(params)
        except Exception as e:
            self.log(logging.WARNING, 'notifier-failed', {'error':
                e.message}, 'Notification center failed: {error}')

    def _ensure_objdir_exists(self):
        if os.path.isdir(self.statedir):
            return

        os.makedirs(self.statedir)

    def _ensure_state_subdir_exists(self, subdir):
        path = os.path.join(self.statedir, subdir)

        if os.path.isdir(path):
            return

        os.makedirs(path)

    def _get_state_filename(self, filename, subdir=None):
        path = self.statedir

        if subdir:
            path = os.path.join(path, subdir)

        return os.path.join(path, filename)

    def _wrap_path_argument(self, arg):
        return PathArgument(arg, self.topsrcdir, self.topobjdir)

    def _run_make(self, directory=None, filename=None, target=None, log=True,
            srcdir=False, allow_parallel=True, line_handler=None,
            append_env=None, explicit_env=None, ignore_errors=False,
            ensure_exit_code=0, silent=True, print_directory=True,
            pass_thru=False, num_jobs=0):
        """Invoke make.

        directory -- Relative directory to look for Makefile in.
        filename -- Explicit makefile to run.
        target -- Makefile target(s) to make. Can be a string or iterable of
            strings.
        srcdir -- If True, invoke make from the source directory tree.
            Otherwise, make will be invoked from the object directory.
        silent -- If True (the default), run make in silent mode.
        print_directory -- If True (the default), have make print directories
        while doing traversal.
        """
        self._ensure_objdir_exists()

        args = self._make_path()

        if directory:
            args.extend(['-C', directory.replace(os.sep, '/')])

        if filename:
            args.extend(['-f', filename])

        if num_jobs == 0 and self.mozconfig['make_flags']:
            flags = iter(self.mozconfig['make_flags'])
            for flag in flags:
                if flag == '-j':
                    try:
                        flag = flags.next()
                    except StopIteration:
                        break
                    try:
                        num_jobs = int(flag)
                    except ValueError:
                        args.append(flag)
                elif flag.startswith('-j'):
                    try:
                        num_jobs = int(flag[2:])
                    except (ValueError, IndexError):
                        break
                else:
                    args.append(flag)

        if allow_parallel:
            if num_jobs > 0:
                args.append('-j%d' % num_jobs)
            else:
                args.append('-j%d' % multiprocessing.cpu_count())
        elif num_jobs > 0:
            args.append('MOZ_PARALLEL_BUILD=%d' % num_jobs)

        if ignore_errors:
            args.append('-k')

        if silent:
            args.append('-s')

        # Print entering/leaving directory messages. Some consumers look at
        # these to measure progress.
        if print_directory:
            args.append('-w')

        if isinstance(target, list):
            args.extend(target)
        elif target:
            args.append(target)

        fn = self._run_command_in_objdir

        if srcdir:
            fn = self._run_command_in_srcdir

        append_env = dict(append_env or ())
        append_env[b'MACH'] = '1'

        params = {
            'args': args,
            'line_handler': line_handler,
            'append_env': append_env,
            'explicit_env': explicit_env,
            'log_level': logging.INFO,
            'require_unix_environment': False,
            'ensure_exit_code': ensure_exit_code,
            'pass_thru': pass_thru,

            # Make manages its children, so mozprocess doesn't need to bother.
            # Having mozprocess manage children can also have side-effects when
            # building on Windows. See bug 796840.
            'ignore_children': True,
        }

        if log:
            params['log_name'] = 'make'

        return fn(**params)

    def _make_path(self):
        baseconfig = os.path.join(self.topsrcdir, 'config', 'baseconfig.mk')

        def is_xcode_lisense_error(output):
            return self._is_osx() and 'Agreeing to the Xcode' in output

        def validate_make(make):
            if os.path.exists(baseconfig) and os.path.exists(make):
                cmd = [make, '-f', baseconfig]
                if self._is_windows():
                    cmd.append('HOST_OS_ARCH=WINNT')
                try:
                    subprocess.check_output(cmd, stderr=subprocess.STDOUT)
                except subprocess.CalledProcessError as e:
                    return False, is_xcode_lisense_error(e.output)
                return True, False
            return False, False

        xcode_lisense_error = False
        possible_makes = ['gmake', 'make', 'mozmake', 'gnumake', 'mingw32-make']

        if 'MAKE' in os.environ:
            make = os.environ['MAKE']
            possible_makes.insert(0, make)

        for test in possible_makes:
            if os.path.isabs(test):
                make = test
            else:
                try:
                    make = which.which(test)
                except which.WhichError:
                    continue
            result, xcode_lisense_error_tmp = validate_make(make)
            if result:
                return [make]
            if xcode_lisense_error_tmp:
                xcode_lisense_error = True

        if xcode_lisense_error:
            raise Exception('Xcode requires accepting to the license agreement.\n'
                'Please run Xcode and accept the license agreement.')

        if self._is_windows():
            raise Exception('Could not find a suitable make implementation.\n'
                'Please use MozillaBuild 1.9 or newer')
        else:
            raise Exception('Could not find a suitable make implementation.')

    def _run_command_in_srcdir(self, **args):
        return self.run_process(cwd=self.topsrcdir, **args)

    def _run_command_in_objdir(self, **args):
        return self.run_process(cwd=self.topobjdir, **args)

    def _is_windows(self):
        return os.name in ('nt', 'ce')

    def _is_osx(self):
        return 'darwin' in str(sys.platform).lower()

    def _spawn(self, cls):
        """Create a new MozbuildObject-derived class instance from ourselves.

        This is used as a convenience method to create other
        MozbuildObject-derived class instances. It can only be used on
        classes that have the same constructor arguments as us.
        """

        return cls(self.topsrcdir, self.settings, self.log_manager,
            topobjdir=self.topobjdir)

    def _activate_virtualenv(self):
        self.virtualenv_manager.ensure()
        self.virtualenv_manager.activate()


class MachCommandBase(MozbuildObject):
    """Base class for mach command providers that wish to be MozbuildObjects.

    This provides a level of indirection so MozbuildObject can be refactored
    without having to change everything that inherits from it.
    """

    def __init__(self, context):
        # Attempt to discover topobjdir through environment detection, as it is
        # more reliable than mozconfig when cwd is inside an objdir.
        topsrcdir = context.topdir
        topobjdir = None
        detect_virtualenv_mozinfo = True
        if hasattr(context, 'detect_virtualenv_mozinfo'):
            detect_virtualenv_mozinfo = getattr(context,
                'detect_virtualenv_mozinfo')
        try:
            dummy = MozbuildObject.from_environment(cwd=context.cwd,
                detect_virtualenv_mozinfo=detect_virtualenv_mozinfo)
            topsrcdir = dummy.topsrcdir
            topobjdir = dummy._topobjdir
            if topobjdir:
                # If we're inside a objdir and the found mozconfig resolves to
                # another objdir, we abort. The reasoning here is that if you
                # are inside an objdir you probably want to perform actions on
                # that objdir, not another one. This prevents accidental usage
                # of the wrong objdir when the current objdir is ambiguous.
                config_topobjdir = dummy.resolve_mozconfig_topobjdir()
                if config_topobjdir and not samepath(topobjdir,
                                                     config_topobjdir):
                    raise ObjdirMismatchException(topobjdir, config_topobjdir)
        except BuildEnvironmentNotFoundException:
            pass
        except ObjdirMismatchException as e:
            print('Ambiguous object directory detected. We detected that '
                'both %s and %s could be object directories. This is '
                'typically caused by having a mozconfig pointing to a '
                'different object directory from the current working '
                'directory. To solve this problem, ensure you do not have a '
                'default mozconfig in searched paths.' % (e.objdir1,
                    e.objdir2))
            sys.exit(1)

        except MozconfigLoadException as e:
            print('Error loading mozconfig: ' + e.path)
            print('')
            print(e.message)
            if e.output:
                print('')
                print('mozconfig output:')
                print('')
                for line in e.output:
                    print(line)

            sys.exit(1)

        MozbuildObject.__init__(self, topsrcdir, context.settings,
            context.log_manager, topobjdir=topobjdir)

        self._mach_context = context

        # Incur mozconfig processing so we have unified error handling for
        # errors. Otherwise, the exceptions could bubble back to mach's error
        # handler.
        try:
            self.mozconfig

        except MozconfigFindException as e:
            print(e.message)
            sys.exit(1)

        except MozconfigLoadException as e:
            print('Error loading mozconfig: ' + e.path)
            print('')
            print(e.message)
            if e.output:
                print('')
                print('mozconfig output:')
                print('')
                for line in e.output:
                    print(line)

            sys.exit(1)

        # Always keep a log of the last command, but don't do that for mach
        # invokations from scripts (especially not the ones done by the build
        # system itself).
        if (os.isatty(sys.stdout.fileno()) and
                not getattr(self, 'NO_AUTO_LOG', False)):
            self._ensure_state_subdir_exists('.')
            logfile = self._get_state_filename('last_log.json')
            try:
                fd = open(logfile, "wb")
                self.log_manager.add_json_handler(fd)
            except Exception as e:
                self.log(logging.WARNING, 'mach', {'error': e},
                         'Log will not be kept for this command: {error}.')


class MachCommandConditions(object):
    """A series of commonly used condition functions which can be applied to
    mach commands with providers deriving from MachCommandBase.
    """
    @staticmethod
    def is_firefox(cls):
        """Must have a Firefox build."""
        if hasattr(cls, 'substs'):
            return cls.substs.get('MOZ_BUILD_APP') == 'browser'
        return False

    @staticmethod
    def is_mulet(cls):
        """Must have a Mulet build."""
        if hasattr(cls, 'substs'):
            return cls.substs.get('MOZ_BUILD_APP') == 'b2g/dev'
        return False

    @staticmethod
    def is_b2g(cls):
        """Must have a B2G build."""
        if hasattr(cls, 'substs'):
            return cls.substs.get('MOZ_WIDGET_TOOLKIT') == 'gonk'
        return False

    @staticmethod
    def is_b2g_desktop(cls):
        """Must have a B2G desktop build."""
        if hasattr(cls, 'substs'):
            return cls.substs.get('MOZ_BUILD_APP') == 'b2g' and \
                   cls.substs.get('MOZ_WIDGET_TOOLKIT') != 'gonk'
        return False

    @staticmethod
    def is_emulator(cls):
        """Must have a B2G build with an emulator configured."""
        try:
            return MachCommandConditions.is_b2g(cls) and \
                   cls.device_name.startswith('emulator')
        except AttributeError:
            return False

    @staticmethod
    def is_android(cls):
        """Must have an Android build."""
        if hasattr(cls, 'substs'):
            return cls.substs.get('MOZ_WIDGET_TOOLKIT') == 'android'
        return False

    @staticmethod
    def is_hg(cls):
        """Must have a mercurial source checkout."""
        if hasattr(cls, 'substs'):
            top_srcdir = cls.substs.get('top_srcdir')
            return top_srcdir and os.path.isdir(os.path.join(top_srcdir, '.hg'))
        return False

    @staticmethod
    def is_git(cls):
        """Must have a git source checkout."""
        if hasattr(cls, 'substs'):
            top_srcdir = cls.substs.get('top_srcdir')
            return top_srcdir and os.path.isdir(os.path.join(top_srcdir, '.git'))
        return False


class PathArgument(object):
    """Parse a filesystem path argument and transform it in various ways."""

    def __init__(self, arg, topsrcdir, topobjdir, cwd=None):
        self.arg = arg
        self.topsrcdir = topsrcdir
        self.topobjdir = topobjdir
        self.cwd = os.getcwd() if cwd is None else cwd

    def relpath(self):
        """Return a path relative to the topsrcdir or topobjdir.

        If the argument is a path to a location in one of the base directories
        (topsrcdir or topobjdir), then strip off the base directory part and
        just return the path within the base directory."""

        abspath = os.path.abspath(os.path.join(self.cwd, self.arg))

        # If that path is within topsrcdir or topobjdir, return an equivalent
        # path relative to that base directory.
        for base_dir in [self.topobjdir, self.topsrcdir]:
            if abspath.startswith(os.path.abspath(base_dir)):
                return mozpath.relpath(abspath, base_dir)

        return mozpath.normsep(self.arg)

    def srcdir_path(self):
        return mozpath.join(self.topsrcdir, self.relpath())

    def objdir_path(self):
        return mozpath.join(self.topobjdir, self.relpath())


class ExecutionSummary(dict):
    """Helper for execution summaries."""

    def __init__(self, summary_format, **data):
        self._summary_format = ''
        assert 'execution_time' in data
        self.extend(summary_format, **data)

    def extend(self, summary_format, **data):
        self._summary_format += summary_format
        self.update(data)

    def __str__(self):
        return self._summary_format.format(**self)

    def __getattr__(self, key):
        return self[key]
