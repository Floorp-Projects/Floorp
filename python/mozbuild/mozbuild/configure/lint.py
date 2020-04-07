# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
import re
import types
from dis import Bytecode
from functools import wraps
from io import StringIO
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
from mozbuild.util import memoize


class LintSandbox(ConfigureSandbox):
    def __init__(self, environ=None, argv=None, stdout=None, stderr=None):
        out = StringIO()
        stdout = stdout or out
        stderr = stderr or out
        environ = environ or {}
        argv = argv or []
        self._wrapped = {}
        self._has_imports = set()
        self._bool_options = []
        self._bool_func_options = []
        self.LOG = ""
        super(LintSandbox, self).__init__({}, environ=environ, argv=argv,
                                          stdout=stdout, stderr=stderr)

    def run(self, path=None):
        if path:
            self.include_file(path)

        for dep in self._depends.values():
            self._check_dependencies(dep)

    def _raise_from(self, exception, obj, line=0):
        '''
        Raises the given exception as if it were emitted from the given
        location.

        The location is determined from the values of obj and line.
        - `obj` can be a function or DependsFunction, in which case
          `line` corresponds to the line within the function the exception
          will be raised from (as an offset from the function's firstlineno).
        - `obj` can be a stack frame, in which case `line` is ignored.
        '''
        def thrower(e):
            raise e

        if isinstance(obj, DependsFunction):
            obj, _ = self.unwrap(obj._func)

        if inspect.isfunction(obj):
            funcname = obj.__name__
            filename = obj.__code__.co_filename
            firstline = obj.__code__.co_firstlineno
            line += firstline
        elif inspect.isframe(obj):
            funcname = obj.f_code.co_name
            filename = obj.f_code.co_filename
            firstline = obj.f_code.co_firstlineno
            line = obj.f_lineno
        else:
            # Don't know how to handle the given location, still raise the
            # exception.
            raise exception

        # Create a new function from the above thrower that pretends
        # the `def` line is on the first line of the function given as
        # argument, and the `raise` line is on the line given as argument.

        offset = line - firstline
        # co_lnotab is a string where each pair of consecutive character is
        # (chr(byte_increment), chr(line_increment)), mapping bytes in co_code
        # to line numbers relative to co_firstlineno.
        # If the offset we need to encode is larger than 255, we need to split it.
        co_lnotab = bytes([0, 255] * (offset // 255) + [0, offset % 255])
        code = thrower.__code__
        code = types.CodeType(
            code.co_argcount,
            code.co_kwonlyargcount,
            code.co_nlocals,
            code.co_stacksize,
            code.co_flags,
            code.co_code,
            code.co_consts,
            code.co_names,
            code.co_varnames,
            filename,
            funcname,
            firstline,
            co_lnotab
        )
        thrower = types.FunctionType(
            code,
            thrower.__globals__,
            funcname,
            thrower.__defaults__,
            thrower.__closure__
        )
        thrower(exception)

    def _check_dependencies(self, obj):
        if isinstance(obj, CombinedDependsFunction) or obj in (self._always,
                                                               self._never):
            return
        func, glob = self.unwrap(obj._func)
        func_args = inspect.getargspec(func)
        if func_args.keywords:
            e = ConfigureError(
                    'Keyword arguments are not allowed in @depends functions')
            self._raise_from(e, func)

        all_args = list(func_args.args)
        if func_args.varargs:
            all_args.append(func_args.varargs)
        used_args = set()

        for instr in Bytecode(func):
            if instr.opname in ('LOAD_FAST', 'LOAD_CLOSURE'):
                if instr.argval in all_args:
                    used_args.add(instr.argval)

        for num, arg in enumerate(all_args):
            if arg not in used_args:
                dep = obj.dependencies[num]
                if dep != self._help_option or not self._need_help_dependency(obj):
                    if isinstance(dep, DependsFunction):
                        dep = dep.name
                    else:
                        dep = dep.option
                    e = ConfigureError('The dependency on `%s` is unused' % dep)
                    self._raise_from(e, func)

    def _need_help_dependency(self, obj):
        if isinstance(obj, (CombinedDependsFunction, TrivialDependsFunction)):
            return False
        if isinstance(obj, DependsFunction):
            if obj in (self._always, self._never):
                return False
            func, glob = self.unwrap(obj._func)
            # We allow missing --help dependencies for functions that:
            # - don't use @imports
            # - don't have a closure
            # - don't use global variables
            if func in self._has_imports or func.__closure__:
                return True
            for instr in Bytecode(func):
                if instr.opname in ('LOAD_GLOBAL', 'STORE_GLOBAL'):
                    # There is a fake os module when one is not imported,
                    # and it's allowed for functions without a --help
                    # dependency.
                    if instr.argval == 'os' and glob.get('os') is self.OS:
                        continue
                    if instr.argval in self.BUILTINS:
                        continue
                    return True
        return False

    def _missing_help_dependency(self, obj):
        if (isinstance(obj, DependsFunction) and
                self._help_option in obj.dependencies):
            return False
        return self._need_help_dependency(obj)

    @memoize
    def _value_for_depends(self, obj):
        with_help = self._help_option in obj.dependencies
        if with_help:
            for arg in obj.dependencies:
                if self._missing_help_dependency(arg):
                    e = ConfigureError(
                        "Missing '--help' dependency because `%s` depends on "
                        "'--help' and `%s`" % (obj.name, arg.name))
                    self._raise_from(e, arg)
        elif self._missing_help_dependency(obj):
            e = ConfigureError("Missing '--help' dependency")
            self._raise_from(e, obj)
        return super(LintSandbox, self)._value_for_depends(obj)

    def option_impl(self, *args, **kwargs):
        result = super(LintSandbox, self).option_impl(*args, **kwargs)
        when = self._conditions.get(result)
        if when:
            self._value_for(when)

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
        for prefix, replacement in table[default].items():
            if name.startswith('--{}-'.format(prefix)):
                frame = inspect.currentframe()
                while frame and frame.f_code.co_name != self.option_impl.__name__:
                    frame = frame.f_back
                e = ConfigureError('{} should be used instead of '
                                   '{} with default={}'.format(
                                       name.replace('--{}-'.format(prefix),
                                                    '--{}-'.format(replacement)),
                                       name, default))
                self._raise_from(e, frame.f_back if frame else None)

    def _check_help_for_option_with_func_default(self, option, *args, **kwargs):
        default = kwargs['default']

        if not isinstance(default, SandboxDependsFunction):
            return

        if not option.prefix:
            return

        default = self._resolve(default)
        if type(default) is str:
            return

        help = kwargs['help']
        match = re.search(HelpFormatter.RE_FORMAT, help)
        if match:
            return

        if option.prefix in ('enable', 'disable'):
            rule = '{Enable|Disable}'
        else:
            rule = '{With|Without}'

        frame = inspect.currentframe()
        while frame and frame.f_code.co_name != self.option_impl.__name__:
            frame = frame.f_back
        e = ConfigureError(
            '`help` should contain "{}" because of non-constant default'
            .format(rule))
        self._raise_from(e, frame.f_back if frame else None)

    def unwrap(self, func):
        glob = func.__globals__
        while func in self._wrapped:
            if isinstance(func.__globals__, SandboxedGlobal):
                glob = func.__globals__
            func = self._wrapped[func]
        return func, glob

    def wraps(self, func):
        def do_wraps(wrapper):
            self._wrapped[wrapper] = func
            return wraps(func)(wrapper)
        return do_wraps

    def imports_impl(self, _import, _from=None, _as=None):
        wrapper = super(LintSandbox, self).imports_impl(_import, _from=_from, _as=_as)

        def decorator(func):
            self._has_imports.add(func)
            return wrapper(func)
        return decorator

    def _prepare_function(self, func, update_globals=None):
        wrapped = super(LintSandbox, self)._prepare_function(func, update_globals)
        _, glob = self.unwrap(wrapped)
        imports = set()
        for _from, _import, _as in self._imports.get(func, ()):
            if _as:
                imports.add(_as)
            else:
                what = _import.split('.')[0]
                imports.add(what)
        for instr in Bytecode(func):
            code = func.__code__
            if instr.opname == 'LOAD_GLOBAL' and \
                    instr.argval not in glob and \
                    instr.argval not in imports and \
                    instr.argval not in glob['__builtins__'] and \
                    instr.argval not in code.co_varnames[:code.co_argcount]:
                # Raise the same kind of error as what would happen during
                # execution.
                e = NameError("global name '{}' is not defined".format(instr.argval))
                self._raise_from(e, func, instr.starts_line - func.__code__.co_firstlineno)

        return wrapped
