# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
import types
from collections import OrderedDict


def istupleofstrings(obj):
    return isinstance(obj, tuple) and len(obj) and all(
        isinstance(o, types.StringTypes) for o in obj)


class OptionValue(tuple):
    '''Represents the value of a configure option.

    This class is not meant to be used directly. Use its subclasses instead.

    The `origin` attribute holds where the option comes from (e.g. environment,
    command line, or default)
    '''
    def __new__(cls, values=(), origin='unknown'):
        return super(OptionValue, cls).__new__(cls, values)

    def __init__(self, values=(), origin='unknown'):
        self.origin = origin

    def format(self, option):
        if option.startswith('--'):
            prefix, name, values = Option.split_option(option)
            assert values == ()
            for prefix_set in (
                    ('disable', 'enable'),
                    ('without', 'with'),
            ):
                if prefix in prefix_set:
                    prefix = prefix_set[int(bool(self))]
                    break
            if prefix:
                option = '--%s-%s' % (prefix, name)
            elif self:
                option = '--%s' % name
            else:
                return ''
            if len(self):
                return '%s=%s' % (option, ','.join(self))
            return option
        elif self and not len(self):
            return '%s=1' % option
        return '%s=%s' % (option, ','.join(self))

    def __eq__(self, other):
        # This is to catch naive comparisons against strings and other
        # types in moz.configure files, as it is really easy to write
        # value == 'foo'.
        if not isinstance(other, tuple):
            raise TypeError('cannot compare a %s against an %s; OptionValue '
                            'instances are tuples - did you mean to compare '
                            'against member elements using [x]?' % (
                                type(other).__name__, type(self).__name__))

        # Allow explicit tuples to be compared.
        if type(other) == tuple:
            return tuple.__eq__(self, other)
        # Else we're likely an OptionValue class.
        elif type(other) != type(self):
            return False
        else:
            return super(OptionValue, self).__eq__(other)

    def __ne__(self, other):
        return not self.__eq__(other)

    def __repr__(self):
        return '%s%s' % (self.__class__.__name__,
                         super(OptionValue, self).__repr__())


class PositiveOptionValue(OptionValue):
    '''Represents the value for a positive option (--enable/--with/--foo)
    in the form of a tuple for when values are given to the option (in the form
    --option=value[,value2...].
    '''
    def __nonzero__(self):
        return True


class NegativeOptionValue(OptionValue):
    '''Represents the value for a negative option (--disable/--without)

    This is effectively an empty tuple with a `origin` attribute.
    '''
    def __new__(cls, origin='unknown'):
        return super(NegativeOptionValue, cls).__new__(cls, origin=origin)

    def __init__(self, origin='unknown'):
        return super(NegativeOptionValue, self).__init__(origin=origin)


class InvalidOptionError(Exception):
    pass


class ConflictingOptionError(InvalidOptionError):
    def __init__(self, message, **format_data):
        if format_data:
            message = message.format(**format_data)
        super(ConflictingOptionError, self).__init__(message)
        for k, v in format_data.iteritems():
            setattr(self, k, v)


