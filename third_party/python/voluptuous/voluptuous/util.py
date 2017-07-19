import sys

try:
    from error import LiteralInvalid, TypeInvalid, Invalid
    from schema_builder import Schema, default_factory, raises
    import validators
except ImportError:
    from .error import LiteralInvalid, TypeInvalid, Invalid
    from .schema_builder import Schema, default_factory, raises
    from . import validators

__author__ = 'tusharmakkar08'


def Lower(v):
    """Transform a string to lower case.

    >>> s = Schema(Lower)
    >>> s('HI')
    'hi'
    """
    return str(v).lower()


def Upper(v):
    """Transform a string to upper case.

    >>> s = Schema(Upper)
    >>> s('hi')
    'HI'
    """
    return str(v).upper()


def Capitalize(v):
    """Capitalise a string.

    >>> s = Schema(Capitalize)
    >>> s('hello world')
    'Hello world'
    """
    return str(v).capitalize()


def Title(v):
    """Title case a string.

    >>> s = Schema(Title)
    >>> s('hello world')
    'Hello World'
    """
    return str(v).title()


def Strip(v):
    """Strip whitespace from a string.

    >>> s = Schema(Strip)
    >>> s('  hello world  ')
    'hello world'
    """
    return str(v).strip()


class DefaultTo(object):
    """Sets a value to default_value if none provided.

    >>> s = Schema(DefaultTo(42))
    >>> s(None)
    42
    >>> s = Schema(DefaultTo(list))
    >>> s(None)
    []
    """

    def __init__(self, default_value, msg=None):
        self.default_value = default_factory(default_value)
        self.msg = msg

    def __call__(self, v):
        if v is None:
            v = self.default_value()
        return v

    def __repr__(self):
        return 'DefaultTo(%s)' % (self.default_value(),)


class SetTo(object):
    """Set a value, ignoring any previous value.

    >>> s = Schema(validators.Any(int, SetTo(42)))
    >>> s(2)
    2
    >>> s("foo")
    42
    """

    def __init__(self, value):
        self.value = default_factory(value)

    def __call__(self, v):
        return self.value()

    def __repr__(self):
        return 'SetTo(%s)' % (self.value(),)


class Set(object):
    """Convert a list into a set.

    >>> s = Schema(Set())
    >>> s([]) == set([])
    True
    >>> s([1, 2]) == set([1, 2])
    True
    >>> with raises(Invalid, regex="^cannot be presented as set: "):
    ...   s([set([1, 2]), set([3, 4])])
    """

    def __init__(self, msg=None):
        self.msg = msg

    def __call__(self, v):
        try:
            set_v = set(v)
        except Exception as e:
            raise TypeInvalid(
                self.msg or 'cannot be presented as set: {0}'.format(e))
        return set_v

    def __repr__(self):
        return 'Set()'


class Literal(object):
    def __init__(self, lit):
        self.lit = lit

    def __call__(self, value, msg=None):
        if self.lit != value:
            raise LiteralInvalid(
                msg or '%s not match for %s' % (value, self.lit)
            )
        else:
            return self.lit

    def __str__(self):
        return str(self.lit)

    def __repr__(self):
        return repr(self.lit)


def u(x):
    if sys.version_info < (3,):
        return unicode(x)
    else:
        return x
