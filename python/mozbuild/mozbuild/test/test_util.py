# coding: utf-8
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import hashlib
import itertools
import os
import string
import sys
import unittest

import pytest
import six
from mozfile.mozfile import NamedTemporaryFile
from mozunit import main

from mozbuild.util import (
    EnumString,
    EnumStringComparisonError,
    HierarchicalStringList,
    MozbuildDeletionError,
    ReadOnlyDict,
    StrictOrderingOnAppendList,
    StrictOrderingOnAppendListWithAction,
    StrictOrderingOnAppendListWithFlagsFactory,
    TypedList,
    TypedNamedTuple,
    UnsortedError,
    expand_variables,
    group_unified_files,
    hash_file,
    hexdump,
    memoize,
    memoized_property,
    pair,
    resolve_target_to_make,
)

if sys.version_info[0] == 3:
    str_type = "str"
else:
    str_type = "unicode"

data_path = os.path.abspath(os.path.dirname(__file__))
data_path = os.path.join(data_path, "data")


class TestHashing(unittest.TestCase):
    def test_hash_file_known_hash(self):
        """Ensure a known hash value is recreated."""
        data = b"The quick brown fox jumps over the lazy cog"
        expected = "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3"

        temp = NamedTemporaryFile()
        temp.write(data)
        temp.flush()

        actual = hash_file(temp.name)

        self.assertEqual(actual, expected)

    def test_hash_file_large(self):
        """Ensure that hash_file seems to work with a large file."""
        data = b"x" * 1048576

        hasher = hashlib.sha1()
        hasher.update(data)
        expected = hasher.hexdigest()

        temp = NamedTemporaryFile()
        temp.write(data)
        temp.flush()

        actual = hash_file(temp.name)

        self.assertEqual(actual, expected)


class TestResolveTargetToMake(unittest.TestCase):
    def setUp(self):
        self.topobjdir = data_path

    def assertResolve(self, path, expected):
        # Handle Windows path separators.
        (reldir, target) = resolve_target_to_make(self.topobjdir, path)
        if reldir is not None:
            reldir = reldir.replace(os.sep, "/")
        if target is not None:
            target = target.replace(os.sep, "/")
        self.assertEqual((reldir, target), expected)

    def test_root_path(self):
        self.assertResolve("/test-dir", ("test-dir", None))
        self.assertResolve("/test-dir/with", ("test-dir/with", None))
        self.assertResolve("/test-dir/without", ("test-dir", None))
        self.assertResolve("/test-dir/without/with", ("test-dir/without/with", None))

    def test_dir(self):
        self.assertResolve("test-dir", ("test-dir", None))
        self.assertResolve("test-dir/with", ("test-dir/with", None))
        self.assertResolve("test-dir/with", ("test-dir/with", None))
        self.assertResolve("test-dir/without", ("test-dir", None))
        self.assertResolve("test-dir/without/with", ("test-dir/without/with", None))

    def test_top_level(self):
        self.assertResolve("package", (None, "package"))
        # Makefile handling shouldn't affect top-level targets.
        self.assertResolve("Makefile", (None, "Makefile"))

    def test_regular_file(self):
        self.assertResolve("test-dir/with/file", ("test-dir/with", "file"))
        self.assertResolve(
            "test-dir/with/without/file", ("test-dir/with", "without/file")
        )
        self.assertResolve(
            "test-dir/with/without/with/file", ("test-dir/with/without/with", "file")
        )

        self.assertResolve("test-dir/without/file", ("test-dir", "without/file"))
        self.assertResolve(
            "test-dir/without/with/file", ("test-dir/without/with", "file")
        )
        self.assertResolve(
            "test-dir/without/with/without/file",
            ("test-dir/without/with", "without/file"),
        )

    def test_Makefile(self):
        self.assertResolve("test-dir/with/Makefile", ("test-dir", "with/Makefile"))
        self.assertResolve(
            "test-dir/with/without/Makefile", ("test-dir/with", "without/Makefile")
        )
        self.assertResolve(
            "test-dir/with/without/with/Makefile",
            ("test-dir/with", "without/with/Makefile"),
        )

        self.assertResolve(
            "test-dir/without/Makefile", ("test-dir", "without/Makefile")
        )
        self.assertResolve(
            "test-dir/without/with/Makefile", ("test-dir", "without/with/Makefile")
        )
        self.assertResolve(
            "test-dir/without/with/without/Makefile",
            ("test-dir/without/with", "without/Makefile"),
        )


