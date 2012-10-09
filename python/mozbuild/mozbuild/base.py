# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import logging
import multiprocessing
import os
import pymake.parser
import sys
import subprocess
import which

from mozprocess.processhandler import ProcessHandlerMixin
from pymake.data import Makefile

from mozbuild.config import ConfigProvider
from mozbuild.config import PositiveIntegerType


# Perform detection of operating system environment. This is used by command
# execution. We only do this once to save redundancy. Yes, this can fail module
# loading. That is arguably OK.
if 'SHELL' in os.environ:
    _current_shell = os.environ['SHELL']
elif 'MOZILLABUILD' in os.environ:
    _current_shell = os.environ['MOZILLABUILD'] + '/msys/bin/sh.exe'
elif 'COMSPEC' in os.environ:
    _current_shell = os.environ['COMSPEC']
else:
    raise Exception('Could not detect environment shell!')

_in_msys = False

if os.environ.get('MSYSTEM', None) == 'MINGW32':
    _in_msys = True

    if not _current_shell.lower().endswith('.exe'):
        _current_shell += '.exe'


class MozbuildObject(object):
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
        self.logger = logging.getLogger(__name__)
        self.log_manager = log_manager

        self._config_guess_output = None
        self._make = None
        self._topobjdir = topobjdir

    @property
    def topobjdir(self):
        if self._topobjdir is None:
            self._load_mozconfig()

        if self._topobjdir is None:
            self._topobjdir = 'obj-%s' % self._config_guess

        return self._topobjdir

    @property
    def distdir(self):
        return os.path.join(self.topobjdir, 'dist')

    @property
    def bindir(self):
        return os.path.join(self.topobjdir, 'dist', 'bin')

    @property
    def statedir(self):
        return os.path.join(self.topobjdir, '.mozbuild')

    def log(self, level, action, params, format_str):
        self.logger.log(level, format_str,
            extra={'action': action, 'params': params})

    def _load_mozconfig(self, path=None):
        # The mozconfig loader outputs a make file. We parse and load this make
        # file with pymake and evaluate it in a context similar to client.mk.

        loader = os.path.join(self.topsrcdir, 'build', 'autoconf',
            'mozconfig2client-mk')

        # os.environ from a library function is somewhat evil. But, mozconfig
        # files are tightly coupled with the environment by definition. In the
        # future, perhaps we'll have a more sanitized environment for mozconfig
        # execution.
        #
        # The force of str is required because subprocess on Python <2.7.3
        # does not like unicode in environment keys or values. At the time this
        # was written, Mozilla shipped Python 2.7.2 with MozillaBuild.
        env = dict(os.environ)
        if path is not None:
            env[str('MOZCONFIG')] = path

        env[str('CONFIG_GUESS')] = self._config_guess

        args = self._normalize_command([loader, self.topsrcdir], True)

        output = subprocess.check_output(args, stderr=subprocess.PIPE,
            cwd=self.topsrcdir, env=env)

        # The output is make syntax. We parse this in a specialized make
        # context.
        statements = pymake.parser.parsestring(output, 'mozconfig')

        makefile = Makefile(workdir=self.topsrcdir, env={
            'TOPSRCDIR': self.topsrcdir,
            'CONFIG_GUESS': self._config_guess})

        statements.execute(makefile)

        def get_value(name):
            exp = makefile.variables.get(name)[2]

            return exp.resolvestr(makefile, makefile.variables)

        for name, flavor, source, value in makefile.variables:
            # We only care about variables that came from the parsed mozconfig.
            if source != pymake.data.Variables.SOURCE_MAKEFILE:
                continue

            # Ignore some pymake built-ins.
            if name in ('.PYMAKE', 'MAKELEVEL', 'MAKEFLAGS'):
                continue

            if name == 'MOZ_OBJDIR':
                self._topobjdir = get_value(name)

            # If we want to extract other variables defined by mozconfig, here
            # is where we'd do it.

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
            silent=True, print_directory=True):
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
            args.append('-j%d' % self.settings.build.threads)

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
            'ignore_errors': ignore_errors,

            # Make manages its children, so mozprocess doesn't need to bother.
            # Having mozprocess manage children can also have side-effects when
            # building on Windows. See bug 796840.
            'ignore_children': True,
        }

        if log:
            params['log_name'] = 'make'

        fn(**params)

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
        self._run_command(cwd=self.topsrcdir, **args)

    def _run_command_in_objdir(self, **args):
        self._run_command(cwd=self.topobjdir, **args)

    def _run_command(self, args=None, cwd=None, append_env=None,
        explicit_env=None, log_name=None, log_level=logging.INFO,
        line_handler=None, require_unix_environment=False,
        ignore_errors=False, ignore_children=False):
        """Runs a single command to completion.

        Takes a list of arguments to run where the first item is the
        executable. Runs the command in the specified directory and
        with optional environment variables.

        append_env -- Dict of environment variables to append to the current
            set of environment variables.
        explicit_env -- Dict of environment variables to set for the new
            process. Any existing environment variables will be ignored.

        require_unix_environment if True will ensure the command is executed
        within a UNIX environment. Basically, if we are on Windows, it will
        execute the command via an appropriate UNIX-like shell.

        ignore_children is proxied to mozprocess's ignore_children.
        """
        args = self._normalize_command(args, require_unix_environment)

        self.log(logging.INFO, 'process', {'args': args}, ' '.join(args))

        def handleLine(line):
            # Converts str to unicode on Python 2 and bytes to str on Python 3.
            if isinstance(line, bytes):
                line = line.decode(sys.stdout.encoding or 'utf-8', 'replace')

            if line_handler:
                line_handler(line)

            if not log_name:
                return

            self.log(log_level, log_name, {'line': line.strip()}, '{line}')

        use_env = {}
        if explicit_env:
            use_env = explicit_env
        else:
            use_env.update(os.environ)

            if append_env:
                use_env.update(append_env)

        self.log(logging.DEBUG, 'process', {'env': use_env}, 'Environment: {env}')

        p = ProcessHandlerMixin(args, cwd=cwd, env=use_env,
            processOutputLine=[handleLine], universal_newlines=True,
            ignore_children=ignore_children)
        p.run()
        p.processOutput()
        status = p.wait()

        if status != 0 and not ignore_errors:
            raise Exception('Process executed with non-0 exit code: %s' % args)

    def _normalize_command(self, args, require_unix_environment):
        """Adjust command arguments to run in the necessary environment.

        This exists mainly to facilitate execution of programs requiring a *NIX
        shell when running on Windows. The caller specifies whether a shell
        environment is required. If it is and we are running on Windows but
        aren't running in the UNIX-like msys environment, then we rewrite the
        command to execute via a shell.
        """
        assert isinstance(args, list) and len(args)

        if not require_unix_environment or not _in_msys:
            return args

        # Always munge Windows-style into Unix style for the command.
        prog = args[0].replace('\\', '/')

        # PyMake removes the C: prefix. But, things seem to work here
        # without it. Not sure what that's about.

        # We run everything through the msys shell. We need to use
        # '-c' and pass all the arguments as one argument because that is
        # how sh works.
        cline = subprocess.list2cmdline([prog] + args[1:])
        return [_current_shell, '-c', cline]

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


class BuildConfig(ConfigProvider):
    """The configuration for mozbuild."""

    def __init__(self, settings):
        self.settings = settings

    @classmethod
    def _register_settings(cls):
        def register(section, option, type_cls, **kwargs):
            cls.register_setting(section, option, type_cls, domain='mozbuild',
                **kwargs)

        register('build', 'threads', PositiveIntegerType,
            default=multiprocessing.cpu_count())
