# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import logging
import multiprocessing
import os
import subprocess
import sys
import which

from mach.mixin.logging import LoggingMixin
from mach.mixin.process import ProcessExecutionMixin

from mozfile.mozfile import rmtree

from .backend.configenvironment import ConfigEnvironment
from .config import BuildConfig
from .mozconfig import (
    MozconfigFindException,
    MozconfigLoadException,
    MozconfigLoader,
)


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
        self.config = BuildConfig(settings)

        self.populate_logger()
        self.log_manager = log_manager

        self._make = None
        self._topobjdir = topobjdir
        self._mozconfig = None
        self._config_guess_output = None
        self._config_environment = None

    @property
    def topobjdir(self):
        if self._topobjdir is None:
            if self.mozconfig['topobjdir'] is None:
                self._topobjdir = 'obj-%s' % self._config_guess
            else:
                self._topobjdir = self.mozconfig['topobjdir']

        return self._topobjdir

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

    def get_binary_path(self, what='app', validate_exists=True):
        """Obtain the path to a compiled binary for this build configuration.

        The what argument is the program or tool being sought after. See the
        code implementation for supported values.

        If validate_exists is True (the default), we will ensure the found path
        exists before returning, raising an exception if it doesn't.

        If no arguments are specified, we will return the main binary for the
        configured XUL application.
        """

        substs = self.substs

        stem = self.distdir
        if substs['OS_ARCH'] == 'Darwin':
            stem = os.path.join(stem, substs['MOZ_MACBUNDLE_NAME'], 'Contents',
                'MacOS')

        leaf = None

        if what == 'app':
            leaf = substs['MOZ_APP_NAME'] + substs['BIN_SUFFIX']
        elif what == 'xpcshell':
            leaf = 'xpcshell'
        else:
            raise Exception("Don't know how to locate binary: %s" % what)

        path = os.path.join(stem, leaf)

        if validate_exists and not os.path.exists(path):
            raise Exception('Binary expected at %s does not exist.' % path)

        return path

    @property
    def _config_guess(self):
        if self._config_guess_output is None:
            p = os.path.join(self.topsrcdir, 'build', 'autoconf',
                'config.guess')
            args = self._normalize_command([p], True)
            self._config_guess_output = subprocess.check_output(args,
                cwd=self.topsrcdir).strip()

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

    def _get_srcdir_path(self, path):
        """Convert a relative path in the source directory to a full path."""
        return os.path.join(self.topsrcdir, path)

    def _get_objdir_path(self, path):
        """Convert a relative path in the object directory to a full path."""
        return os.path.join(self.topobjdir, path)

    def _run_make(self, directory=None, filename=None, target=None, log=True,
            srcdir=False, allow_parallel=True, line_handler=None,
            append_env=None, explicit_env=None, ignore_errors=False,
            ensure_exit_code=0, silent=True, print_directory=True,
            pass_thru=False):
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

        args = [self._make_path]

        if directory:
            args.extend(['-C', directory])

        if filename:
            args.extend(['-f', filename])

        if allow_parallel:
            args.append('-j%d' % multiprocessing.cpu_count())

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

    @property
    def _make_path(self):
        if self._make is None:
            if self._is_windows():
                self._make = os.path.join(self.topsrcdir, 'build', 'pymake',
                    'make.py')

            else:
                for test in ['gmake', 'make']:
                    try:
                        self._make = which.which(test)
                        break
                    except which.WhichError:
                        continue

        if self._make is None:
            raise Exception('Could not find suitable make binary!')

        return self._make

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


class MachCommandBase(MozbuildObject):
    """Base class for mach command providers that wish to be MozbuildObjects.

    This provides a level of indirection so MozbuildObject can be refactored
    without having to change everything that inherits from it.
    """

    def __init__(self, context):
        MozbuildObject.__init__(self, context.topdir, context.settings,
            context.log_manager)

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