class TestHierarchicalStringList(unittest.TestCase):
    def setUp(self):
        self.EXPORTS = HierarchicalStringList()

    def test_exports_append(self):
        self.assertEqual(self.EXPORTS._strings, [])
        self.EXPORTS += ["foo.h"]
        self.assertEqual(self.EXPORTS._strings, ["foo.h"])
        self.EXPORTS += ["bar.h"]
        self.assertEqual(self.EXPORTS._strings, ["foo.h", "bar.h"])

    def test_exports_subdir(self):
        self.assertEqual(self.EXPORTS._children, {})
        self.EXPORTS.foo += ["foo.h"]
        six.assertCountEqual(self, self.EXPORTS._children, {"foo": True})
        self.assertEqual(self.EXPORTS.foo._strings, ["foo.h"])
        self.EXPORTS.bar += ["bar.h"]
        six.assertCountEqual(self, self.EXPORTS._children, {"foo": True, "bar": True})
        self.assertEqual(self.EXPORTS.foo._strings, ["foo.h"])
        self.assertEqual(self.EXPORTS.bar._strings, ["bar.h"])

    def test_exports_multiple_subdir(self):
        self.EXPORTS.foo.bar = ["foobar.h"]
        six.assertCountEqual(self, self.EXPORTS._children, {"foo": True})
        six.assertCountEqual(self, self.EXPORTS.foo._children, {"bar": True})
        six.assertCountEqual(self, self.EXPORTS.foo.bar._children, {})
        self.assertEqual(self.EXPORTS._strings, [])
        self.assertEqual(self.EXPORTS.foo._strings, [])
        self.assertEqual(self.EXPORTS.foo.bar._strings, ["foobar.h"])

    def test_invalid_exports_append(self):
        with self.assertRaises(ValueError) as ve:
            self.EXPORTS += "foo.h"
        six.assertRegex(
            self,
            str(ve.exception),
            "Expected a list of strings, not <(?:type|class) '%s'>" % str_type,
        )

    def test_invalid_exports_set(self):
        with self.assertRaises(ValueError) as ve:
            self.EXPORTS.foo = "foo.h"

        six.assertRegex(
            self,
            str(ve.exception),
            "Expected a list of strings, not <(?:type|class) '%s'>" % str_type,
        )

    def test_invalid_exports_append_base(self):
        with self.assertRaises(ValueError) as ve:
            self.EXPORTS += "foo.h"

        six.assertRegex(
            self,
            str(ve.exception),
            "Expected a list of strings, not <(?:type|class) '%s'>" % str_type,
        )

    def test_invalid_exports_bool(self):
        with self.assertRaises(ValueError) as ve:
            self.EXPORTS += [True]

        six.assertRegex(
            self,
            str(ve.exception),
            "Expected a list of strings, not an element of " "<(?:type|class) 'bool'>",
        )

    def test_del_exports(self):
        with self.assertRaises(MozbuildDeletionError):
            self.EXPORTS.foo += ["bar.h"]
            del self.EXPORTS.foo

    def test_unsorted(self):
        with self.assertRaises(UnsortedError):
            self.EXPORTS += ["foo.h", "bar.h"]

        with self.assertRaises(UnsortedError):
            self.EXPORTS.foo = ["foo.h", "bar.h"]

        with self.assertRaises(UnsortedError):
            self.EXPORTS.foo += ["foo.h", "bar.h"]

    def test_reassign(self):
        self.EXPORTS.foo = ["foo.h"]

        with self.assertRaises(KeyError):
            self.EXPORTS.foo = ["bar.h"]

    def test_walk(self):
        l = HierarchicalStringList()
        l += ["root1", "root2", "root3"]
        l.child1 += ["child11", "child12", "child13"]
        l.child1.grandchild1 += ["grandchild111", "grandchild112"]
        l.child1.grandchild2 += ["grandchild121", "grandchild122"]
        l.child2.grandchild1 += ["grandchild211", "grandchild212"]
        l.child2.grandchild1 += ["grandchild213", "grandchild214"]

        els = list((path, list(seq)) for path, seq in l.walk())
        self.assertEqual(
            els,
            [
                ("", ["root1", "root2", "root3"]),
                ("child1", ["child11", "child12", "child13"]),
                ("child1/grandchild1", ["grandchild111", "grandchild112"]),
                ("child1/grandchild2", ["grandchild121", "grandchild122"]),
                (
                    "child2/grandchild1",
                    [
                        "grandchild211",
                        "grandchild212",
                        "grandchild213",
                        "grandchild214",
                    ],
                ),
            ],
        )

    def test_merge(self):
        l1 = HierarchicalStringList()
        l1 += ["root1", "root2", "root3"]
        l1.child1 += ["child11", "child12", "child13"]
        l1.child1.grandchild1 += ["grandchild111", "grandchild112"]
        l1.child1.grandchild2 += ["grandchild121", "grandchild122"]
        l1.child2.grandchild1 += ["grandchild211", "grandchild212"]
        l1.child2.grandchild1 += ["grandchild213", "grandchild214"]
        l2 = HierarchicalStringList()
        l2.child1 += ["child14", "child15"]
        l2.child1.grandchild2 += ["grandchild123"]
        l2.child3 += ["child31", "child32"]

        l1 += l2
        els = list((path, list(seq)) for path, seq in l1.walk())
        self.assertEqual(
            els,
            [
                ("", ["root1", "root2", "root3"]),
                ("child1", ["child11", "child12", "child13", "child14", "child15"]),
                ("child1/grandchild1", ["grandchild111", "grandchild112"]),
                (
                    "child1/grandchild2",
                    ["grandchild121", "grandchild122", "grandchild123"],
                ),
                (
                    "child2/grandchild1",
                    [
                        "grandchild211",
                        "grandchild212",
                        "grandchild213",
                        "grandchild214",
                    ],
                ),
                ("child3", ["child31", "child32"]),
            ],
        )


