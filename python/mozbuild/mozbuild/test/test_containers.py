# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import unittest

from mozunit import main

from mozbuild.util import (
    KeyedDefaultDict,
    List,
    OrderedDefaultDict,
    ReadOnlyNamespace,
    ReadOnlyDefaultDict,
    ReadOnlyDict,
    ReadOnlyKeyedDefaultDict,
)

from collections import OrderedDict


class TestReadOnlyNamespace(unittest.TestCase):
    def test_basic(self):
        test = ReadOnlyNamespace(foo=1, bar=2)

        self.assertEqual(test.foo, 1)
        self.assertEqual(test.bar, 2)
        self.assertEqual(
            sorted(i for i in dir(test) if not i.startswith("__")), ["bar", "foo"]
        )

        with self.assertRaises(AttributeError):
            test.missing

        with self.assertRaises(Exception):
            test.foo = 2

        with self.assertRaises(Exception):
            del test.foo

        self.assertEqual(test, test)
        self.assertEqual(test, ReadOnlyNamespace(foo=1, bar=2))
        self.assertNotEqual(test, ReadOnlyNamespace(foo="1", bar=2))
        self.assertNotEqual(test, ReadOnlyNamespace(foo=1, bar=2, qux=3))
        self.assertNotEqual(test, ReadOnlyNamespace(foo=1, qux=3))
        self.assertNotEqual(test, ReadOnlyNamespace(foo=3, bar="42"))


class TestReadOnlyDict(unittest.TestCase):
    def test_basic(self):
        original = {"foo": 1, "bar": 2}

        test = ReadOnlyDict(original)

        self.assertEqual(original, test)
        self.assertEqual(test["foo"], 1)

        with self.assertRaises(KeyError):
            test["missing"]

        with self.assertRaises(Exception):
            test["baz"] = True

    def test_update(self):
        original = {"foo": 1, "bar": 2}

        test = ReadOnlyDict(original)

        with self.assertRaises(Exception):
            test.update(foo=2)

        self.assertEqual(original, test)

    def test_del(self):
        original = {"foo": 1, "bar": 2}

        test = ReadOnlyDict(original)

        with self.assertRaises(Exception):
            del test["foo"]

        self.assertEqual(original, test)


class TestReadOnlyDefaultDict(unittest.TestCase):
    def test_simple(self):
        original = {"foo": 1, "bar": 2}

        test = ReadOnlyDefaultDict(bool, original)

        self.assertEqual(original, test)

        self.assertEqual(test["foo"], 1)

    def test_assignment(self):
        test = ReadOnlyDefaultDict(bool, {})

        with self.assertRaises(Exception):
            test["foo"] = True

    def test_defaults(self):
        test = ReadOnlyDefaultDict(bool, {"foo": 1})

        self.assertEqual(test["foo"], 1)

        self.assertEqual(test["qux"], False)


class TestList(unittest.TestCase):
    def test_add_list(self):
        test = List([1, 2, 3])

        test += [4, 5, 6]
        self.assertIsInstance(test, List)
        self.assertEqual(test, [1, 2, 3, 4, 5, 6])

        test = test + [7, 8]
        self.assertIsInstance(test, List)
        self.assertEqual(test, [1, 2, 3, 4, 5, 6, 7, 8])

    def test_add_string(self):
        test = List([1, 2, 3])

        with self.assertRaises(ValueError):
            test += "string"

    def test_none(self):
        """As a special exception, we allow None to be treated as an empty
        list."""
        test = List([1, 2, 3])

        test += None
        self.assertEqual(test, [1, 2, 3])

        test = test + None
        self.assertIsInstance(test, List)
        self.assertEqual(test, [1, 2, 3])

        with self.assertRaises(ValueError):
            test += False

        with self.assertRaises(ValueError):
            test = test + False


class TestOrderedDefaultDict(unittest.TestCase):
    def test_simple(self):
        original = OrderedDict(foo=1, bar=2)

        test = OrderedDefaultDict(bool, original)

        self.assertEqual(original, test)

        self.assertEqual(test["foo"], 1)

        self.assertEqual(list(test), ["foo", "bar"])

    def test_defaults(self):
        test = OrderedDefaultDict(bool, {"foo": 1})

        self.assertEqual(test["foo"], 1)

        self.assertEqual(test["qux"], False)

        self.assertEqual(list(test), ["foo", "qux"])


class TestKeyedDefaultDict(unittest.TestCase):
    def test_simple(self):
        original = {"foo": 1, "bar": 2}

        test = KeyedDefaultDict(lambda x: x, original)

        self.assertEqual(original, test)

        self.assertEqual(test["foo"], 1)

    def test_defaults(self):
        test = KeyedDefaultDict(lambda x: x, {"foo": 1})

        self.assertEqual(test["foo"], 1)

        self.assertEqual(test["qux"], "qux")

        self.assertEqual(test["bar"], "bar")

        test["foo"] = 2
        test["qux"] = None
        test["baz"] = "foo"

        self.assertEqual(test["foo"], 2)

        self.assertEqual(test["qux"], None)

        self.assertEqual(test["baz"], "foo")


class TestReadOnlyKeyedDefaultDict(unittest.TestCase):
    def test_defaults(self):
        test = ReadOnlyKeyedDefaultDict(lambda x: x, {"foo": 1})

        self.assertEqual(test["foo"], 1)

        self.assertEqual(test["qux"], "qux")

        self.assertEqual(test["bar"], "bar")

        copy = dict(test)

        with self.assertRaises(Exception):
            test["foo"] = 2

        with self.assertRaises(Exception):
            test["qux"] = None

        with self.assertRaises(Exception):
            test["baz"] = "foo"

        self.assertEqual(test, copy)

        self.assertEqual(len(test), 3)


if __name__ == "__main__":
    main()
