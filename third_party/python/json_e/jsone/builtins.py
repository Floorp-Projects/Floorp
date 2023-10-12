from __future__ import absolute_import, print_function, unicode_literals

import math
from .shared import string, to_str, fromNow, JSONTemplateError


class BuiltinError(JSONTemplateError):
    pass


def build():
    builtins = {}

    def builtin(name, variadic=None, argument_tests=None, minArgs=None, needs_context=False):
        def wrap(fn):
            if variadic:
                def invoke(context, *args):
                    if minArgs:
                        if len(args) < minArgs:
                            raise BuiltinError(
                                'invalid arguments to builtin: {}: expected at least {} arguments'.format(name, minArgs)
                            )
                    for arg in args:
                        if not variadic(arg):
                            raise BuiltinError('invalid arguments to builtin: {}'.format(name))
                    if needs_context is True:
                        return fn(context, *args)
                    return fn(*args)

            elif argument_tests:
                def invoke(context, *args):
                    if len(args) != len(argument_tests):
                        raise BuiltinError('invalid arguments to builtin: {}'.format(name))
                    for t, arg in zip(argument_tests, args):
                        if not t(arg):
                            raise BuiltinError('invalid arguments to builtin: {}'.format(name))
                    if needs_context is True:
                        return fn(context, *args)
                    return fn(*args)

            else:
                def invoke(context, *args):
                    if needs_context is True:
                        return fn(context, *args)
                    return fn(*args)

            invoke._jsone_builtin = True
            builtins[name] = invoke
            return fn

        return wrap

    def is_number(v):
        return isinstance(v, (int, float)) and not isinstance(v, bool)

    def is_string(v):
        return isinstance(v, string)

    def is_string_or_number(v):
        return is_string(v) or is_number(v)

    def is_array(v):
        return isinstance(v, list)

    def is_string_or_array(v):
        return isinstance(v, (string, list))

    def anything_except_array(v):
        return isinstance(v, (string, int, float, bool)) or v is None

    def anything(v):
        return isinstance(v, (string, int, float, list, dict)) or v is None or callable(v)

    # ---

    builtin('min', variadic=is_number, minArgs=1)(min)
    builtin('max', variadic=is_number, minArgs=1)(max)
    builtin('sqrt', argument_tests=[is_number])(math.sqrt)
    builtin('abs', argument_tests=[is_number])(abs)

    @builtin('ceil', argument_tests=[is_number])
    def ceil(v):
        return int(math.ceil(v))

    @builtin('floor', argument_tests=[is_number])
    def floor(v):
        return int(math.floor(v))

    @builtin('lowercase', argument_tests=[is_string])
    def lowercase(v):
        return v.lower()

    @builtin('uppercase', argument_tests=[is_string])
    def lowercase(v):
        return v.upper()

    builtin('len', argument_tests=[is_string_or_array])(len)
    builtin('str', argument_tests=[anything_except_array])(to_str)
    builtin('number', variadic=is_string, minArgs=1)(float)

    @builtin('strip', argument_tests=[is_string])
    def strip(s):
        return s.strip()

    @builtin('rstrip', argument_tests=[is_string])
    def rstrip(s):
        return s.rstrip()

    @builtin('lstrip', argument_tests=[is_string])
    def lstrip(s):
        return s.lstrip()

    @builtin('join', argument_tests=[is_array, is_string_or_number])
    def join(list, separator):
        # convert potential numbers into strings
        string_list = [str(int) for int in list]

        return str(separator).join(string_list)

    @builtin('split', variadic=is_string_or_number, minArgs=1)
    def split(s, d=''):
        if not d and is_string(s):
            return list(s)

        return s.split(to_str(d))

    @builtin('fromNow', variadic=is_string, minArgs=1, needs_context=True)
    def fromNow_builtin(context, offset, reference=None):
        return fromNow(offset, reference or context.get('now'))

    @builtin('typeof', argument_tests=[anything])
    def typeof(v):
        if isinstance(v, bool):
            return 'boolean'
        elif isinstance(v, string):
            return 'string'
        elif isinstance(v, (int, float)):
            return 'number'
        elif isinstance(v, list):
            return 'array'
        elif isinstance(v, dict):
            return 'object'
        elif v is None:
            return 'null'
        elif callable(v):
            return 'function'

    @builtin('defined', argument_tests=[is_string], needs_context=True)
    def defined(context, s):
        if s not in context:
            return False
        else:
            return True

    return builtins
