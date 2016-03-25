# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
import logging
import os
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
    LineIO,
)
from mozbuild.util import (
    ReadOnlyDict,
    ReadOnlyNamespace,
)
import mozpack.path as mozpath


class ConfigureError(Exception):
    pass


class DummyFunction(object):
    '''Sandbox-visible representation of @depends functions.'''
    def __call__(self, *arg, **kwargs):
        raise RuntimeError('The `%s` function may not be called'
                           % self.__name__)


class SandboxedGlobal(dict):
    '''Identifiable dict type for use as function global'''


def forbidden_import(*args, **kwargs):
    raise ImportError('Importing modules is forbidden')


class ConfigureSandbox(dict):
    """Represents a sandbox for executing Python code for build configuration.
    This is a different kind of sandboxing than the one used for moz.build
    processing.

    The sandbox has 5 primitives:
    - option
    - depends
    - template
    - advanced
    - include

    `option` and `include` are functions. `depends`, `template` and `advanced`
    are decorators.

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

    # The default set of builtins.
    BUILTINS = ReadOnlyDict({
        b: __builtins__[b]
        for b in ('None', 'False', 'True', 'int', 'bool', 'any', 'all', 'len',
                  'list', 'tuple', 'set', 'dict', 'isinstance')
    }, __import__=forbidden_import)

    # Expose a limited set of functions from os.path
    OS = ReadOnlyNamespace(path=ReadOnlyNamespace(**{
        k: getattr(mozpath, k, getattr(os.path, k))
        for k in ('abspath', 'basename', 'dirname', 'exists', 'isabs', 'isdir',
                  'isfile', 'join', 'normpath', 'realpath', 'relpath')
    }))

    def __init__(self, config, environ=os.environ, argv=sys.argv,
                 stdout=sys.stdout, stderr=sys.stderr, logger=None):
        dict.__setitem__(self, '__builtins__', self.BUILTINS)

        self._paths = []
        self._templates = set()
        # Store the real function and its dependencies, behind each
        # DummyFunction generated from @depends.
        self._depends = {}
        self._seen = set()

        self._options = OrderedDict()
        # Store the raw values returned by @depends functions
        self._results = {}
        # Store values for each Option, as per returned by Option.get_value
        self._option_values = {}
        # Store raw option (as per command line or environment) for each Option
        self._raw_options = {}

        # Store options added with `imply_option`, and the reason they were
        # added (which can either have been given to `imply_option`, or
        # inferred.
        self._implied_options = {}

        # Store all results from _prepare_function
        self._prepared_functions = set()

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

        log_namespace = {
            k: getattr(logger, k)
            for k in ('debug', 'info', 'warning', 'error')
        }
        log_namespace['queue_debug'] = queue_debug
        self.log_impl = ReadOnlyNamespace(**log_namespace)

        self._help = None
        self._help_option = self.option_impl('--help',
                                             help='print this message')
        self._seen.add(self._help_option)
        # self._option_impl('--help') will have set this if --help was on the
        # command line.
        if self._option_values[self._help_option]:
            self._help = HelpFormatter(argv[0])
            self._help.add(self._help_option)
        elif moz_logger:
            handler = logging.FileHandler('config.log', mode='w', delay=True)
            handler.setFormatter(formatter)
            logger.addHandler(handler)

    def exec_file(self, path):
        '''Execute one file within the sandbox. Users of this class probably
        want to use `run` instead.'''

        if self._paths:
            path = mozpath.join(mozpath.dirname(self._paths[-1]), path)
            if not mozpath.basedir(path, (mozpath.dirname(self._paths[0]),)):
                raise ConfigureError(
                    'Cannot include `%s` because it is not in a subdirectory '
                    'of `%s`' % (path, mozpath.dirname(self._paths[0])))
        else:
            path = mozpath.realpath(mozpath.abspath(path))
        if path in self._paths:
            raise ConfigureError(
                'Cannot include `%s` because it was included already.' % path)
        self._paths.append(path)

        source = open(path, 'rb').read()

        code = compile(source, path, 'exec')

        exec(code, self)

        self._paths.pop(-1)

    def run(self, path):
        '''Executes the given file within the sandbox, and ensure the overall
        consistency of the executed script.'''
        self.exec_file(path)

        # All command line arguments should have been removed (handled) by now.
        for arg in self._helper:
            without_value = arg.split('=', 1)[0]
            if arg in self._implied_options:
                frameinfo, reason = self._implied_options[arg]
                raise ConfigureError(
                    '`%s`, emitted from `%s` line `%d`, was not handled.'
                    % (without_value, frameinfo[1], frameinfo[2]))
            raise InvalidOptionError('Unknown option: %s' % without_value)

        # All options must be referenced by some @depends function
        for option in self._options.itervalues():
            if option not in self._seen:
                raise ConfigureError(
                    'Option `%s` is not handled ; reference it with a @depends'
                    % option.option
                )

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

        if (not isinstance(value, DummyFunction) and
                value not in self._templates and
                not issubclass(value, Exception)):
            raise KeyError('Cannot assign `%s` because it is neither a '
                           '@depends nor a @template' % key)

        return super(ConfigureSandbox, self).__setitem__(key, value)

    def _resolve(self, arg, need_help_dependency=True):
        if isinstance(arg, DummyFunction):
            assert arg in self._depends
            func, deps = self._depends[arg]
            assert not inspect.isgeneratorfunction(func)
            assert func in self._results
            if need_help_dependency and self._help_option not in deps:
                raise ConfigureError("Missing @depends for `%s`: '--help'" %
                                     func.__name__)
            result = self._results[func]
            return result
        return arg

    def option_impl(self, *args, **kwargs):
        '''Implementation of option()
        This function creates and returns an Option() object, passing it the
        resolved arguments (uses the result of functions when functions are
        passed). In most cases, the result of this function is not expected to
        be used.
        Command line argument/environment variable parsing for this Option is
        handled here.
        '''
        args = [self._resolve(arg) for arg in args]
        kwargs = {k: self._resolve(v) for k, v in kwargs.iteritems()}
        option = Option(*args, **kwargs)
        if option.name in self._options:
            raise ConfigureError('Option `%s` already defined'
                                 % self._options[option.name].option)
        if option.env in self._options:
            raise ConfigureError('Option `%s` already defined'
                                 % self._options[option.env].option)
        if option.name:
            self._options[option.name] = option
        if option.env:
            self._options[option.env] = option

        try:
            value, option_string = self._helper.handle(option)
        except ConflictingOptionError as e:
            frameinfo, reason = self._implied_options[e.arg]
            raise InvalidOptionError(
                "'%s' implied by '%s' conflicts with '%s' from the %s"
                % (e.arg, reason, e.old_arg, e.old_origin))

        if self._help:
            self._help.add(option)

        self._option_values[option] = value
        self._raw_options[option] = (option_string.split('=', 1)[0]
                                     if option_string else option_string)
        return option

    def depends_impl(self, *args):
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
        if not args:
            raise ConfigureError('@depends needs at least one argument')

        resolved_args = []
        dependencies = []
        for arg in args:
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
                dependencies.append(arg)
                assert arg in self._option_values or self._help
                resolved_arg = self._option_values.get(arg)
            elif isinstance(arg, DummyFunction):
                assert arg in self._depends
                dependencies.append(arg)
                arg, _ = self._depends[arg]
                resolved_arg = self._results.get(arg)
            else:
                raise TypeError(
                    "Cannot use object of type '%s' as argument to @depends"
                    % type(arg))
            resolved_args.append(resolved_arg)
        dependencies = tuple(dependencies)

        def decorator(func):
            if inspect.isgeneratorfunction(func):
                raise ConfigureError(
                    'Cannot decorate generator functions with @depends')
            func, glob = self._prepare_function(func)
            dummy = wraps(func)(DummyFunction())
            self._depends[dummy] = func, dependencies
            with_help = self._help_option in dependencies
            if with_help:
                for arg in args:
                    if isinstance(arg, DummyFunction):
                        _, deps = self._depends[arg]
                        if self._help_option not in deps:
                            raise ConfigureError(
                                "`%s` depends on '--help' and `%s`. "
                                "`%s` must depend on '--help'"
                                % (func.__name__, arg.__name__, arg.__name__))

            if not self._help or with_help:
                self._results[func] = func(*resolved_args)

            return dummy

        return decorator

    def include_impl(self, what):
        '''Implementation of include().
        Allows to include external files for execution in the sandbox.
        It is possible to use a @depends function as argument, in which case
        the result of the function is the file name to include. This latter
        feature is only really meant for --enable-application/--enable-project.
        '''
        what = self._resolve(what)
        if what:
            if not isinstance(what, types.StringTypes):
                raise TypeError("Unexpected type: '%s'" % type(what))
            self.exec_file(what)

    def template_impl(self, func):
        '''Implementation of @template.
        This function is a decorator. Template functions are called
        immediately. They are altered so that their global namespace exposes
        a limited set of functions from os.path, as well as `advanced`,
        `depends` and `option`.
        Templates allow to simplify repetitive constructs, or to implement
        helper decorators and somesuch.
        '''
        template, glob = self._prepare_function(func)
        glob.update(
            (k[:-len('_impl')], getattr(self, k))
            for k in dir(self) if k.endswith('_impl') and k != 'template_impl'
        )
        self._templates.add(template)
        return template

    def advanced_impl(self, func):
        '''Implementation of @advanced.
        This function gives the decorated function access to the complete set
        of builtins, allowing the import keyword as an expected side effect.
        '''
        func, glob = self._prepare_function(func)
        glob.update(__builtins__=__builtins__)
        return func

    def _resolve_and_set(self, data, name, value):
        # Don't set anything when --help was on the command line
        if self._help:
            return
        name = self._resolve(name, need_help_dependency=False)
        if name is None:
            return
        if not isinstance(name, types.StringTypes):
            raise TypeError("Unexpected type: '%s'" % type(name))
        if name in data:
            raise ConfigureError(
                "Cannot add '%s' to configuration: Key already "
                "exists" % name)
        value = self._resolve(value, need_help_dependency=False)
        if value is not None:
            data[name] = value

    def set_config_impl(self, name, value):
        '''Implementation of set_config().
        Set the configuration items with the given name to the given value.
        Both `name` and `value` can be references to @depends functions,
        in which case the result from these functions is used. If the result
        of either function is None, the configuration item is not set.
        '''
        self._resolve_and_set(self._config, name, value)

    def set_define_impl(self, name, value):
        '''Implementation of set_define().
        Set the define with the given name to the given value. Both `name` and
        `value` can be references to @depends functions, in which case the
        result from these functions is used. If the result of either function
        is None, the define is not set. If the result is False, the define is
        explicitly undefined (-U).
        '''
        defines = self._config.setdefault('DEFINES', {})
        self._resolve_and_set(defines, name, value)

    def imply_option_impl(self, option, value, reason=None):
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
        if not reason and isinstance(value, DummyFunction):
            deps = self._depends[value][1]
            possible_reasons = [d for d in deps if d != self._help_option]
            if len(possible_reasons) == 1:
                if isinstance(possible_reasons[0], Option):
                    reason = (self._raw_options.get(possible_reasons[0]) or
                              possible_reasons[0].option)

        if not reason or not isinstance(value, DummyFunction):
            raise ConfigureError(
                "Cannot infer what implies '%s'. Please add a `reason` to "
                "the `imply_option` call."
                % option)

        value = self._resolve(value, need_help_dependency=False)
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
                raise TypeError("Unexpected type: '%s'" % type(value))

            option = value.format(option)
            self._helper.add(option, 'implied')
            self._implied_options[option] = inspect.stack()[1], reason

    def _prepare_function(self, func):
        '''Alter the given function global namespace with the common ground
        for @depends, @template and @advanced.
        '''
        if not inspect.isfunction(func):
            raise TypeError("Unexpected type: '%s'" % type(func))
        if func in self._prepared_functions:
            return func, func.func_globals

        glob = SandboxedGlobal(func.func_globals)
        glob.update(
            __builtins__=self.BUILTINS,
            __file__=self._paths[-1] if self._paths else '',
            os=self.OS,
            log=self.log_impl,
        )
        func = wraps(func)(types.FunctionType(
            func.func_code,
            glob,
            func.__name__,
            func.func_defaults,
            func.func_closure
        ))
        self._prepared_functions.add(func)
        return func, glob