class TestStrictOrderingOnAppendList(unittest.TestCase):
    def test_init(self):
        l = StrictOrderingOnAppendList()
        self.assertEqual(len(l), 0)

        l = StrictOrderingOnAppendList(["a", "b", "c"])
        self.assertEqual(len(l), 3)

        with self.assertRaises(UnsortedError):
            StrictOrderingOnAppendList(["c", "b", "a"])

        self.assertEqual(len(l), 3)

    def test_extend(self):
        l = StrictOrderingOnAppendList()
        l.extend(["a", "b"])
        self.assertEqual(len(l), 2)
        self.assertIsInstance(l, StrictOrderingOnAppendList)

        with self.assertRaises(UnsortedError):
            l.extend(["d", "c"])

        self.assertEqual(len(l), 2)

    def test_slicing(self):
        l = StrictOrderingOnAppendList()
        l[:] = ["a", "b"]
        self.assertEqual(len(l), 2)
        self.assertIsInstance(l, StrictOrderingOnAppendList)

        with self.assertRaises(UnsortedError):
            l[:] = ["b", "a"]

        self.assertEqual(len(l), 2)

    def test_add(self):
        l = StrictOrderingOnAppendList()
        l2 = l + ["a", "b"]
        self.assertEqual(len(l), 0)
        self.assertEqual(len(l2), 2)
        self.assertIsInstance(l2, StrictOrderingOnAppendList)

        with self.assertRaises(UnsortedError):
            l2 = l + ["b", "a"]

        self.assertEqual(len(l), 0)

    def test_iadd(self):
        l = StrictOrderingOnAppendList()
        l += ["a", "b"]
        self.assertEqual(len(l), 2)
        self.assertIsInstance(l, StrictOrderingOnAppendList)

        with self.assertRaises(UnsortedError):
            l += ["b", "a"]

        self.assertEqual(len(l), 2)

    def test_add_after_iadd(self):
        l = StrictOrderingOnAppendList(["b"])
        l += ["a"]
        l2 = l + ["c", "d"]
        self.assertEqual(len(l), 2)
        self.assertEqual(len(l2), 4)
        self.assertIsInstance(l2, StrictOrderingOnAppendList)
        with self.assertRaises(UnsortedError):
            l2 = l + ["d", "c"]

        self.assertEqual(len(l), 2)

    def test_add_StrictOrderingOnAppendList(self):
        l = StrictOrderingOnAppendList()
        l += ["c", "d"]
        l += ["a", "b"]
        l2 = StrictOrderingOnAppendList()
        with self.assertRaises(UnsortedError):
            l2 += list(l)
        # Adding a StrictOrderingOnAppendList to another shouldn't throw
        l2 += l


