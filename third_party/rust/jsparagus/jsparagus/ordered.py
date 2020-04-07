""" Deterministic data structures. """

from __future__ import annotations
import typing


# Type parameter for OrderedSet[T] and OrderedFrozenSet[T].
#
# This should be `bound=Hashable`, but I gather MyPy has some issues with
# hashability. It doesn't enforce hashability on Dict and Set.
T = typing.TypeVar('T')


class OrderedSet(typing.Generic[T]):
    """Like set(), but iteration order is insertion order.

    Two OrderedSets, x and y, that have different insertion order are still
    considered equal (x == y) if they contain the same elements.
    """
    _data: typing.Dict[T, int]

    def __init__(self, values: typing.Iterable[T] = ()):
        self._data = {}
        for v in values:
            self.add(v)

    def __repr__(self) -> str:
        return self.__class__.__name__ + "(" + repr(list(self)) + ")"

    def add(self, v: T) -> None:
        self._data[v] = 1

    def extend(self, iterable: typing.Iterable[T]) -> None:
        for v in iterable:
            self.add(v)

    def remove(self, v: T) -> None:
        del self._data[v]

    def __eq__(self, other: object) -> bool:
        return isinstance(other, OrderedSet) and self._data == other._data

    def __hash__(self) -> int:
        raise TypeError("unhashable type: " + self.__class__.__name__)

    def __len__(self) -> int:
        return len(self._data)

    def __bool__(self) -> bool:
        return bool(self._data)

    def __contains__(self, v: T) -> bool:
        return v in self._data

    def __iter__(self) -> typing.Iterator[T]:
        return iter(self._data)

    def __ior__(self, other: OrderedSet[T]) -> OrderedSet[T]:
        for v in other:
            self.add(v)
        return self

    def __or__(self, other: OrderedSet[T]) -> OrderedSet[T]:
        u = self.__class__(self)
        u |= other
        return u

    def __iand__(self, other: OrderedSet[T]) -> OrderedSet[T]:
        self._data = {v: 1 for v in self if v in other}
        return self

    def __sub__(self, other: OrderedSet[T]) -> OrderedSet[T]:
        return OrderedSet(v for v in self if v not in other)

    def __isub__(self, other: OrderedSet[T]) -> OrderedSet[T]:
        for v in other:
            if v in self:
                self.remove(v)
        return self


class OrderedFrozenSet(typing.Generic[T]):
    """Like frozenset(), but iteration order is insertion order.

    Two OrderedFrozenSets, x and y, that have different insertion order are
    still considered equal (x == y) if they contain the same elements.
    """
    __slots__ = ['_data', '_hash']

    _data: typing.Dict[T, int]
    _hash: typing.Optional[int]

    def __init__(self, values: typing.Iterable[T] = ()):
        self._data = {v: 1 for v in values}
        self._hash = None

    def __repr__(self) -> str:
        return self.__class__.__name__ + "(" + repr(list(self)) + ")"

    def __len__(self) -> int:
        return len(self._data)

    def __bool__(self) -> bool:
        return bool(self._data)

    def __contains__(self, v: T) -> bool:
        return v in self._data

    def __iter__(self) -> typing.Iterator[T]:
        return iter(self._data)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, OrderedFrozenSet) and self._data == other._data

    def __hash__(self) -> int:
        if self._hash is None:
            self._hash = hash(frozenset(self._data))
        return self._hash

    def __and__(self, other: OrderedFrozenSet[T]) -> OrderedFrozenSet[T]:
        return OrderedFrozenSet(v for v in self._data if v in other)

    def __or__(self, other: OrderedFrozenSet[T]) -> OrderedFrozenSet[T]:
        return OrderedFrozenSet(list(self) + list(other))

    def __sub__(self, other: OrderedFrozenSet[T]) -> OrderedFrozenSet[T]:
        return OrderedFrozenSet(v for v in self._data if v not in other)