class Option(object):
    '''Represents a configure option

    A configure option can be a command line flag or an environment variable
    or both.

    - `name` is the full command line flag (e.g. --enable-foo).
    - `env` is the environment variable name (e.g. ENV)
    - `nargs` is the number of arguments the option may take. It can be a
      number or the special values '?' (0 or 1), '*' (0 or more), or '+' (1 or
      more).
    - `default` can be used to give a default value to the option. When the
      `name` of the option starts with '--enable-' or '--with-', the implied
      default is an empty PositiveOptionValue. When it starts with '--disable-'
      or '--without-', the implied default is a NegativeOptionValue.
    - `choices` restricts the set of values that can be given to the option.
    - `help` is the option description for use in the --help output.
    - `possible_origins` is a tuple of strings that are origins accepted for
      this option. Example origins are 'mozconfig', 'implied', and 'environment'.
    '''
    __slots__ = (
        'id', 'prefix', 'name', 'env', 'nargs', 'default', 'choices', 'help',
        'possible_origins',
    )

    def __init__(self, name=None, env=None, nargs=None, default=None,
                 possible_origins=None, choices=None, help=None):
        if not name and not env:
            raise InvalidOptionError(
                'At least an option name or an environment variable name must '
                'be given')
        if name:
            if not isinstance(name, types.StringTypes):
                raise InvalidOptionError('Option must be a string')
            if not name.startswith('--'):
                raise InvalidOptionError('Option must start with `--`')
            if '=' in name:
                raise InvalidOptionError('Option must not contain an `=`')
            if not name.islower():
                raise InvalidOptionError('Option must be all lowercase')
        if env:
            if not isinstance(env, types.StringTypes):
                raise InvalidOptionError(
                    'Environment variable name must be a string')
            if not env.isupper():
                raise InvalidOptionError(
                    'Environment variable name must be all uppercase')
        if nargs not in (None, '?', '*', '+') and not (
                isinstance(nargs, int) and nargs >= 0):
            raise InvalidOptionError(
                "nargs must be a positive integer, '?', '*' or '+'")
        if (not isinstance(default, types.StringTypes) and
                not isinstance(default, (bool, types.NoneType)) and
                not istupleofstrings(default)):
            raise InvalidOptionError(
                'default must be a bool, a string or a tuple of strings')
        if choices and not istupleofstrings(choices):
            raise InvalidOptionError(
                'choices must be a tuple of strings')
        if not help:
            raise InvalidOptionError('A help string must be provided')
        if possible_origins and not istupleofstrings(possible_origins):
            raise InvalidOptionError(
                'possible_origins must be a tuple of strings')
        self.possible_origins = possible_origins

        if name:
            prefix, name, values = self.split_option(name)
            assert values == ()

            # --disable and --without options mean the default is enabled.
            # --enable and --with options mean the default is disabled.
            # However, we allow a default to be given so that the default
            # can be affected by other factors.
            if prefix:
                if default is None:
                    default = prefix in ('disable', 'without')
                elif default is False:
                    prefix = {
                        'disable': 'enable',
                        'without': 'with',
                    }.get(prefix, prefix)
                elif default is True:
                    prefix = {
                        'enable': 'disable',
                        'with': 'without',
                    }.get(prefix, prefix)
        else:
            prefix = ''

        self.prefix = prefix
        self.name = name
        self.env = env
        if default in (None, False):
            self.default = NegativeOptionValue(origin='default')
        elif isinstance(default, tuple):
            self.default = PositiveOptionValue(default, origin='default')
        elif default is True:
            self.default = PositiveOptionValue(origin='default')
        else:
            self.default = PositiveOptionValue((default,), origin='default')
        if nargs is None:
            nargs = 0
            if len(self.default) == 1:
                nargs = '?'
            elif len(self.default) > 1:
                nargs = '*'
            elif choices:
                nargs = 1
        self.nargs = nargs
        has_choices = choices is not None
        if isinstance(self.default, PositiveOptionValue):
            if has_choices and len(self.default) == 0:
                raise InvalidOptionError(
                    'A `default` must be given along with `choices`')
            if not self._validate_nargs(len(self.default)):
                raise InvalidOptionError(
                    "The given `default` doesn't satisfy `nargs`")
            if has_choices and not all(d in choices for d in self.default):
                raise InvalidOptionError(
                    'The `default` value must be one of %s' %
                    ', '.join("'%s'" % c for c in choices))
        elif has_choices:
            maxargs = self.maxargs
            if len(choices) < maxargs and maxargs != sys.maxint:
                raise InvalidOptionError('Not enough `choices` for `nargs`')
        self.choices = choices
        self.help = help

    @staticmethod
    def split_option(option):
        '''Split a flag or variable into a prefix, a name and values

        Variables come in the form NAME=values (no prefix).
        Flags come in the form --name=values or --prefix-name=values
        where prefix is one of 'with', 'without', 'enable' or 'disable'.
        The '=values' part is optional. Values are separated with commas.
        '''
        if not isinstance(option, types.StringTypes):
            raise InvalidOptionError('Option must be a string')

        elements = option.split('=', 1)
        name = elements[0]
        values = tuple(elements[1].split(',')) if len(elements) == 2 else ()
        if name.startswith('--'):
            name = name[2:]
            if not name.islower():
                raise InvalidOptionError('Option must be all lowercase')
            elements = name.split('-', 1)
            prefix = elements[0]
            if len(elements) == 2 and prefix in ('enable', 'disable',
                                                 'with', 'without'):
                return prefix, elements[1], values
        else:
            if name.startswith('-'):
                raise InvalidOptionError(
                    'Option must start with two dashes instead of one')
            if name.islower():
                raise InvalidOptionError(
                    'Environment variable name must be all uppercase')
        return '', name, values

    @staticmethod
    def _join_option(prefix, name):
        # The constraints around name and env in __init__ make it so that
        # we can distinguish between flags and environment variables with
        # islower/isupper.
        if name.isupper():
            assert not prefix
            return name
        elif prefix:
            return '--%s-%s' % (prefix, name)
        return '--%s' % name

    @property
    def option(self):
        if self.prefix or self.name:
            return self._join_option(self.prefix, self.name)
        else:
            return self.env

    @property
    def minargs(self):
        if isinstance(self.nargs, int):
            return self.nargs
        return 1 if self.nargs == '+' else 0

    @property
    def maxargs(self):
        if isinstance(self.nargs, int):
            return self.nargs
        return 1 if self.nargs == '?' else sys.maxint

    def _validate_nargs(self, num):
        minargs, maxargs = self.minargs, self.maxargs
        return num >= minargs and num <= maxargs

    def get_value(self, option=None, origin='unknown'):
        '''Given a full command line option (e.g. --enable-foo=bar) or a
        variable assignment (FOO=bar), returns the corresponding OptionValue.

        Note: variable assignments can come from either the environment or
        from the command line (e.g. `../configure CFLAGS=-O2`)
        '''
        if not option:
            return self.default

        if self.possible_origins and origin not in self.possible_origins:
            raise InvalidOptionError(
                '%s can not be set by %s. Values are accepted from: %s' %
                (option, origin, ', '.join(self.possible_origins)))

        prefix, name, values = self.split_option(option)
        option = self._join_option(prefix, name)

        assert name in (self.name, self.env)

        if prefix in ('disable', 'without'):
            if values != ():
                raise InvalidOptionError('Cannot pass a value to %s' % option)
            return NegativeOptionValue(origin=origin)

        if name == self.env:
            if values == ('',):
                return NegativeOptionValue(origin=origin)
            if self.nargs in (0, '?', '*') and values == ('1',):
                return PositiveOptionValue(origin=origin)

        values = PositiveOptionValue(values, origin=origin)

        if not self._validate_nargs(len(values)):
            raise InvalidOptionError('%s takes %s value%s' % (
                option,
                {
                    '?': '0 or 1',
                    '*': '0 or more',
                    '+': '1 or more',
                }.get(self.nargs, str(self.nargs)),
                's' if (not isinstance(self.nargs, int) or
                        self.nargs != 1) else ''
            ))

        if len(values) and self.choices:
            relative_result = None
            for val in values:
                if self.nargs in ('+', '*'):
                    if val.startswith(('+', '-')):
                        if relative_result is None:
                            relative_result = list(self.default)
                        sign = val[0]
                        val = val[1:]
                        if sign == '+':
                            if val not in relative_result:
                                relative_result.append(val)
                        else:
                            try:
                                relative_result.remove(val)
                            except ValueError:
                                pass

                if val not in self.choices:
                    raise InvalidOptionError(
                        "'%s' is not one of %s"
                        % (val, ', '.join("'%s'" % c for c in self.choices)))

            if relative_result is not None:
                values = PositiveOptionValue(relative_result, origin=origin)

        return values

    def __repr__(self):
        return '<%s.%s [%s]>' % (self.__class__.__module__,
                                 self.__class__.__name__, self.option)