class TestStrictOrderingOnAppendListWithAction(unittest.TestCase):
    def setUp(self):
        self.action = lambda a: (a, id(a))

    def assertSameList(self, expected, actual):
        self.assertEqual(len(expected), len(actual))
        for idx, item in enumerate(actual):
            self.assertEqual(item, expected[idx])

    def test_init(self):
        l = StrictOrderingOnAppendListWithAction(action=self.action)
        self.assertEqual(len(l), 0)
        original = ["a", "b", "c"]
        l = StrictOrderingOnAppendListWithAction(["a", "b", "c"], action=self.action)
        expected = [self.action(i) for i in original]
        self.assertSameList(expected, l)

        with self.assertRaises(ValueError):
            StrictOrderingOnAppendListWithAction("abc", action=self.action)

        with self.assertRaises(ValueError):
            StrictOrderingOnAppendListWithAction()

    def test_extend(self):
        l = StrictOrderingOnAppendListWithAction(action=self.action)
        original = ["a", "b"]
        l.extend(original)
        expected = [self.action(i) for i in original]
        self.assertSameList(expected, l)

        with self.assertRaises(ValueError):
            l.extend("ab")

    def test_slicing(self):
        l = StrictOrderingOnAppendListWithAction(action=self.action)
        original = ["a", "b"]
        l[:] = original
        expected = [self.action(i) for i in original]
        self.assertSameList(expected, l)

        with self.assertRaises(ValueError):
            l[:] = "ab"

    def test_add(self):
        l = StrictOrderingOnAppendListWithAction(action=self.action)
        original = ["a", "b"]
        l2 = l + original
        expected = [self.action(i) for i in original]
        self.assertSameList(expected, l2)

        with self.assertRaises(ValueError):
            l + "abc"

    def test_iadd(self):
        l = StrictOrderingOnAppendListWithAction(action=self.action)
        original = ["a", "b"]
        l += original
        expected = [self.action(i) for i in original]
        self.assertSameList(expected, l)

        with self.assertRaises(ValueError):
            l += "abc"


