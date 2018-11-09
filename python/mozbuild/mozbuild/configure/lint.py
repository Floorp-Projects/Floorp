# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
import re
import types
from functools import wraps
from StringIO import StringIO
from . import (
    CombinedDependsFunction,
    ConfigureError,
    ConfigureSandbox,
    DependsFunction,
    SandboxedGlobal,
    TrivialDependsFunction,
    SandboxDependsFunction,
)
from .help import HelpFormatter
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
        self._bool_options = []
        self._bool_func_options = []
        self.LOG = ""
        super(LintSandbox, self).__init__({}, environ=environ, argv=argv,
                                          stdout=stdout, stderr=stderr)

    def run(self, path=None):
        if path:
            self.include_file(path)

        for dep in self._depends.itervalues():
            self._check_dependencies(dep)

    def _check_dependencies(self, obj):
        if isinstance(obj, CombinedDependsFunction) or obj in (self._always,
                                                               self._never):
            return
        func, glob = self.unwrap(obj._func)
        loc = '%s:%d' % (func.func_code.co_filename,
                         func.func_code.co_firstlineno)
        func_args = inspect.getargspec(func)
        if func_args.keywords:
            raise ConfigureError(
                '%s: Keyword arguments are not allowed in @depends functions'
                % loc
            )

        all_args = list(func_args.args)
        if func_args.varargs:
            all_args.append(func_args.varargs)
        used_args = set()

        for op, arg in disassemble_as_iter(func):
            if op in ('LOAD_FAST', 'LOAD_CLOSURE'):
                if arg in all_args:
                    used_args.add(arg)

        for num, arg in enumerate(all_args):
            if arg not in used_args:
                dep = obj.dependencies[num]
                if dep != self._help_option:
                    if isinstance(dep, DependsFunction):
                        dep = dep.name
                    else:
                        dep = dep.option
                    raise ConfigureError(
                        '%s: The dependency on `%s` is unused.'
                        % (loc, dep)
                    )

    def _missing_help_dependency(self, obj):
        if isinstance(obj, (CombinedDependsFunction, TrivialDependsFunction)):
            return False
        if isinstance(obj, DependsFunction):
            if (self._help_option in obj.dependencies or
                obj in (self._always, self._never)):
                return False
            func, glob = self.unwrap(obj._func)
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
                    if arg in self.BUILTINS:
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

    def option_impl(self, *args, **kwargs):
        result = super(LintSandbox, self).option_impl(*args, **kwargs)
        when = self._conditions.get(result)
        if when:
            self._value_for(when, need_help_dependency=True)

        self._check_option(result, *args, **kwargs)

        return result

    def _check_option(self, option, *args, **kwargs):
        if 'default' not in kwargs:
            return
        if len(args) == 0:
            return

        self._check_prefix_for_bool_option(*args, **kwargs)
        self._check_help_for_option_with_func_default(option, *args, **kwargs)

    def _check_prefix_for_bool_option(self, *args, **kwargs):
        name = args[0]
        default = kwargs['default']

        if type(default) != bool:
            return

        table = {
            True: {
                'enable': 'disable',
                'with': 'without',
            },
            False: {
                'disable': 'enable',
                'without': 'with',
            }
        }
        for prefix, replacement in table[default].iteritems():
            if name.startswith('--{}-'.format(prefix)):
                raise ConfigureError(('{} should be used instead of '
                                      '{} with default={}').format(
                                          name.replace('--{}-'.format(prefix),
                                                       '--{}-'.format(replacement)),
                                          name, default))

    def _check_help_for_option_with_func_default(self, option, *args, **kwargs):
        name = args[0]
        default = kwargs['default']

        if not isinstance(default, SandboxDependsFunction):
            return

        if not option.prefix:
            return

        default = self._resolve(default)
        if type(default) in (str, unicode):
            return

        help = kwargs['help']
        match = re.search(HelpFormatter.RE_FORMAT, help)
        if match:
            return

        if option.prefix in ('enable', 'disable'):
            rule = '{Enable|Disable}'
        else:
            rule = '{With|Without}'

        raise ConfigureError(('{} has a non-constant default. '
                              'Its help should contain "{}"').format(name, rule))

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
