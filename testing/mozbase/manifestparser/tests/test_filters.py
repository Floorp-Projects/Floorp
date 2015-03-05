#!/usr/bin/env python

from copy import deepcopy
import os
import unittest

from manifestparser import TestManifest
from manifestparser.filters import (
    subsuite,
    skip_if,
    fail_if,
    enabled,
    exists,
    filterlist,
)

here = os.path.dirname(os.path.abspath(__file__))


class FilterList(unittest.TestCase):
    """Test filterlist datatype"""

    def test_data_model(self):
        foo = lambda x, y: x
        bar = lambda x, y: x
        baz = lambda x, y: x
        fl = filterlist()

        fl.extend([foo, bar])
        self.assertEquals(len(fl), 2)
        self.assertTrue(foo in fl)

        fl.append(baz)
        self.assertEquals(fl[2], baz)

        fl.remove(baz)
        self.assertFalse(baz in fl)

        item = fl.pop()
        self.assertEquals(item, bar)

        self.assertEquals(fl.index(foo), 0)

        del fl[0]
        self.assertFalse(foo in fl)
        with self.assertRaises(IndexError):
            fl[0]

    def test_add_non_callable_to_set(self):
        fl = filterlist()
        with self.assertRaises(TypeError):
            fl.append('foo')

    def test_add_duplicates_to_set(self):
        foo = lambda x, y: x
        bar = lambda x, y: x
        sub = subsuite('foo')
        fl = filterlist([foo, bar, sub])
        self.assertEquals(len(fl), 3)
        self.assertEquals(fl[0], foo)

        with self.assertRaises(ValueError):
            fl.append(foo)

        with self.assertRaises(ValueError):
            fl.append(subsuite('bar'))

    def test_filters_run_in_order(self):
        a = lambda x, y: x
        b = lambda x, y: x
        c = lambda x, y: x
        d = lambda x, y: x
        e = lambda x, y: x
        f = lambda x, y: x

        fl = filterlist([a, b])
        fl.append(c)
        fl.extend([d, e])
        fl += [f]
        self.assertEquals([i for i in fl], [a, b, c, d, e, f])


class BuiltinFilters(unittest.TestCase):
    """Test the built-in filters"""

    tests = (
        { "name": "test0" },
        { "name": "test1", "skip-if": "foo == 'bar'" },
        { "name": "test2", "fail-if": "foo == 'bar'" },
        { "name": "test3", "disabled": "some reason" },
        { "name": "test4", "subsuite": "baz" },
        { "name": "test5", "subsuite": "baz,foo == 'bar'" })

    def test_skip_if(self):
        tests = deepcopy(self.tests)
        tests = list(skip_if(tests, {}))
        self.assertEquals(len(tests), len(self.tests))

        tests = deepcopy(self.tests)
        tests = list(skip_if(tests, {'foo': 'bar'}))
        self.assertNotIn(self.tests[1], tests)

    def test_fail_if(self):
        tests = deepcopy(self.tests)
        tests = list(fail_if(tests, {}))
        self.assertNotIn('expected', tests[2])

        tests = deepcopy(self.tests)
        tests = list(fail_if(tests, {'foo': 'bar'}))
        self.assertEquals(tests[2]['expected'], 'fail')

    def test_enabled(self):
        tests = deepcopy(self.tests)
        tests = list(enabled(tests, {}))
        self.assertNotIn(self.tests[3], tests)

    def test_subsuite(self):
        sub1 = subsuite()
        sub2 = subsuite('baz')

        tests = deepcopy(self.tests)
        tests = list(sub1(tests, {}))
        self.assertNotIn(self.tests[4], tests)
        self.assertEquals(tests[-1]['name'], 'test6')

        tests = deepcopy(self.tests)
        tests = list(sub2(tests, {}))
        self.assertEquals(len(tests), 1)
        self.assertIn(self.tests[4], tests)

    def test_subsuite_condition(self):
        sub1 = subsuite()
        sub2 = subsuite('baz')

        tests = deepcopy(self.tests)

        tests = list(sub1(tests, {'foo': 'bar'}))
        self.assertNotIn(self.tests[4], tests)
        self.assertNotIn(self.tests[5], tests)

        tests = deepcopy(self.tests)
        tests = list(sub2(tests, {'foo': 'bar'}))
        self.assertEquals(len(tests), 2)
        self.assertEquals(tests[0]['name'], 'test4')
        self.assertEquals(tests[1]['name'], 'test5')