class TestStrictOrderingOnAppendListWithFlagsFactory(unittest.TestCase):
    def test_strict_ordering_on_append_list_with_flags_factory(self):
        cls = StrictOrderingOnAppendListWithFlagsFactory(
            {
                "foo": bool,
                "bar": int,
            }
        )

        l = cls()
        l += ["a", "b"]

        with self.assertRaises(Exception):
            l["a"] = "foo"

        with self.assertRaises(Exception):
            l["c"]

        self.assertEqual(l["a"].foo, False)
        l["a"].foo = True
        self.assertEqual(l["a"].foo, True)

        with self.assertRaises(TypeError):
            l["a"].bar = "bar"

        self.assertEqual(l["a"].bar, 0)
        l["a"].bar = 42
        self.assertEqual(l["a"].bar, 42)

        l["b"].foo = True
        self.assertEqual(l["b"].foo, True)

        with self.assertRaises(AttributeError):
            l["b"].baz = False

        l["b"].update(foo=False, bar=12)
        self.assertEqual(l["b"].foo, False)
        self.assertEqual(l["b"].bar, 12)

        with self.assertRaises(AttributeError):
            l["b"].update(xyz=1)

    def test_strict_ordering_on_append_list_with_flags_factory_extend(self):
        FooList = StrictOrderingOnAppendListWithFlagsFactory(
            {"foo": bool, "bar": six.text_type}
        )
        foo = FooList(["a", "b", "c"])
        foo["a"].foo = True
        foo["b"].bar = "bar"

        # Don't allow extending lists with different flag definitions.
        BarList = StrictOrderingOnAppendListWithFlagsFactory(
            {"foo": six.text_type, "baz": bool}
        )
        bar = BarList(["d", "e", "f"])
        bar["d"].foo = "foo"
        bar["e"].baz = True
        with self.assertRaises(ValueError):
            foo + bar
        with self.assertRaises(ValueError):
            bar + foo

        # It's not obvious what to do with duplicate list items with possibly
        # different flag values, so don't allow that case.
        with self.assertRaises(ValueError):
            foo + foo

        def assertExtended(l):
            self.assertEqual(len(l), 6)
            self.assertEqual(l["a"].foo, True)
            self.assertEqual(l["b"].bar, "bar")
            self.assertTrue("c" in l)
            self.assertEqual(l["d"].foo, True)
            self.assertEqual(l["e"].bar, "bar")
            self.assertTrue("f" in l)

        # Test extend.
        zot = FooList(["d", "e", "f"])
        zot["d"].foo = True
        zot["e"].bar = "bar"
        zot.extend(foo)
        assertExtended(zot)

        # Test __add__.
        zot = FooList(["d", "e", "f"])
        zot["d"].foo = True
        zot["e"].bar = "bar"
        assertExtended(foo + zot)
        assertExtended(zot + foo)

        # Test __iadd__.
        foo += zot
        assertExtended(foo)

        # Test __setitem__.
        foo[3:] = []
        self.assertEqual(len(foo), 3)
        foo[3:] = zot
        assertExtended(foo)


class TestMemoize(unittest.TestCase):
    def test_memoize(self):
        self._count = 0

        @memoize
        def wrapped(a, b):
            self._count += 1
            return a + b

        self.assertEqual(self._count, 0)
        self.assertEqual(wrapped(1, 1), 2)
        self.assertEqual(self._count, 1)
        self.assertEqual(wrapped(1, 1), 2)
        self.assertEqual(self._count, 1)
        self.assertEqual(wrapped(2, 1), 3)
        self.assertEqual(self._count, 2)
        self.assertEqual(wrapped(1, 2), 3)
        self.assertEqual(self._count, 3)
        self.assertEqual(wrapped(1, 2), 3)
        self.assertEqual(self._count, 3)
        self.assertEqual(wrapped(1, 1), 2)
        self.assertEqual(self._count, 3)

    def test_memoize_method(self):
        class foo(object):
            def __init__(self):
                self._count = 0

            @memoize
            def wrapped(self, a, b):
                self._count += 1
                return a + b

        instance = foo()
        refcount = sys.getrefcount(instance)
        self.assertEqual(instance._count, 0)
        self.assertEqual(instance.wrapped(1, 1), 2)
        self.assertEqual(instance._count, 1)
        self.assertEqual(instance.wrapped(1, 1), 2)
        self.assertEqual(instance._count, 1)
        self.assertEqual(instance.wrapped(2, 1), 3)
        self.assertEqual(instance._count, 2)
        self.assertEqual(instance.wrapped(1, 2), 3)
        self.assertEqual(instance._count, 3)
        self.assertEqual(instance.wrapped(1, 2), 3)
        self.assertEqual(instance._count, 3)
        self.assertEqual(instance.wrapped(1, 1), 2)
        self.assertEqual(instance._count, 3)

        # Memoization of methods is expected to not keep references to
        # instances, so the refcount shouldn't have changed after executing the
        # memoized method.
        self.assertEqual(refcount, sys.getrefcount(instance))

    def test_memoized_property(self):
        class foo(object):
            def __init__(self):
                self._count = 0

            @memoized_property
            def wrapped(self):
                self._count += 1
                return 42

        instance = foo()
        self.assertEqual(instance._count, 0)
        self.assertEqual(instance.wrapped, 42)
        self.assertEqual(instance._count, 1)
        self.assertEqual(instance.wrapped, 42)
        self.assertEqual(instance._count, 1)


