""" Deterministic data structures. """

from __future__ import annotations
# mypy: disallow-untyped-defs, disallow-incomplete-defs, disallow-untyped-calls

from typing import (AbstractSet, Dict, Generic, Iterable, Iterator, List,
                    MutableSet, Optional, TypeVar, Union)


__all__ = ['OrderedSet', 'OrderedFrozenSet']


# Type parameters for OrderedSet[T] and OrderedFrozenSet[T].
#
# These should be `bound=Hashable`, but I gather MyPy has some issues with
# hashability. It doesn't enforce hashability on Dict and Set.
T = TypeVar('T')
T_co = TypeVar('T_co', covariant=True)

S = TypeVar('S')


class OrderedSet(Generic[T], MutableSet[T]):
    """Like set(), but iteration order is insertion order.

    Two OrderedSets, x and y, that have different insertion order are still
    considered equal (x == y) if they contain the same elements.
    """
    _data: Dict[T, int]

    def __init__(self, values: Iterable[T] = ()):
        self._data = {}
        for v in values:
            self.add(v)

    def __repr__(self) -> str:
        return self.__class__.__name__ + "(" + repr(list(self)) + ")"

    def add(self, v: T) -> None:
        self._data[v] = 1

    def extend(self, iterable: Iterable[T]) -> None:
        for v in iterable:
            self.add(v)

    def remove(self, v: T) -> None:
        del self._data[v]

    def discard(self, v: T) -> None:
        if v in self._data:
            del self._data[v]

    def __eq__(self, other: object) -> bool:
        return isinstance(other, OrderedSet) and self._data == other._data

    def __hash__(self) -> int:
        raise TypeError("unhashable type: " + self.__class__.__name__)

    def __len__(self) -> int:
        return len(self._data)

    def __bool__(self) -> bool:
        return bool(self._data)

    def __contains__(self, v: object) -> bool:
        return v in self._data

    def __iter__(self) -> Iterator[T]:
        return iter(self._data)

    def __ior__(self, other: AbstractSet[S]) -> OrderedSet[Union[T, S]]:
        for v in other:
            self.add(v)  # type: ignore
        return self  # type: ignore

    def __or__(self, other: AbstractSet[S]) -> OrderedSet[Union[T, S]]:
        u: OrderedSet[Union[T, S]] = OrderedSet(self)
        u |= other
        return u

    def __iand__(self, other: AbstractSet[T]) -> OrderedSet[T]:
        self._data = {v: 1 for v in self if v in other}
        return self

    def __and__(self, other: AbstractSet[T]) -> OrderedSet[T]:
        return OrderedSet(v for v in self if v in other)

    def __sub__(self, other: AbstractSet[T]) -> OrderedSet[T]:
        return OrderedSet(v for v in self if v not in other)

    def __isub__(self, other: AbstractSet[T]) -> OrderedSet[T]:
        for v in other:
            if v in self:
                self.remove(v)
        return self


class OrderedFrozenSet(Generic[T_co], AbstractSet[T_co]):
    """Like frozenset(), but iteration order is insertion order.

    Two OrderedFrozenSets, x and y, that have different insertion order are
    still considered equal (x == y) if they contain the same elements.
    """
    __slots__ = ['_data', '_hash']

    _data: Dict[T_co, int]
    _hash: Optional[int]

    def __init__(self, values: Iterable[T_co] = ()):
        self._data = {v: 1 for v in values}
        self._hash = None

    def __repr__(self) -> str:
        return self.__class__.__name__ + "(" + repr(list(self)) + ")"

    def __len__(self) -> int:
        return len(self._data)

    def __bool__(self) -> bool:
        return bool(self._data)

    def __contains__(self, v: object) -> bool:
        return v in self._data

    def __iter__(self) -> Iterator[T_co]:
        return iter(self._data)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, OrderedFrozenSet) and self._data == other._data

    def __hash__(self) -> int:
        if self._hash is None:
            self._hash = hash(frozenset(self._data))
        return self._hash

    def __and__(self, other: AbstractSet[T_co]) -> OrderedFrozenSet[T_co]:
        return OrderedFrozenSet(v for v in self._data if v in other)

    def __or__(self, other: AbstractSet[S]) -> OrderedFrozenSet[Union[T_co, S]]:
        values: List[Union[T_co, S]] = list(self)
        values += list(other)
        return OrderedFrozenSet(values)

    def __sub__(self, other: AbstractSet[T_co]) -> OrderedFrozenSet[T_co]:
        return OrderedFrozenSet(v for v in self._data if v not in other)
