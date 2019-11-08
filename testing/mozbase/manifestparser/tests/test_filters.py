#!/usr/bin/env python
# flake8: noqa

from __future__ import absolute_import

from copy import deepcopy
import os

import mozunit
import pytest

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


def test_data_model():
    foo = lambda x, y: x
    bar = lambda x, y: x
    baz = lambda x, y: x
    fl = filterlist()

    fl.extend([foo, bar])
    assert len(fl) == 2
    assert foo in fl

    fl.append(baz)
    assert fl[2] == baz

    fl.remove(baz)
    assert baz not in fl

    item = fl.pop()
    assert item == bar

    assert fl.index(foo) == 0

    del fl[0]
    assert foo not in fl
    with pytest.raises(IndexError):
        fl[0]


def test_add_non_callable_to_list():
    fl = filterlist()
    with pytest.raises(TypeError):
        fl.append('foo')


def test_add_duplicates_to_list():
    foo = lambda x, y: x
    bar = lambda x, y: x
    sub = subsuite('foo')
    fl = filterlist([foo, bar, sub])
    assert len(fl) == 3
    assert fl[0] == foo

    with pytest.raises(ValueError):
        fl.append(foo)

    with pytest.raises(ValueError):
        fl.append(subsuite('bar'))


def test_add_two_tags_filters():
    tag1 = tags('foo')
    tag2 = tags('bar')
    fl = filterlist([tag1])

    with pytest.raises(ValueError):
        fl.append(tag1)

    fl.append(tag2)
    assert len(fl) == 2


def test_filters_run_in_order():
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
    assert [i for i in fl] == [a, b, c, d, e, f]


@pytest.fixture
def tests():
    return (
        {"name": "test0"},
        {"name": "test1", "skip-if": "foo == 'bar'"},
        {"name": "test2", "run-if": "foo == 'bar'"},
        {"name": "test3", "fail-if": "foo == 'bar'"},
        {"name": "test4", "disabled": "some reason"},
        {"name": "test5", "subsuite": "baz"},
        {"name": "test6", "subsuite": "baz,foo == 'bar'"},
        {"name": "test7", "tags": "foo bar"},
    )


def test_skip_if(tests):
    ref = deepcopy(tests)
    tests = list(skip_if(tests, {}))
    assert len(tests) == len(ref)

    tests = deepcopy(ref)
    tests = list(skip_if(tests, {'foo': 'bar'}))
    assert tests[1] not in ref


def test_run_if(tests):
    ref = deepcopy(tests)
    tests = list(run_if(tests, {}))
    assert ref[2] not in tests

    tests = deepcopy(ref)
    tests = list(run_if(tests, {'foo': 'bar'}))
    assert len(tests) == len(ref)


def test_fail_if(tests):
    ref = deepcopy(tests)
    tests = list(fail_if(tests, {}))
    assert 'expected' not in tests[3]

    tests = deepcopy(ref)
    tests = list(fail_if(tests, {'foo': 'bar'}))
    assert tests[3]['expected'] == 'fail'


def test_enabled(tests):
    ref = deepcopy(tests)
    tests = list(enabled(tests, {}))
    assert ref[4] not in tests


def test_subsuite(tests):
    sub1 = subsuite()
    sub2 = subsuite('baz')

    ref = deepcopy(tests)
    tests = list(sub1(tests, {}))
    assert ref[5] not in tests
    assert len(tests) == len(ref) - 1

    tests = deepcopy(ref)
    tests = list(sub2(tests, {}))
    assert len(tests) == 1
    assert ref[5] in tests


def test_subsuite_condition(tests):
    sub1 = subsuite()
    sub2 = subsuite('baz')

    ref = deepcopy(tests)

    tests = list(sub1(tests, {'foo': 'bar'}))
    assert ref[5] not in tests
    assert ref[6] not in tests

    tests = deepcopy(ref)
    tests = list(sub2(tests, {'foo': 'bar'}))
    assert len(tests) == 2
    assert tests[0]['name'] == 'test5'
    assert tests[1]['name'] == 'test6'


def test_tags(tests):
    ftags1 = tags([])
    ftags2 = tags(['bar', 'baz'])

    ref = deepcopy(tests)
    tests = list(ftags1(tests, {}))
    assert len(tests) == 0

    tests = deepcopy(ref)
    tests = list(ftags2(tests, {}))
    assert len(tests) == 1
    assert ref[7] in tests


if __name__ == '__main__':
    mozunit.main()