class TestTypedList(unittest.TestCase):
    def test_init(self):
        cls = TypedList(int)
        l = cls()
        self.assertEqual(len(l), 0)

        l = cls([1, 2, 3])
        self.assertEqual(len(l), 3)

        with self.assertRaises(ValueError):
            cls([1, 2, "c"])

    def test_extend(self):
        cls = TypedList(int)
        l = cls()
        l.extend([1, 2])
        self.assertEqual(len(l), 2)
        self.assertIsInstance(l, cls)

        with self.assertRaises(ValueError):
            l.extend([3, "c"])

        self.assertEqual(len(l), 2)

    def test_slicing(self):
        cls = TypedList(int)
        l = cls()
        l[:] = [1, 2]
        self.assertEqual(len(l), 2)
        self.assertIsInstance(l, cls)

        with self.assertRaises(ValueError):
            l[:] = [3, "c"]

        self.assertEqual(len(l), 2)

    def test_add(self):
        cls = TypedList(int)
        l = cls()
        l2 = l + [1, 2]
        self.assertEqual(len(l), 0)
        self.assertEqual(len(l2), 2)
        self.assertIsInstance(l2, cls)

        with self.assertRaises(ValueError):
            l2 = l + [3, "c"]

        self.assertEqual(len(l), 0)

    def test_iadd(self):
        cls = TypedList(int)
        l = cls()
        l += [1, 2]
        self.assertEqual(len(l), 2)
        self.assertIsInstance(l, cls)

        with self.assertRaises(ValueError):
            l += [3, "c"]

        self.assertEqual(len(l), 2)

    def test_add_coercion(self):
        objs = []

        class Foo(object):
            def __init__(self, obj):
                objs.append(obj)

        cls = TypedList(Foo)
        l = cls()
        l += [1, 2]
        self.assertEqual(len(objs), 2)
        self.assertEqual(type(l[0]), Foo)
        self.assertEqual(type(l[1]), Foo)

        # Adding a TypedList to a TypedList shouldn't trigger coercion again
        l2 = cls()
        l2 += l
        self.assertEqual(len(objs), 2)
        self.assertEqual(type(l2[0]), Foo)
        self.assertEqual(type(l2[1]), Foo)

        # Adding a TypedList to a TypedList shouldn't even trigger the code
        # that does coercion at all.
        l2 = cls()
        list.__setitem__(l, slice(0, -1), [1, 2])
        l2 += l
        self.assertEqual(len(objs), 2)
        self.assertEqual(type(l2[0]), int)
        self.assertEqual(type(l2[1]), int)

    def test_memoized(self):
        cls = TypedList(int)
        cls2 = TypedList(str)
        self.assertEqual(TypedList(int), cls)
        self.assertNotEqual(cls, cls2)


class TypedTestStrictOrderingOnAppendList(unittest.TestCase):
    def test_init(self):
        class Unicode(six.text_type):
            def __new__(cls, other):
                if not isinstance(other, six.text_type):
                    raise ValueError()
                return six.text_type.__new__(cls, other)

        cls = TypedList(Unicode, StrictOrderingOnAppendList)
        l = cls()
        self.assertEqual(len(l), 0)

        l = cls(["a", "b", "c"])
        self.assertEqual(len(l), 3)

        with self.assertRaises(UnsortedError):
            cls(["c", "b", "a"])

        with self.assertRaises(ValueError):
            cls(["a", "b", 3])

        self.assertEqual(len(l), 3)


class TestTypedNamedTuple(unittest.TestCase):
    def test_simple(self):
        FooBar = TypedNamedTuple("FooBar", [("foo", six.text_type), ("bar", int)])

        t = FooBar(foo="foo", bar=2)
        self.assertEqual(type(t), FooBar)
        self.assertEqual(t.foo, "foo")
        self.assertEqual(t.bar, 2)
        self.assertEqual(t[0], "foo")
        self.assertEqual(t[1], 2)

        FooBar("foo", 2)

        with self.assertRaises(TypeError):
            FooBar("foo", "not integer")
        with self.assertRaises(TypeError):
            FooBar(2, 4)

        # Passing a tuple as the first argument is the same as passing multiple
        # arguments.
        t1 = ("foo", 3)
        t2 = FooBar(t1)
        self.assertEqual(type(t2), FooBar)
        self.assertEqual(FooBar(t1), FooBar("foo", 3))