class CommandLineHelper(object):
    '''Helper class to handle the various ways options can be given either
    on the command line of through the environment.

    For instance, an Option('--foo', env='FOO') can be passed as --foo on the
    command line, or as FOO=1 in the environment *or* on the command line.

    If multiple variants are given, command line is prefered over the
    environment, and if different values are given on the command line, the
    last one wins. (This mimicks the behavior of autoconf, avoiding to break
    existing mozconfigs using valid options in weird ways)

    Extra options can be added afterwards through API calls. For those,
    conflicting values will raise an exception.
    '''
    def __init__(self, environ=os.environ, argv=sys.argv):
        self._environ = dict(environ)
        self._args = OrderedDict()
        self._extra_args = OrderedDict()
        self._origins = {}
        self._last = 0

        assert(argv and not argv[0].startswith('--'))
        for arg in argv[1:]:
            self.add(arg, 'command-line', self._args)

    def add(self, arg, origin='command-line', args=None):
        assert origin != 'default'
        prefix, name, values = Option.split_option(arg)
        if args is None:
            args = self._extra_args
        if args is self._extra_args and name in self._extra_args:
            old_arg = self._extra_args[name][0]
            old_prefix, _, old_values = Option.split_option(old_arg)
            if prefix != old_prefix or values != old_values:
                raise ConflictingOptionError(
                    "Cannot add '{arg}' to the {origin} set because it "
                    "conflicts with '{old_arg}' that was added earlier",
                    arg=arg, origin=origin, old_arg=old_arg,
                    old_origin=self._origins[old_arg])
        self._last += 1
        args[name] = arg, self._last
        self._origins[arg] = origin

    def _prepare(self, option, args):
        arg = None
        origin = 'command-line'
        from_name = args.get(option.name)
        from_env = args.get(option.env)
        if from_name and from_env:
            arg1, pos1 = from_name
            arg2, pos2 = from_env
            arg, pos = (arg1, pos1) if abs(pos1) > abs(pos2) else (arg2, pos2)
            if args is self._extra_args and (option.get_value(arg1) !=
                                             option.get_value(arg2)):
                origin = self._origins[arg]
                old_arg = arg2 if abs(pos1) > abs(pos2) else arg1
                raise ConflictingOptionError(
                    "Cannot add '{arg}' to the {origin} set because it "
                    "conflicts with '{old_arg}' that was added earlier",
                    arg=arg, origin=origin, old_arg=old_arg,
                    old_origin=self._origins[old_arg])
        elif from_name or from_env:
            arg, pos = from_name if from_name else from_env
        elif option.env and args is self._args:
            env = self._environ.get(option.env)
            if env is not None:
                arg = '%s=%s' % (option.env, env)
                origin = 'environment'

        origin = self._origins.get(arg, origin)

        for k in (option.name, option.env):
            try:
                del args[k]
            except KeyError:
                pass

        return arg, origin

    def handle(self, option):
        '''Return the OptionValue corresponding to the given Option instance,
        depending on the command line, environment, and extra arguments, and
        the actual option or variable that set it.
        Only works once for a given Option.
        '''
        assert isinstance(option, Option)

        arg, origin = self._prepare(option, self._args)
        ret = option.get_value(arg, origin)

        extra_arg, extra_origin = self._prepare(option, self._extra_args)
        extra_ret = option.get_value(extra_arg, extra_origin)

        if extra_ret.origin == 'default':
            return ret, arg

        if ret.origin != 'default' and extra_ret != ret:
            raise ConflictingOptionError(
                "Cannot add '{arg}' to the {origin} set because it conflicts "
                "with {old_arg} from the {old_origin} set", arg=extra_arg,
                origin=extra_ret.origin, old_arg=arg, old_origin=ret.origin)

        return extra_ret, extra_arg

    def __iter__(self):
        for d in (self._args, self._extra_args):
            for arg, pos in d.itervalues():
                yield arg
