from typing import Any, List

import attr


# Typing via "type" Argument ---


@attr.s
class C:
    a = attr.ib(type=int)


c = C(1)
C(a=1)


@attr.s
class D:
    x = attr.ib(type=List[int])


@attr.s
class E:
    y = attr.ib(type="List[int]")


@attr.s
class F:
    z = attr.ib(type=Any)


# Typing via Annotations ---


@attr.s
class CC:
    a: int = attr.ib()


cc = CC(1)
CC(a=1)


@attr.s
class DD:
    x: List[int] = attr.ib()


@attr.s
class EE:
    y: "List[int]" = attr.ib()


@attr.s
class FF:
    z: Any = attr.ib()


# Inheritance --


@attr.s
class GG(DD):
    y: str = attr.ib()


GG(x=[1], y="foo")


@attr.s
class HH(DD, EE):
    z: float = attr.ib()


HH(x=[1], y=[], z=1.1)


# same class
c == cc


# Exceptions
@attr.s(auto_exc=True)
class Error(Exception):
    x = attr.ib()


try:
    raise Error(1)
except Error as e:
    e.x
    e.args
    str(e)


# Converters
# XXX: Currently converters can only be functions so none of this works
# although the stubs should be correct.

# @attr.s
# class ConvCOptional:
#     x: Optional[int] = attr.ib(converter=attr.converters.optional(int))


# ConvCOptional(1)
# ConvCOptional(None)


# @attr.s
# class ConvCDefaultIfNone:
#     x: int = attr.ib(converter=attr.converters.default_if_none(42))


# ConvCDefaultIfNone(1)
# ConvCDefaultIfNone(None)
