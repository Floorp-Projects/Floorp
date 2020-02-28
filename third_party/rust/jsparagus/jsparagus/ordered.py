""" Deterministic data structures. """


class OrderedSet:
    """Like set(), but iteration order is insertion order.

    Two OrderedSets, x and y, that have different insertion order are still
    considered equal (x == y) if they contain the same elements.
    """
    def __init__(self, values=()):
        self._data = {}
        for v in values:
            self.add(v)

    def __repr__(self):
        return self.__class__.__name__ + "(" + repr(list(self)) + ")"

    def add(self, v):
        self._data[v] = 1

    def remove(self, v):
        del self._data[v]

    def __eq__(self, other):
        return isinstance(other, OrderedSet) and self._data == other._data

    def __hash__(self):
        raise TypeError("unhashable type: " + self.__class__.__name__)

    def __len__(self):
        return len(self._data)

    def __bool__(self):
        return bool(self._data)

    def __contains__(self, v):
        return v in self._data

    def __iter__(self):
        return iter(self._data)

    def __ior__(self, other):
        for v in other:
            self.add(v)
        return self

    def __or__(self, other):
        u = self.__class__(self)
        u |= other
        return u

    def __iand__(self, other):
        self._data = {v: 1 for v in self if v in other}
        return self

    def __sub__(self, other):
        return OrderedSet(v for v in self if v not in other)

    def __isub__(self, other):
        for v in other:
            if v in self:
                self.remove(v)
        return self


class OrderedFrozenSet:
    """Like frozenset(), but iteration order is insertion order.

    Two OrderedFrozenSets, x and y, that have different insertion order are
    still considered equal (x == y) if they contain the same elements.
    """
    __slots__ = ['_data', '_hash']

    def __init__(self, values=()):
        self._data = {v: 1 for v in values}
        self._hash = None

    def __repr__(self):
        return self.__class__.__name__ + "(" + repr(list(self)) + ")"

    def __len__(self):
        return len(self._data)

    def __bool__(self):
        return bool(self._data)

    def __contains__(self, v):
        return v in self._data

    def __iter__(self):
        return iter(self._data)

    def __eq__(self, other):
        return isinstance(other, OrderedFrozenSet) and self._data == other._data

    def __hash__(self):
        if self._hash is None:
            self._hash = hash(frozenset(self._data))
        return self._hash

    def __and__(self, other):
        return OrderedFrozenSet(v for v in self._data if v in other)

    def __or__(self, other):
        return OrderedFrozenSet(list(self) + list(other))

    def __sub__(self, other):
        return OrderedFrozenSet(v for v in self._data if v not in other)
