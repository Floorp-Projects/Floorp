# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
import logging
import os
import re
import sys
import types
from collections import OrderedDict
from contextlib import contextmanager
from functools import wraps
from mozbuild.configure.options import (
    CommandLineHelper,
    ConflictingOptionError,
    InvalidOptionError,
    NegativeOptionValue,
    Option,
    OptionValue,
    PositiveOptionValue,
)
from mozbuild.configure.help import HelpFormatter
from mozbuild.configure.util import (
    ConfigureOutputHandler,
    getpreferredencoding,
    LineIO,
)
from mozbuild.util import (
    exec_,
    memoize,
    memoized_property,
    ReadOnlyDict,
    ReadOnlyNamespace,
)

import mozpack.path as mozpath


class ConfigureError(Exception):
    pass


class SandboxDependsFunction(object):
    '''Sandbox-visible representation of @depends functions.'''
    def __call__(self, *arg, **kwargs):
        raise ConfigureError('The `%s` function may not be called'
                             % self.__name__)


class DependsFunction(object):
    __slots__ = (
        'func', 'dependencies', 'when', 'sandboxed', 'sandbox', '_result')

    def __init__(self, sandbox, func, dependencies, when=None):
        assert isinstance(sandbox, ConfigureSandbox)
        self.func = func
        self.dependencies = dependencies
        self.sandboxed = wraps(func)(SandboxDependsFunction())
        self.sandbox = sandbox
        self.when = when
        sandbox._depends[self.sandboxed] = self

        # Only @depends functions with a dependency on '--help' are executed
        # immediately. Everything else is queued for later execution.
        if sandbox._help_option in dependencies:
            sandbox._value_for(self)
        elif not sandbox._help:
            sandbox._execution_queue.append((sandbox._value_for, (self,)))

    @property
    def name(self):
        return self.func.__name__

    @property
    def sandboxed_dependencies(self):
        return [
            d.sandboxed if isinstance(d, DependsFunction) else d
            for d in self.dependencies
        ]

    @memoized_property
    def result(self):
        if self.when and not self.sandbox._value_for(self.when):
            return None

        resolved_args = [self.sandbox._value_for(d) for d in self.dependencies]
        return self.func(*resolved_args)

    def __repr__(self):
        return '<%s.%s %s(%s)>' % (
            self.__class__.__module__,
            self.__class__.__name__,
            self.name,
            ', '.join(repr(d) for d in self.dependencies),
        )


class CombinedDependsFunction(DependsFunction):
    def __init__(self, sandbox, func, dependencies):
        @memoize
        @wraps(func)
        def wrapper(*args):
            return func(args)

        flatten_deps = []
        for d in dependencies:
            if isinstance(d, CombinedDependsFunction) and d.func == wrapper:
                for d2 in d.dependencies:
                    if d2 not in flatten_deps:
                        flatten_deps.append(d2)
            elif d not in flatten_deps:
                flatten_deps.append(d)

        # Automatically add a --help dependency if one of the dependencies
        # depends on it.
        for d in flatten_deps:
            if (isinstance(d, DependsFunction) and
                sandbox._help_option in d.dependencies):
                flatten_deps.insert(0, sandbox._help_option)
                break

        super(CombinedDependsFunction, self).__init__(
            sandbox, wrapper, flatten_deps)

    @memoized_property
    def result(self):
        # Ignore --help for the combined result
        deps = self.dependencies
        if deps[0] == self.sandbox._help_option:
            deps = deps[1:]
        resolved_args = [self.sandbox._value_for(d) for d in deps]
        return self.func(*resolved_args)

    def __eq__(self, other):
        return (isinstance(other, self.__class__) and
                self.func == other.func and
                set(self.dependencies) == set(other.dependencies))

    def __ne__(self, other):
        return not self == other

class SandboxedGlobal(dict):
    '''Identifiable dict type for use as function global'''


def forbidden_import(*args, **kwargs):
    raise ImportError('Importing modules is forbidden')


