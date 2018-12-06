# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''A tuple with keys.

A Sequence type that allows to refer to its elements by key.
Making this immutable, 'cause keeping track of mutations is hard.

compare-locales uses strings for Entity keys, and tuples in the
case of PO. Support both.

In the interfaces that check for membership, dicts check keys and
sequences check values. Always try our dict cache `__map` first,
and fall back to the superclass implementation.
'''

from __future__ import absolute_import
from __future__ import unicode_literals


class KeyedTuple(tuple):

    def __new__(cls, iterable):
        return super(KeyedTuple, cls).__new__(cls, iterable)

    def __init__(self, iterable):
        self.__map = {}
        if iterable:
            for index, item in enumerate(self):
                self.__map[item.key] = index

    def __contains__(self, key):
        try:
            contains = key in self.__map
            if contains:
                return True
        except TypeError:
            pass
        return super(KeyedTuple, self).__contains__(key)

    def __getitem__(self, key):
        try:
            key = self.__map[key]
        except (KeyError, TypeError):
            pass
        return super(KeyedTuple, self).__getitem__(key)

    def keys(self):
        for value in self:
            yield value.key

    def items(self):
        for value in self:
            yield value.key, value

    def values(self):
        return self
