# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from functools import wraps
from StringIO import StringIO
from . import (
    CombinedDependsFunction,
    ConfigureError,
    ConfigureSandbox,
    DependsFunction,
    SandboxedGlobal,
)
from .lint_util import disassemble_as_iter
from mozbuild.util import memoize


class LintSandbox(ConfigureSandbox):
    def __init__(self, environ=None, argv=None, stdout=None, stderr=None):
        out = StringIO()
        stdout = stdout or out
        stderr = stderr or out
        environ = environ or {}
        argv = argv or []
        self._wrapped = {}
        super(LintSandbox, self).__init__({}, environ=environ, argv=argv,
                                          stdout=stdout, stderr=stderr)

    def run(self, path=None):
        if path:
            self.include_file(path)

    def _missing_help_dependency(self, obj):
        if isinstance(obj, CombinedDependsFunction):
            return False
        if isinstance(obj, DependsFunction):
            if (self._help_option in obj.dependencies or
                obj in (self._always, self._never)):
                return False
            func, glob = self.unwrap(obj.func)
            # We allow missing --help dependencies for functions that:
            # - don't use @imports
            # - don't have a closure
            # - don't use global variables
            if func in self._imports or func.func_closure:
                return True
            for op, arg in disassemble_as_iter(func):
                if op in ('LOAD_GLOBAL', 'STORE_GLOBAL'):
                    # There is a fake os module when one is not imported,
                    # and it's allowed for functions without a --help
                    # dependency.
                    if arg == 'os' and glob.get('os') is self.OS:
                        continue
                    return True
        return False

    @memoize
    def _value_for_depends(self, obj, need_help_dependency=False):
        with_help = self._help_option in obj.dependencies
        if with_help:
            for arg in obj.dependencies:
                if self._missing_help_dependency(arg):
                    raise ConfigureError(
                        "`%s` depends on '--help' and `%s`. "
                        "`%s` must depend on '--help'"
                        % (obj.name, arg.name, arg.name))
        elif ((self._help or need_help_dependency) and
              self._missing_help_dependency(obj)):
            raise ConfigureError("Missing @depends for `%s`: '--help'" %
                                 obj.name)
        return super(LintSandbox, self)._value_for_depends(
            obj, need_help_dependency)

    def unwrap(self, func):
        glob = func.func_globals
        while func in self._wrapped:
            if isinstance(func.func_globals, SandboxedGlobal):
                glob = func.func_globals
            func = self._wrapped[func]
        return func, glob

    def wraps(self, func):
        def do_wraps(wrapper):
            self._wrapped[wrapper] = func
            return wraps(func)(wrapper)
        return do_wraps
