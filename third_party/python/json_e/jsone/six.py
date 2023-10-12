import sys
import operator

# https://github.com/benjaminp/six/blob/2c3492a9f16d294cd5e6b43d6110c5a3a2e58b4c/six.py#L818


def with_metaclass(meta, *bases):
    """Create a base class with a metaclass."""
    # This requires a bit of explanation: the basic idea is to make a dummy
    # metaclass for one level of class instantiation that replaces itself with
    # the actual metaclass.
    class metaclass(meta):

        def __new__(cls, name, this_bases, d):
            return meta(name, bases, d)
    return type.__new__(metaclass, 'temporary_class', (), {})


# https://github.com/benjaminp/six/blob/2c3492a9f16d294cd5e6b43d6110c5a3a2e58b4c/six.py#L578
if sys.version_info[0] == 3:
    viewitems = operator.methodcaller("items")
else:
    viewitems = operator.methodcaller("viewitems")
