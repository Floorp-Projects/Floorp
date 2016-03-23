# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
import os
import sys
import types
from collections import OrderedDict
from functools import wraps
from mozbuild.configure.options import (
    CommandLineHelper,
    ConflictingOptionError,
    InvalidOptionError,
    Option,
    OptionValue,
)
from mozbuild.configure.help import HelpFormatter
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


class DependsOutput(dict):
    '''Dict holding the results yielded by a @depends function.'''
    __slots__ = ('implied_options',)

    def __init__(self):
        super(DependsOutput, self).__init__()
        self.implied_options = []

    def imply_option(self, option, reason=None):
        if not isinstance(option, types.StringTypes):
            raise TypeError('imply_option must be given a string')
        self.implied_options.append((option, reason))


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
    OS = ReadOnlyNamespace(path=ReadOnlyNamespace(
        abspath=mozpath.abspath,
        basename=mozpath.basename,
        dirname=mozpath.dirname,
        exists=os.path.exists,
        isabs=os.path.isabs,
        isdir=os.path.isdir,
        isfile=os.path.isfile,
        join=mozpath.join,
        normpath=mozpath.normpath,
        realpath=mozpath.realpath,
        relpath=mozpath.relpath,
    ))

    def __init__(self, config, environ=os.environ, argv=sys.argv,
                 stdout=sys.stdout, stderr=sys.stderr):
        dict.__setitem__(self, '__builtins__', self.BUILTINS)

        self._paths = []
        self._templates = set()
        self._depends = {}
        self._seen = set()

        self._options = OrderedDict()
        # Store the raw values returned by @depends functions
        self._results = {}
        # Store several kind of information:
        # - value for each Option, as per returned by Option.get_value
        # - raw option (as per command line or environment) for each value
        self._db = {}

        # Store options added with `imply_option`, and the reason they were
        # added (which can either have been given to `imply_option`, or
        # infered.
        self._implied_options = {}

        # Store all results from _prepare_function
        self._prepared_functions = set()

        self._helper = CommandLineHelper(environ, argv)

        self._config, self._stdout, self._stderr = config, stdout, stderr

        self._help = None
        self._help_option = self.option_impl('--help',
                                             help='print this message')
        self._seen.add(self._help_option)
        # self._option_impl('--help') will have set this if --help was on the
        # command line.
        if self._db[self._help_option]:
            self._help = HelpFormatter(argv[0])
            self._help.add(self._help_option)

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
                func, reason = self._implied_options[arg]
                raise ConfigureError(
                    '`%s`, emitted by `%s` in `%s`, was not handled.'
                    % (without_value, func.__name__,
                       func.func_code.co_filename))
            raise InvalidOptionError('Unknown option: %s' % without_value)

        # All options must be referenced by some @depends function
        for option in self._options.itervalues():
            if option not in self._seen:
                raise ConfigureError(
                    'Option `%s` is not handled ; reference it with a @depends'
                    % option.option
                )

        if self._help:
            self._help.usage(self._stdout)

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
                value not in self._templates):
            raise KeyError('Cannot assign `%s` because it is neither a '
                           '@depends nor a @template' % key)

        return super(ConfigureSandbox, self).__setitem__(key, value)

    def _resolve(self, arg):
        if isinstance(arg, DummyFunction):
            assert arg in self._depends
            func = self._depends[arg]
            assert not inspect.isgeneratorfunction(func)
            assert func in self._results
            if not func.with_help:
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
            func, reason = self._implied_options[e.arg]
            raise InvalidOptionError(
                "'%s' implied by '%s' conflicts with '%s' from the %s"
                % (e.arg, reason, e.old_arg, e.old_origin))

        if self._help:
            self._help.add(option)

        self._db[option] = value
        self._db[value] = (option_string.split('=', 1)[0]
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
        set of functions from os.path, and two additional functions:
        `imply_option` and `set_config`. The former allows to inject additional
        options as if they had been passed on the command line. The latter
        declares new configuration items for consumption by moz.build.
        '''
        if not args:
            raise ConfigureError('@depends needs at least one argument')

        with_help = False
        resolved_args = []
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
                if arg == self._help_option:
                    with_help = True
                self._seen.add(arg)
                assert arg in self._db or self._help
                resolved_arg = self._db.get(arg)
            elif isinstance(arg, DummyFunction):
                assert arg in self._depends
                arg = self._depends[arg]
                resolved_arg = self._results.get(arg)
            else:
                raise TypeError(
                    "Cannot use object of type '%s' as argument to @depends"
                    % type(arg))
            resolved_args.append(resolved_arg)

        def decorator(func):
            if inspect.isgeneratorfunction(func):
                raise ConfigureError(
                    'Cannot decorate generator functions with @depends')
            func, glob = self._prepare_function(func)
            result = DependsOutput()
            glob.update(
                imply_option=result.imply_option,
                set_config=result.__setitem__,
            )
            dummy = wraps(func)(DummyFunction())
            self._depends[dummy] = func
            func.with_help = with_help
            if with_help:
                for arg in args:
                    if (isinstance(arg, DummyFunction) and
                            not self._depends[arg].with_help):
                        raise ConfigureError(
                            "`%s` depends on '--help' and `%s`. "
                            "`%s` must depend on '--help'"
                            % (func.__name__, arg.__name__, arg.__name__))

            if self._help and not with_help:
                return dummy

            self._results[func] = func(*resolved_args)

            for option, reason in result.implied_options:
                self._helper.add(option, 'implied')
                if not reason:
                    deps = []
                    for name, value in zip(args, resolved_args):
                        if not isinstance(value, OptionValue):
                            raise ConfigureError(
                                "Cannot infer what implied '%s'" % option)
                        if name == '--help':
                            continue
                        deps.append(value.format(self._db.get(value) or name))
                    if len(deps) != 1:
                        raise ConfigureError(
                            "Cannot infer what implied '%s'" % option)
                    reason = deps[0]

                self._implied_options[option] = func, reason

            if not self._help:
                for k, v in result.iteritems():
                    if k in self._config:
                        raise ConfigureError(
                            "Cannot add '%s' to configuration: Key already "
                            "exists" % k)
                    self._config[k] = v

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
            advanced=self.advanced_impl,
            depends=self.depends_impl,
            option=self.option_impl,
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
            __file__=self._paths[-1],
            os=self.OS,
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
