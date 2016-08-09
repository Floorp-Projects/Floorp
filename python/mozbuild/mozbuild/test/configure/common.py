# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import errno
import os
import subprocess
import sys
import tempfile
import unittest

from mozbuild.configure import ConfigureSandbox
from mozbuild.util import ReadOnlyNamespace
from mozpack import path as mozpath

from StringIO import StringIO
from which import WhichError

from buildconfig import (
    topobjdir,
    topsrcdir,
)


def ensure_exe_extension(path):
    if sys.platform.startswith('win'):
        return path + '.exe'
    return path


class ConfigureTestVFS(object):
    def __init__(self, paths):
        self._paths = set(mozpath.abspath(p) for p in paths)

    def exists(self, path):
        path = mozpath.abspath(path)
        if path in self._paths:
            return True
        if mozpath.basedir(path, [topsrcdir, topobjdir]):
            return os.path.exists(path)
        return False

    def isfile(self, path):
        path = mozpath.abspath(path)
        if path in self._paths:
            return True
        if mozpath.basedir(path, [topsrcdir, topobjdir]):
            return os.path.isfile(path)
        return False


class ConfigureTestSandbox(ConfigureSandbox):
    '''Wrapper around the ConfigureSandbox for testing purposes.

    Its arguments are the same as ConfigureSandbox, except for the additional
    `paths` argument, which is a dict where the keys are file paths and the
    values are either None or a function that will be called when the sandbox
    calls an implemented function from subprocess with the key as command.
    When the command is CONFIG_SHELL, the function for the path of the script
    that follows will be called.

    The API for those functions is:
        retcode, stdout, stderr = func(stdin, args)

    This class is only meant to implement the minimal things to make
    moz.configure testing possible. As such, it takes shortcuts.
    '''
    def __init__(self, paths, config, environ, *args, **kwargs):
        self._search_path = environ.get('PATH', '').split(os.pathsep)

        self._subprocess_paths = {
            mozpath.abspath(k): v for k, v in paths.iteritems() if v
        }

        paths = paths.keys()

        environ = dict(environ)
        if 'CONFIG_SHELL' not in environ:
            environ['CONFIG_SHELL'] = mozpath.abspath('/bin/sh')
            self._subprocess_paths[environ['CONFIG_SHELL']] = self.shell
            paths.append(environ['CONFIG_SHELL'])
        self._environ = copy.copy(environ)

        vfs = ConfigureTestVFS(paths)

        self.OS = ReadOnlyNamespace(path=ReadOnlyNamespace(**{
            k: v if k not in ('exists', 'isfile')
            else getattr(vfs, k)
            for k, v in ConfigureSandbox.OS.path.__dict__.iteritems()
        }))

        super(ConfigureTestSandbox, self).__init__(config, environ, *args,
                                                   **kwargs)

    def _get_one_import(self, what):
        if what == 'which.which':
            return self.which

        if what == 'which':
            return ReadOnlyNamespace(
                which=self.which,
                WhichError=WhichError,
            )

        if what == 'subprocess.Popen':
            return self.Popen

        if what == 'subprocess':
            return ReadOnlyNamespace(
                CalledProcessError=subprocess.CalledProcessError,
                check_output=self.check_output,
                PIPE=subprocess.PIPE,
                STDOUT=subprocess.STDOUT,
                Popen=self.Popen,
            )

        if what == 'os.environ':
            return self._environ

        if what == 'ctypes.wintypes':
            return ReadOnlyNamespace(
                LPCWSTR=0,
                LPWSTR=1,
                DWORD=2,
            )

        if what == 'ctypes':
            class CTypesFunc(object):
                def __init__(self, func):
                    self._func = func

                def __call__(self, *args, **kwargs):
                    return self._func(*args, **kwargs)


            return ReadOnlyNamespace(
                create_unicode_buffer=self.create_unicode_buffer,
                windll=ReadOnlyNamespace(
                    kernel32=ReadOnlyNamespace(
                        GetShortPathNameW=CTypesFunc(self.GetShortPathNameW),
                    )
                ),
            )

        if what == '_winreg':
            def OpenKey(*args, **kwargs):
                raise WindowsError()

            return ReadOnlyNamespace(
                HKEY_LOCAL_MACHINE=0,
                OpenKey=OpenKey,
            )

        return super(ConfigureTestSandbox, self)._get_one_import(what)

    def create_unicode_buffer(self, *args, **kwargs):
        class Buffer(object):
            def __init__(self):
                self.value = ''

        return Buffer()

    def GetShortPathNameW(self, path_in, path_out, length):
        path_out.value = path_in
        return length

    def which(self, command, path=None):
        for parent in (path or self._search_path):
            c = mozpath.abspath(mozpath.join(parent, command))
            for candidate in (c, ensure_exe_extension(c)):
                if self.OS.path.exists(candidate):
                    return candidate
        raise WhichError()

    def Popen(self, args, stdin=None, stdout=None, stderr=None, **kargs):
        try:
            program = self.which(args[0])
        except WhichError:
            raise OSError(errno.ENOENT, 'File not found')

        func = self._subprocess_paths.get(program)
        retcode, stdout, stderr = func(stdin, args[1:])

        class Process(object):
            def communicate(self, stdin=None):
                return stdout, stderr

            def wait(self):
                return retcode

        return Process()

    def check_output(self, args, **kwargs):
        proc = self.Popen(args, **kwargs)
        stdout, stderr = proc.communicate()
        retcode = proc.wait()
        if retcode:
            raise subprocess.CalledProcessError(retcode, args, stdout)
        return stdout

    def shell(self, stdin, args):
        script = mozpath.abspath(args[0])
        if script in self._subprocess_paths:
            return self._subprocess_paths[script](stdin, args[1:])
        return 127, '', 'File not found'


class BaseConfigureTest(unittest.TestCase):
    HOST = 'x86_64-pc-linux-gnu'

    def setUp(self):
        self._cwd = os.getcwd()
        os.chdir(topobjdir)

    def tearDown(self):
        os.chdir(self._cwd)

    def config_guess(self, stdin, args):
        return 0, self.HOST, ''

    def config_sub(self, stdin, args):
        return 0, args[0], ''

    def get_sandbox(self, paths, config, args=[], environ={}, mozconfig='',
                    out=None, logger=None):
        kwargs = {}
        if logger:
            kwargs['logger'] = logger
        else:
            if not out:
                out = StringIO()
            kwargs['stdout'] = out
            kwargs['stderr'] = out

        if hasattr(self, 'TARGET'):
            target = ['--target=%s' % self.TARGET]
        else:
            target = []

        if mozconfig:
            fh, mozconfig_path = tempfile.mkstemp()
            os.write(fh, mozconfig)
            os.close(fh)
        else:
            mozconfig_path = os.path.join(os.path.dirname(__file__), 'data',
                                          'empty_mozconfig')

        try:
            environ = dict(
                environ,
                OLD_CONFIGURE=os.path.join(topsrcdir, 'old-configure'),
                MOZCONFIG=mozconfig_path)

            paths = dict(paths)
            autoconf_dir = mozpath.join(topsrcdir, 'build', 'autoconf')
            paths[mozpath.join(autoconf_dir,
                               'config.guess')] = self.config_guess
            paths[mozpath.join(autoconf_dir, 'config.sub')] = self.config_sub

            sandbox = ConfigureTestSandbox(paths, config, environ,
                                           ['configure'] + target + args,
                                           **kwargs)
            sandbox.include_file(os.path.join(topsrcdir, 'moz.configure'))

            return sandbox
        finally:
            if mozconfig:
                os.remove(mozconfig_path)