class ConfigureSandbox(dict):
    """Represents a sandbox for executing Python code for build configuration.
    This is a different kind of sandboxing than the one used for moz.build
    processing.

    The sandbox has 9 primitives:
    - option
    - depends
    - template
    - imports
    - include
    - set_config
    - set_define
    - imply_option
    - only_when

    `option`, `include`, `set_config`, `set_define` and `imply_option` are
    functions. `depends`, `template`, and `imports` are decorators. `only_when`
    is a context_manager.

    These primitives are declared as name_impl methods to this class and
    the mapping name -> name_impl is done automatically in __getitem__.

    Additional primitives should be frowned upon to keep the sandbox itself as
    simple as possible. Instead, helpers should be created within the sandbox
    with the existing primitives.

    The sandbox is given, at creation, a dict where the yielded configuration
    will be stored.

        config = {}
        sandbox = ConfigureSandbox(config)
        sandbox.run(path)
        do_stuff(config)
    """

    # The default set of builtins. We expose unicode as str to make sandboxed
    # files more python3-ready.
    BUILTINS = ReadOnlyDict({
        b: __builtins__[b]
        for b in ('None', 'False', 'True', 'int', 'bool', 'any', 'all', 'len',
                  'list', 'tuple', 'set', 'dict', 'isinstance', 'getattr',
                  'hasattr', 'enumerate', 'range', 'zip')
    }, __import__=forbidden_import, str=unicode)

    # Expose a limited set of functions from os.path
    OS = ReadOnlyNamespace(path=ReadOnlyNamespace(**{
        k: getattr(mozpath, k, getattr(os.path, k))
        for k in ('abspath', 'basename', 'dirname', 'isabs', 'join',
                  'normcase', 'normpath', 'realpath', 'relpath')
    }))

    def __init__(self, config, environ=os.environ, argv=sys.argv,
                 stdout=sys.stdout, stderr=sys.stderr, logger=None):
        dict.__setitem__(self, '__builtins__', self.BUILTINS)

        self._paths = []
        self._all_paths = set()
        self._templates = set()
        # Associate SandboxDependsFunctions to DependsFunctions.
        self._depends = {}
        self._seen = set()
        # Store the @imports added to a given function.
        self._imports = {}

        self._options = OrderedDict()
        # Store raw option (as per command line or environment) for each Option
        self._raw_options = OrderedDict()

        # Store options added with `imply_option`, and the reason they were
        # added (which can either have been given to `imply_option`, or
        # inferred. Their order matters, so use a list.
        self._implied_options = []

        # Store all results from _prepare_function
        self._prepared_functions = set()

        # Queue of functions to execute, with their arguments
        self._execution_queue = []

        # Store the `when`s associated to some options.
        self._conditions = {}

        # A list of conditions to apply as a default `when` for every *_impl()
        self._default_conditions = []

        self._helper = CommandLineHelper(environ, argv)

        assert isinstance(config, dict)
        self._config = config

        if logger is None:
            logger = moz_logger = logging.getLogger('moz.configure')
            logger.setLevel(logging.DEBUG)
            formatter = logging.Formatter('%(levelname)s: %(message)s')
            handler = ConfigureOutputHandler(stdout, stderr)
            handler.setFormatter(formatter)
            queue_debug = handler.queue_debug
            logger.addHandler(handler)

        else:
            assert isinstance(logger, logging.Logger)
            moz_logger = None
            @contextmanager
            def queue_debug():
                yield

        # Some callers will manage to log a bytestring with characters in it
        # that can't be converted to ascii. Make our log methods robust to this
        # by detecting the encoding that a producer is likely to have used.
        encoding = getpreferredencoding()
        def wrapped_log_method(logger, key):
            method = getattr(logger, key)
            if not encoding:
                return method
            def wrapped(*args, **kwargs):
                out_args = [
                    arg.decode(encoding) if isinstance(arg, str) else arg
                    for arg in args
                ]
                return method(*out_args, **kwargs)
            return wrapped

        log_namespace = {
            k: wrapped_log_method(logger, k)
            for k in ('debug', 'info', 'warning', 'error')
        }
        log_namespace['queue_debug'] = queue_debug
        self.log_impl = ReadOnlyNamespace(**log_namespace)

        self._help = None
        self._help_option = self.option_impl('--help',
                                             help='print this message')
        self._seen.add(self._help_option)

        self._always = DependsFunction(self, lambda: True, [])
        self._never = DependsFunction(self, lambda: False, [])

        if self._value_for(self._help_option):
            self._help = HelpFormatter(argv[0])
            self._help.add(self._help_option)
        elif moz_logger:
            handler = logging.FileHandler('config.log', mode='w', delay=True)
            handler.setFormatter(formatter)
            logger.addHandler(handler)

    def include_file(self, path):
        '''Include one file in the sandbox. Users of this class probably want

        Note: this will execute all template invocations, as well as @depends
        functions that depend on '--help', but nothing else.
        '''

        if self._paths:
            path = mozpath.join(mozpath.dirname(self._paths[-1]), path)
            path = mozpath.normpath(path)
            if not mozpath.basedir(path, (mozpath.dirname(self._paths[0]),)):
                raise ConfigureError(
                    'Cannot include `%s` because it is not in a subdirectory '
                    'of `%s`' % (path, mozpath.dirname(self._paths[0])))
        else:
            path = mozpath.realpath(mozpath.abspath(path))
        if path in self._all_paths:
            raise ConfigureError(
                'Cannot include `%s` because it was included already.' % path)
        self._paths.append(path)
        self._all_paths.add(path)

        source = open(path, 'rb').read()

        code = compile(source, path, 'exec')

        exec_(code, self)

        self._paths.pop(-1)

    def run(self, path=None):
        '''Executes the given file within the sandbox, as well as everything
        pending from any other included file, and ensure the overall
        consistency of the executed script(s).'''
        if path:
            self.include_file(path)

        for option in self._options.itervalues():
            # All options must be referenced by some @depends function
            if option not in self._seen:
                raise ConfigureError(
                    'Option `%s` is not handled ; reference it with a @depends'
                    % option.option
                )

            self._value_for(option)

        # All implied options should exist.
        for implied_option in self._implied_options:
            value = self._resolve(implied_option.value,
                                  need_help_dependency=False)
            if value is not None:
                raise ConfigureError(
                    '`%s`, emitted from `%s` line %d, is unknown.'
                    % (implied_option.option, implied_option.caller[1],
                       implied_option.caller[2]))

        # All options should have been removed (handled) by now.
        for arg in self._helper:
            without_value = arg.split('=', 1)[0]
            raise InvalidOptionError('Unknown option: %s' % without_value)

        # Run the execution queue
        for func, args in self._execution_queue:
            func(*args)

        if self._help:
            with LineIO(self.log_impl.info) as out:
                self._help.usage(out)

    def __getitem__(self, key):
        impl = '%s_impl' % key
        func = getattr(self, impl, None)
        if func:
            return func

        return super(ConfigureSandbox, self).__getitem__(key)

    def __setitem__(self, key, value):
        if (key in self.BUILTINS or key == '__builtins__' or
                hasattr(self, '%s_impl' % key)):
            raise KeyError('Cannot reassign builtins')

        if inspect.isfunction(value) and value not in self._templates:
            value, _ = self._prepare_function(value)

        elif (not isinstance(value, SandboxDependsFunction) and
                value not in self._templates and
                not (inspect.isclass(value) and issubclass(value, Exception))):
            raise KeyError('Cannot assign `%s` because it is neither a '
                           '@depends nor a @template' % key)

        return super(ConfigureSandbox, self).__setitem__(key, value)

    def _resolve(self, arg, need_help_dependency=True):
        if isinstance(arg, SandboxDependsFunction):
            return self._value_for_depends(self._depends[arg],
                                           need_help_dependency)
        return arg

    def _value_for(self, obj, need_help_dependency=False):
        if isinstance(obj, SandboxDependsFunction):
            assert obj in self._depends
            return self._value_for_depends(self._depends[obj],
                                           need_help_dependency)

        elif isinstance(obj, DependsFunction):
            return self._value_for_depends(obj, need_help_dependency)

        elif isinstance(obj, Option):
            return self._value_for_option(obj)

        assert False

    @memoize
    def _value_for_depends(self, obj, need_help_dependency=False):
        assert not inspect.isgeneratorfunction(obj.func)
        return obj.result

    @memoize
    def _value_for_option(self, option):
        implied = {}
        for implied_option in self._implied_options[:]:
            if implied_option.name not in (option.name, option.env):
                continue
            self._implied_options.remove(implied_option)

            if (implied_option.when and
                not self._value_for(implied_option.when)):
                continue

            value = self._resolve(implied_option.value,
                                  need_help_dependency=False)

            if value is not None:
                if isinstance(value, OptionValue):
                    pass
                elif value is True:
                    value = PositiveOptionValue()
                elif value is False or value == ():
                    value = NegativeOptionValue()
                elif isinstance(value, types.StringTypes):
                    value = PositiveOptionValue((value,))
                elif isinstance(value, tuple):
                    value = PositiveOptionValue(value)
                else:
                    raise TypeError("Unexpected type: '%s'"
                                    % type(value).__name__)

                opt = value.format(implied_option.option)
                self._helper.add(opt, 'implied')
                implied[opt] = implied_option

        try:
            value, option_string = self._helper.handle(option)
        except ConflictingOptionError as e:
            reason = implied[e.arg].reason
            if isinstance(reason, Option):
                reason = self._raw_options.get(reason) or reason.option
                reason = reason.split('=', 1)[0]
            raise InvalidOptionError(
                "'%s' implied by '%s' conflicts with '%s' from the %s"
                % (e.arg, reason, e.old_arg, e.old_origin))

        if option_string:
            self._raw_options[option] = option_string

        when = self._conditions.get(option)
        if (when and not self._value_for(when, need_help_dependency=True) and
            value is not None and value.origin != 'default'):
            if value.origin == 'environment':
                # The value we return doesn't really matter, because of the
                # requirement for @depends to have the same when.
                return None
            raise InvalidOptionError(
                '%s is not available in this configuration'
                % option_string.split('=', 1)[0])

        return value

    def _dependency(self, arg, callee_name, arg_name=None):
        if isinstance(arg, types.StringTypes):
            prefix, name, values = Option.split_option(arg)
            if values != ():
                raise ConfigureError("Option must not contain an '='")
            if name not in self._options:
                raise ConfigureError("'%s' is not a known option. "
                                     "Maybe it's declared too late?"
                                     % arg)
            arg = self._options[name]
            self._seen.add(arg)
        elif isinstance(arg, SandboxDependsFunction):
            assert arg in self._depends
            arg = self._depends[arg]
        else:
            raise TypeError(
                "Cannot use object of type '%s' as %sargument to %s"
                % (type(arg).__name__, '`%s` ' % arg_name if arg_name else '',
                   callee_name))
        return arg

    def _normalize_when(self, when, callee_name):
        if when is True:
            when = self._always
        elif when is False:
            when = self._never
        elif when is not None:
            when = self._dependency(when, callee_name, 'when')

        if self._default_conditions:
            # Create a pseudo @depends function for the combination of all
            # default conditions and `when`.
            dependencies = [when] if when else []
            dependencies.extend(self._default_conditions)
            if len(dependencies) == 1:
                return dependencies[0]
            return CombinedDependsFunction(self, all, dependencies)
        return when

    @contextmanager
    def only_when_impl(self, when):
        '''Implementation of only_when()

        `only_when` is a context manager that essentially makes calls to
        other sandbox functions within the context block ignored.
        '''
        when = self._normalize_when(when, 'only_when')
        if when and self._default_conditions[-1:] != [when]:
            self._default_conditions.append(when)
            yield
            self._default_conditions.pop()
        else:
            yield

    def option_impl(self, *args, **kwargs):
        '''Implementation of option()
        This function creates and returns an Option() object, passing it the
        resolved arguments (uses the result of functions when functions are
        passed). In most cases, the result of this function is not expected to
        be used.
        Command line argument/environment variable parsing for this Option is
        handled here.
        '''
        when = self._normalize_when(kwargs.get('when'), 'option')
        args = [self._resolve(arg) for arg in args]
        kwargs = {k: self._resolve(v) for k, v in kwargs.iteritems()
                                      if k != 'when'}
        option = Option(*args, **kwargs)
        if when:
            self._conditions[option] = when
        if option.name in self._options:
            raise ConfigureError('Option `%s` already defined' % option.option)
        if option.env in self._options:
            raise ConfigureError('Option `%s` already defined' % option.env)
        if option.name:
            self._options[option.name] = option
        if option.env:
            self._options[option.env] = option

        if self._help and (when is None or
                           self._value_for(when, need_help_dependency=True)):
            self._help.add(option)

        return option

    def depends_impl(self, *args, **kwargs):
        '''Implementation of @depends()
        This function is a decorator. It returns a function that subsequently
        takes a function and returns a dummy function. The dummy function
        identifies the actual function for the sandbox, while preventing
        further function calls from within the sandbox.

        @depends() takes a variable number of option strings or dummy function
        references. The decorated function is called as soon as the decorator
        is called, and the arguments it receives are the OptionValue or
        function results corresponding to each of the arguments to @depends.
        As an exception, when a HelpFormatter is attached, only functions that
        have '--help' in their @depends argument list are called.

        The decorated function is altered to use a different global namespace
        for its execution. This different global namespace exposes a limited
        set of functions from os.path.
        '''
        for k in kwargs:
            if k != 'when':
                raise TypeError(
                    "depends_impl() got an unexpected keyword argument '%s'"
                    % k)

        when = self._normalize_when(kwargs.get('when'), '@depends')

        if not when and not args:
            raise ConfigureError('@depends needs at least one argument')

        dependencies = tuple(self._dependency(arg, '@depends') for arg in args)

        conditions = [
            self._conditions[d]
            for d in dependencies
            if d in self._conditions and isinstance(d, Option)
        ]
        for c in conditions:
            if c != when:
                raise ConfigureError('@depends function needs the same `when` '
                                     'as options it depends on')

        def decorator(func):
            if inspect.isgeneratorfunction(func):
                raise ConfigureError(
                    'Cannot decorate generator functions with @depends')
            func, glob = self._prepare_function(func)
            depends = DependsFunction(self, func, dependencies, when=when)
            return depends.sandboxed

        return decorator

    def include_impl(self, what, when=None):
        '''Implementation of include().
        Allows to include external files for execution in the sandbox.
        It is possible to use a @depends function as argument, in which case
        the result of the function is the file name to include. This latter
        feature is only really meant for --enable-application/--enable-project.
        '''
        with self.only_when_impl(when):
            what = self._resolve(what)
            if what:
                if not isinstance(what, types.StringTypes):
                    raise TypeError("Unexpected type: '%s'" % type(what).__name__)
                self.include_file(what)

    def template_impl(self, func):
        '''Implementation of @template.
        This function is a decorator. Template functions are called
        immediately. They are altered so that their global namespace exposes
        a limited set of functions from os.path, as well as `depends` and
        `option`.
        Templates allow to simplify repetitive constructs, or to implement
        helper decorators and somesuch.
        '''
        template, glob = self._prepare_function(func)
        glob.update(
            (k[:-len('_impl')], getattr(self, k))
            for k in dir(self) if k.endswith('_impl') and k != 'template_impl'
        )
        glob.update((k, v) for k, v in self.iteritems() if k not in glob)

        # Any function argument to the template must be prepared to be sandboxed.
        # If the template itself returns a function (in which case, it's very
        # likely a decorator), that function must be prepared to be sandboxed as
        # well.
        def wrap_template(template):
            isfunction = inspect.isfunction

            def maybe_prepare_function(obj):
                if isfunction(obj):
                    func, _ = self._prepare_function(obj)
                    return func
                return obj

            # The following function may end up being prepared to be sandboxed,
            # so it mustn't depend on anything from the global scope in this
            # file. It can however depend on variables from the closure, thus
            # maybe_prepare_function and isfunction are declared above to be
            # available there.
            @self.wraps(template)
            def wrapper(*args, **kwargs):
                args = [maybe_prepare_function(arg) for arg in args]
                kwargs = {k: maybe_prepare_function(v)
                          for k, v in kwargs.iteritems()}
                ret = template(*args, **kwargs)
                if isfunction(ret):
                    # We can't expect the sandboxed code to think about all the
                    # details of implementing decorators, so do some of the
                    # work for them. If the function takes exactly one function
                    # as argument and returns a function, it must be a
                    # decorator, so mark the returned function as wrapping the
                    # function passed in.
                    if len(args) == 1 and not kwargs and isfunction(args[0]):
                        ret = self.wraps(args[0])(ret)
                    return wrap_template(ret)
                return ret
            return wrapper

        wrapper = wrap_template(template)
        self._templates.add(wrapper)
        return wrapper

    def wraps(self, func):
        return wraps(func)

    RE_MODULE = re.compile('^[a-zA-Z0-9_\.]+$')

    def imports_impl(self, _import, _from=None, _as=None):
        '''Implementation of @imports.
        This decorator imports the given _import from the given _from module
        optionally under a different _as name.
        The options correspond to the various forms for the import builtin.
            @imports('sys')
            @imports(_from='mozpack', _import='path', _as='mozpath')
        '''
        for value, required in (
                (_import, True), (_from, False), (_as, False)):

            if not isinstance(value, types.StringTypes) and (
                    required or value is not None):
                raise TypeError("Unexpected type: '%s'" % type(value).__name__)
            if value is not None and not self.RE_MODULE.match(value):
                raise ValueError("Invalid argument to @imports: '%s'" % value)
        if _as and '.' in _as:
            raise ValueError("Invalid argument to @imports: '%s'" % _as)

        def decorator(func):
            if func in self._templates:
                raise ConfigureError(
                    '@imports must appear after @template')
            if func in self._depends:
                raise ConfigureError(
                    '@imports must appear after @depends')
            # For the imports to apply in the order they appear in the
            # .configure file, we accumulate them in reverse order and apply
            # them later.
            imports = self._imports.setdefault(func, [])
            imports.insert(0, (_from, _import, _as))
            return func

        return decorator

    def _apply_imports(self, func, glob):
        for _from, _import, _as in self._imports.get(func, ()):
            _from = '%s.' % _from if _from else ''
            if _as:
                glob[_as] = self._get_one_import('%s%s' % (_from, _import))
            else:
                what = _import.split('.')[0]
                glob[what] = self._get_one_import('%s%s' % (_from, what))

    def _get_one_import(self, what):
        # The special `__sandbox__` module gives access to the sandbox
        # instance.
        if what == '__sandbox__':
            return self
        # Special case for the open() builtin, because otherwise, using it
        # fails with "IOError: file() constructor not accessible in
        # restricted mode"
        if what == '__builtin__.open':
            return lambda *args, **kwargs: open(*args, **kwargs)
        # Until this proves to be a performance problem, just construct an
        # import statement and execute it.
        import_line = ''
        if '.' in what:
            _from, what = what.rsplit('.', 1)
            import_line += 'from %s ' % _from
        import_line += 'import %s as imported' % what
        glob = {}
        exec_(import_line, {}, glob)
        return glob['imported']

    def _resolve_and_set(self, data, name, value, when=None):
        # Don't set anything when --help was on the command line
        if self._help:
            return
        if when and not self._value_for(when):
            return
        name = self._resolve(name, need_help_dependency=False)
        if name is None:
            return
        if not isinstance(name, types.StringTypes):
            raise TypeError("Unexpected type: '%s'" % type(name).__name__)
        if name in data:
            raise ConfigureError(
                "Cannot add '%s' to configuration: Key already "
                "exists" % name)
        value = self._resolve(value, need_help_dependency=False)
        if value is not None:
            data[name] = value

    def set_config_impl(self, name, value, when=None):
        '''Implementation of set_config().
        Set the configuration items with the given name to the given value.
        Both `name` and `value` can be references to @depends functions,
        in which case the result from these functions is used. If the result
        of either function is None, the configuration item is not set.
        '''
        when = self._normalize_when(when, 'set_config')

        self._execution_queue.append((
            self._resolve_and_set, (self._config, name, value, when)))

    def set_define_impl(self, name, value, when=None):
        '''Implementation of set_define().
        Set the define with the given name to the given value. Both `name` and
        `value` can be references to @depends functions, in which case the
        result from these functions is used. If the result of either function
        is None, the define is not set. If the result is False, the define is
        explicitly undefined (-U).
        '''
        when = self._normalize_when(when, 'set_define')

        defines = self._config.setdefault('DEFINES', {})
        self._execution_queue.append((
            self._resolve_and_set, (defines, name, value, when)))

    def imply_option_impl(self, option, value, reason=None, when=None):
        '''Implementation of imply_option().
        Injects additional options as if they had been passed on the command
        line. The `option` argument is a string as in option()'s `name` or
        `env`. The option must be declared after `imply_option` references it.
        The `value` argument indicates the value to pass to the option.
        It can be:
        - True. In this case `imply_option` injects the positive option
          (--enable-foo/--with-foo).
              imply_option('--enable-foo', True)
              imply_option('--disable-foo', True)
          are both equivalent to `--enable-foo` on the command line.

        - False. In this case `imply_option` injects the negative option
          (--disable-foo/--without-foo).
              imply_option('--enable-foo', False)
              imply_option('--disable-foo', False)
          are both equivalent to `--disable-foo` on the command line.

        - None. In this case `imply_option` does nothing.
              imply_option('--enable-foo', None)
              imply_option('--disable-foo', None)
          are both equivalent to not passing any flag on the command line.

        - a string or a tuple. In this case `imply_option` injects the positive
          option with the given value(s).
              imply_option('--enable-foo', 'a')
              imply_option('--disable-foo', 'a')
          are both equivalent to `--enable-foo=a` on the command line.
              imply_option('--enable-foo', ('a', 'b'))
              imply_option('--disable-foo', ('a', 'b'))
          are both equivalent to `--enable-foo=a,b` on the command line.

        Because imply_option('--disable-foo', ...) can be misleading, it is
        recommended to use the positive form ('--enable' or '--with') for
        `option`.

        The `value` argument can also be (and usually is) a reference to a
        @depends function, in which case the result of that function will be
        used as per the descripted mapping above.

        The `reason` argument indicates what caused the option to be implied.
        It is necessary when it cannot be inferred from the `value`.
        '''
        # Don't do anything when --help was on the command line
        if self._help:
            return
        if not reason and isinstance(value, SandboxDependsFunction):
            deps = self._depends[value].dependencies
            possible_reasons = [d for d in deps if d != self._help_option]
            if len(possible_reasons) == 1:
                if isinstance(possible_reasons[0], Option):
                    reason = possible_reasons[0]
        if not reason and (isinstance(value, (bool, tuple)) or
                           isinstance(value, types.StringTypes)):
            # A reason can be provided automatically when imply_option
            # is called with an immediate value.
            _, filename, line, _, _, _ = inspect.stack()[1]
            reason = "imply_option at %s:%s" % (filename, line)

        if not reason:
            raise ConfigureError(
                "Cannot infer what implies '%s'. Please add a `reason` to "
                "the `imply_option` call."
                % option)

        when = self._normalize_when(when, 'imply_option')

        prefix, name, values = Option.split_option(option)
        if values != ():
            raise ConfigureError("Implied option must not contain an '='")

        self._implied_options.append(ReadOnlyNamespace(
            option=option,
            prefix=prefix,
            name=name,
            value=value,
            caller=inspect.stack()[1],
            reason=reason,
            when=when,
        ))

    def _prepare_function(self, func):
        '''Alter the given function global namespace with the common ground
        for @depends, and @template.
        '''
        if not inspect.isfunction(func):
            raise TypeError("Unexpected type: '%s'" % type(func).__name__)
        if func in self._prepared_functions:
            return func, func.func_globals

        glob = SandboxedGlobal(
            (k, v) for k, v in func.func_globals.iteritems()
            if (inspect.isfunction(v) and v not in self._templates) or (
                inspect.isclass(v) and issubclass(v, Exception))
        )
        glob.update(
            __builtins__=self.BUILTINS,
            __file__=self._paths[-1] if self._paths else '',
            __name__=self._paths[-1] if self._paths else '',
            os=self.OS,
            log=self.log_impl,
        )

        # The execution model in the sandbox doesn't guarantee the execution
        # order will always be the same for a given function, and if it uses
        # variables from a closure that are changed after the function is
        # declared, depending when the function is executed, the value of the
        # variable can differ. For consistency, we force the function to use
        # the value from the earliest it can be run, which is at declaration.
        # Note this is not entirely bullet proof (if the value is e.g. a list,
        # the list contents could have changed), but covers the bases.
        closure = None
        if func.func_closure:
            def makecell(content):
                def f():
                    content
                return f.func_closure[0]

            closure = tuple(makecell(cell.cell_contents)
                            for cell in func.func_closure)

        new_func = self.wraps(func)(types.FunctionType(
            func.func_code,
            glob,
            func.__name__,
            func.func_defaults,
            closure
        ))
        @self.wraps(new_func)
        def wrapped(*args, **kwargs):
            if func in self._imports:
                self._apply_imports(func, glob)
                del self._imports[func]
            return new_func(*args, **kwargs)

        self._prepared_functions.add(wrapped)
        return wrapped, glob
