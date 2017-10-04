#!/usr/bin/env python
# flake8: noqa

from copy import deepcopy
import os
import unittest

import mozunit

from manifestparser.filters import (
    subsuite,
    tags,
    skip_if,
    run_if,
    fail_if,
    enabled,
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

    def test_add_non_callable_to_list(self):
        fl = filterlist()
        with self.assertRaises(TypeError):
            fl.append('foo')

    def test_add_duplicates_to_list(self):
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

    def test_add_two_tags_filters(self):
        tag1 = tags('foo')
        tag2 = tags('bar')
        fl = filterlist([tag1])

        with self.assertRaises(ValueError):
            fl.append(tag1)

        fl.append(tag2)
        self.assertEquals(len(fl), 2)

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
        {"name": "test0"},
        {"name": "test1", "skip-if": "foo == 'bar'"},
        {"name": "test2", "run-if": "foo == 'bar'"},
        {"name": "test3", "fail-if": "foo == 'bar'"},
        {"name": "test4", "disabled": "some reason"},
        {"name": "test5", "subsuite": "baz"},
        {"name": "test6", "subsuite": "baz,foo == 'bar'"},
        {"name": "test7", "tags": "foo bar"},
    )

    def test_skip_if(self):
        tests = deepcopy(self.tests)
        tests = list(skip_if(tests, {}))
        self.assertEquals(len(tests), len(self.tests))

        tests = deepcopy(self.tests)
        tests = list(skip_if(tests, {'foo': 'bar'}))
        self.assertNotIn(self.tests[1], tests)

    def test_run_if(self):
        tests = deepcopy(self.tests)
        tests = list(run_if(tests, {}))
        self.assertNotIn(self.tests[2], tests)

        tests = deepcopy(self.tests)
        tests = list(run_if(tests, {'foo': 'bar'}))
        self.assertEquals(len(tests), len(self.tests))

    def test_fail_if(self):
        tests = deepcopy(self.tests)
        tests = list(fail_if(tests, {}))
        self.assertNotIn('expected', tests[3])

        tests = deepcopy(self.tests)
        tests = list(fail_if(tests, {'foo': 'bar'}))
        self.assertEquals(tests[3]['expected'], 'fail')

    def test_enabled(self):
        tests = deepcopy(self.tests)
        tests = list(enabled(tests, {}))
        self.assertNotIn(self.tests[4], tests)

    def test_subsuite(self):
        sub1 = subsuite()
        sub2 = subsuite('baz')

        tests = deepcopy(self.tests)
        tests = list(sub1(tests, {}))
        self.assertNotIn(self.tests[5], tests)
        self.assertEquals(len(tests), len(self.tests) - 1)

        tests = deepcopy(self.tests)
        tests = list(sub2(tests, {}))
        self.assertEquals(len(tests), 1)
        self.assertIn(self.tests[5], tests)

    def test_subsuite_condition(self):
        sub1 = subsuite()
        sub2 = subsuite('baz')

        tests = deepcopy(self.tests)

        tests = list(sub1(tests, {'foo': 'bar'}))
        self.assertNotIn(self.tests[5], tests)
        self.assertNotIn(self.tests[6], tests)

        tests = deepcopy(self.tests)
        tests = list(sub2(tests, {'foo': 'bar'}))
        self.assertEquals(len(tests), 2)
        self.assertEquals(tests[0]['name'], 'test5')
        self.assertEquals(tests[1]['name'], 'test6')

    def test_tags(self):
        ftags1 = tags([])
        ftags2 = tags(['bar', 'baz'])

        tests = deepcopy(self.tests)
        tests = list(ftags1(tests, {}))
        self.assertEquals(len(tests), 0)

        tests = deepcopy(self.tests)
        tests = list(ftags2(tests, {}))
        self.assertEquals(len(tests), 1)
        self.assertIn(self.tests[7], tests)


if __name__ == '__main__':
    mozunit.main()