class TestGroupUnifiedFiles(unittest.TestCase):
    FILES = ["%s.cpp" % letter for letter in string.ascii_lowercase]

    def test_multiple_files(self):
        mapping = list(group_unified_files(self.FILES, "Unified", "cpp", 5))

        def check_mapping(index, expected_num_source_files):
            (unified_file, source_files) = mapping[index]

            self.assertEqual(unified_file, "Unified%d.cpp" % index)
            self.assertEqual(len(source_files), expected_num_source_files)

        all_files = list(itertools.chain(*[files for (_, files) in mapping]))
        self.assertEqual(len(all_files), len(self.FILES))
        self.assertEqual(set(all_files), set(self.FILES))

        expected_amounts = [5, 5, 5, 5, 5, 1]
        for i, amount in enumerate(expected_amounts):
            check_mapping(i, amount)


class TestMisc(unittest.TestCase):
    def test_pair(self):
        self.assertEqual(list(pair([1, 2, 3, 4, 5, 6])), [(1, 2), (3, 4), (5, 6)])

        self.assertEqual(
            list(pair([1, 2, 3, 4, 5, 6, 7])), [(1, 2), (3, 4), (5, 6), (7, None)]
        )

    def test_expand_variables(self):
        self.assertEqual(expand_variables("$(var)", {"var": "value"}), "value")

        self.assertEqual(
            expand_variables("$(a) and $(b)", {"a": "1", "b": "2"}), "1 and 2"
        )

        self.assertEqual(
            expand_variables("$(a) and $(undefined)", {"a": "1", "b": "2"}), "1 and "
        )

        self.assertEqual(
            expand_variables(
                "before $(string) between $(list) after",
                {"string": "abc", "list": ["a", "b", "c"]},
            ),
            "before abc between a b c after",
        )


class TestEnumString(unittest.TestCase):
    def test_string(self):
        class CompilerType(EnumString):
            POSSIBLE_VALUES = ("gcc", "clang", "clang-cl")

        type = CompilerType("gcc")
        self.assertEqual(type, "gcc")
        self.assertNotEqual(type, "clang")
        self.assertNotEqual(type, "clang-cl")
        self.assertIn(type, ("gcc", "clang-cl"))
        self.assertNotIn(type, ("clang", "clang-cl"))

        with self.assertRaises(EnumStringComparisonError):
            self.assertEqual(type, "foo")

        with self.assertRaises(EnumStringComparisonError):
            self.assertNotEqual(type, "foo")

        with self.assertRaises(EnumStringComparisonError):
            self.assertIn(type, ("foo", "gcc"))

        with self.assertRaises(ValueError):
            type = CompilerType("foo")


class TestHexDump(unittest.TestCase):
    @unittest.skipUnless(six.PY3, "requires Python 3")
    def test_hexdump(self):
        self.assertEqual(
            hexdump("abcdef123💩ZYXWVU".encode("utf-8")),
            [
                "00  61 62 63 64 65 66 31 32  33 f0 9f 92 a9 5a 59 58  |abcdef123....ZYX|\n",
                "10  57 56 55                                          |WVU             |\n",
            ],
        )


def test_read_only_dict():
    d = ReadOnlyDict(foo="bar")
    with pytest.raises(Exception):
        d["foo"] = "baz"

    with pytest.raises(Exception):
        d.update({"foo": "baz"})

    with pytest.raises(Exception):
        del d["foo"]

    # ensure copy still works
    d_copy = d.copy()
    assert d == d_copy
    # TODO Returning a dict here feels like a bug, but there are places in-tree
    # relying on this behaviour.
    assert isinstance(d_copy, dict)

    d_copy = copy.copy(d)
    assert d == d_copy
    assert isinstance(d_copy, ReadOnlyDict)

    d_copy = copy.deepcopy(d)
    assert d == d_copy
    assert isinstance(d_copy, ReadOnlyDict)


if __name__ == "__main__":
    main()
