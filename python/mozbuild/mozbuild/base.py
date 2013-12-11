# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import json
import logging
import mozpack.path
import multiprocessing
import os
import subprocess
import sys
import which

from mach.mixin.logging import LoggingMixin
from mach.mixin.process import ProcessExecutionMixin

from mozfile.mozfile import rmtree

from .backend.configenvironment import ConfigEnvironment
from .mozconfig import (
    MozconfigFindException,
    MozconfigLoadException,
    MozconfigLoader,
)
from .virtualenv import VirtualenvManager


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
    return os.path.normpath(os.path.realpath(path1)) == \
        os.path.normpath(os.path.realpath(path2))

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
    def __init__(self, topsrcdir, settings, log_manager, topobjdir=None):
        """Create a new Mozbuild object instance.

        Instances are bound to a source directory, a ConfigSettings instance,
        and a LogManager instance. The topobjdir may be passed in as well. If
        it isn't, it will be calculated from the active mozconfig.
        """
        self.topsrcdir = topsrcdir
        self.settings = settings

        self.populate_logger()
        self.log_manager = log_manager

        self._make = None
        self._topobjdir = topobjdir
        self._mozconfig = None
        self._config_guess_output = None
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
        use the topsrcdir and mozconfig associated with that objdir. If no
        mozconfig is associated with that objdir, we fall back to looking for
        the mozconfig in the usual places.

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
        mozconfig = None

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

        # Now we try to load the config for this environment. If mozconfig is
        # None, read_mozconfig() will attempt to find one in the existing
        # environment. If no mozconfig is present, the config will not have
        # much defined.
        loader = MozconfigLoader(topsrcdir)
        config = loader.read_mozconfig(mozconfig)

        config_topobjdir = MozbuildObject.resolve_mozconfig_topobjdir(
            topsrcdir, config)

        # If we're inside a objdir and the found mozconfig resolves to
        # another objdir, we abort. The reasoning here is that if you are
        # inside an objdir you probably want to perform actions on that objdir,
        # not another one. This prevents accidental usage of the wrong objdir
        # when the current objdir is ambiguous.
        if topobjdir and config_topobjdir:
            mozilla_dir = os.path.join(config_topobjdir, 'mozilla')
            if not samepath(topobjdir, config_topobjdir) \
                and (os.path.exists(mozilla_dir) and not samepath(topobjdir,
                mozilla_dir)):

                raise ObjdirMismatchException(topobjdir, config_topobjdir)

        topobjdir = topobjdir or config_topobjdir
        if topobjdir:
            topobjdir = os.path.normpath(topobjdir)

            if topsrcdir == topobjdir:
                raise BadEnvironmentException('The object directory appears '
                    'to be the same as your source directory (%s). This build '
                    'configuration is not supported.' % topsrcdir)

        # If we can't resolve topobjdir, oh well. The constructor will figure
        # it out via config.guess.
        return cls(topsrcdir, None, None, topobjdir=topobjdir)

    @staticmethod
    def resolve_mozconfig_topobjdir(topsrcdir, mozconfig, default=None):
        topobjdir = mozconfig['topobjdir'] or default
        if not topobjdir:
            return None

        if '@CONFIG_GUESS@' in topobjdir:
            topobjdir = topobjdir.replace('@CONFIG_GUESS@',
                MozbuildObject.resolve_config_guess(mozconfig, topsrcdir))

        if not os.path.isabs(topobjdir):
            topobjdir = os.path.abspath(os.path.join(topsrcdir, topobjdir))

        return os.path.normpath(topobjdir)

    @property
    def topobjdir(self):
        if self._topobjdir is None:
            self._topobjdir = MozbuildObject.resolve_mozconfig_topobjdir(
                self.topsrcdir, self.mozconfig, default='obj-@CONFIG_GUESS@')

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
        if self._mozconfig is None:
            loader = MozconfigLoader(self.topsrcdir)
            self._mozconfig = loader.read_mozconfig()

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
            raise Exception('config.status not available. Run configure.')

        self._config_environment = \
            ConfigEnvironment.from_config_status(config_status)

        return self._config_environment

    @property
    def defines(self):
        return self.config_environment.defines

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
    def statedir(self):
        return os.path.join(self.topobjdir, '.mozbuild')

    def remove_objdir(self):
        """Remove the entire object directory."""

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

    @staticmethod
    def resolve_config_guess(mozconfig, topsrcdir):
        make_extra = mozconfig['make_extra'] or []
        make_extra = dict(m.split('=', 1) for m in make_extra)

        config_guess = make_extra.get('CONFIG_GUESS', None)

        if config_guess:
            return config_guess

        p = os.path.join(topsrcdir, 'build', 'autoconf', 'config.guess')

        # This is a little kludgy. We need access to the normalize_command
        # function. However, that's a method of a mach mixin, so we need a
        # class instance. Ideally the function should be accessible as a
        # standalone function.
        o = MozbuildObject(topsrcdir, None, None, None)
        args = o._normalize_command([p], True)

        return subprocess.check_output(args, cwd=topsrcdir).strip()

    @property
    def _config_guess(self):
        if self._config_guess_output is None:
            self._config_guess_output = MozbuildObject.resolve_config_guess(
                self.mozconfig, self.topsrcdir)

        return self._config_guess_output

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
            pass_thru=False, num_jobs=0, force_pymake=False):
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
        force_pymake -- If True, pymake will be used instead of GNU make.
        """
        self._ensure_objdir_exists()

        args = self._make_path(force_pymake=force_pymake)

        if directory:
            args.extend(['-C', directory.replace(os.sep, '/')])

        if filename:
            args.extend(['-f', filename])

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
        # these to measure progress. Ideally, we'd do everything with pymake
        # and use hooks in its API. Unfortunately, it doesn't provide that
        # feature... yet.
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
            'require_unix_environment': True,
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

    def _make_path(self, force_pymake=False):
        if self._is_windows() and not force_pymake:
            # Use mozmake if it's available.
            try:
                return [which.which('mozmake')]
            except which.WhichError:
                pass

        if self._is_windows() or force_pymake:
            make_py = os.path.join(self.topsrcdir, 'build', 'pymake',
                'make.py').replace(os.sep, '/')

            # We might want to consider invoking with the virtualenv's Python
            # some day. But, there is a chicken-and-egg problem w.r.t. when the
            # virtualenv is created.
            return [sys.executable, make_py]

        for test in ['gmake', 'make']:
            try:
                return [which.which(test)]
            except which.WhichError:
                continue

        raise Exception('Could not find a suitable make implementation.')

    def _run_command_in_srcdir(self, **args):
        return self.run_process(cwd=self.topsrcdir, **args)

    def _run_command_in_objdir(self, **args):
        return self.run_process(cwd=self.topobjdir, **args)

    def _is_windows(self):
        return os.name in ('nt', 'ce')

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
        try:
            dummy = MozbuildObject.from_environment(cwd=context.cwd)
            topsrcdir = dummy.topsrcdir
            topobjdir = dummy._topobjdir
        except BuildEnvironmentNotFoundException:
            pass

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
    def is_b2g(cls):
        """Must have a Boot to Gecko build."""
        if hasattr(cls, 'substs'):
            return cls.substs.get('MOZ_WIDGET_TOOLKIT') == 'gonk'
        return False

    @staticmethod
    def is_b2g_desktop(cls):
        """Must have a Boot to Gecko desktop build."""
        if hasattr(cls, 'substs'):
            return cls.substs.get('MOZ_BUILD_APP') == 'b2g' and \
                   cls.substs.get('MOZ_WIDGET_TOOLKIT') != 'gonk'
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
                return mozpack.path.relpath(abspath, base_dir)

        return mozpack.path.normsep(self.arg)

    def srcdir_path(self):
        return mozpack.path.join(self.topsrcdir, self.relpath())

    def objdir_path(self):
        return mozpack.path.join(self.topobjdir, self.relpath())
