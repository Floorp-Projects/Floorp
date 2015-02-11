# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
A filter is a callable that accepts an iterable of test objects and a
dictionary of values, and returns a new iterable of test objects. It is
possible to define custom filters if the built-in ones are not enough.
"""

from collections import MutableSequence
import os

from .expression import (
    parse,
    ParseError,
)


# built-in filters

def skip_if(tests, values):
    """
    Sets disabled on all tests containing the `skip-if` tag and whose condition
    is True. This filter is added by default.
    """
    tag = 'skip-if'
    for test in tests:
        if tag in test and parse(test[tag], **values):
            test.setdefault('disabled', '{}: {}'.format(tag, test[tag]))
        yield test


def run_if(tests, values):
    """
    Sets disabled on all tests containing the `run-if` tag and whose condition
    is False. This filter is added by default.
    """
    tag = 'run-if'
    for test in tests:
        if tag in test and not parse(test[tag], **values):
            test.setdefault('disabled', '{}: {}'.format(tag, test[tag]))
        yield test


def fail_if(tests, values):
    """
    Sets expected to 'fail' on all tests containing the `fail-if` tag and whose
    condition is True. This filter is added by default.
    """
    tag = 'fail-if'
    for test in tests:
        if tag in test and parse(test[tag], **values):
            test['expected'] = 'fail'
        yield test


def enabled(tests, values):
    """
    Removes all tests containing the `disabled` key. This filter can be
    added by passing `disabled=False` into `active_tests`.
    """
    for test in tests:
        if 'disabled' not in test:
            yield test


def exists(tests, values):
    """
    Removes all tests that do not exist on the file system. This filter is
    added by default, but can be removed by passing `exists=False` into
    `active_tests`.
    """
    for test in tests:
        if os.path.exists(test['path']):
            yield test


# built-in instance filters

class InstanceFilter(object):
    """
    Generally only one instance of a class filter should be applied at a time.
    Two instances of `InstanceFilter` are considered equal if they have the
    same class name. This ensures only a single instance is ever added to
    `filterlist`.
    """
    def __eq__(self, other):
        return self.__class__ == other.__class__


class subsuite(InstanceFilter):
    """
    If `name` is None, removes all tests that have a `subsuite` key.
    Otherwise removes all tests that do not have a subsuite matching `name`.

    It is possible to specify conditional subsuite keys using:
       subsuite = foo,condition

    where 'foo' is the subsuite name, and 'condition' is the same type of
    condition used for skip-if.  If the condition doesn't evaluate to true,
    the subsuite designation will be removed from the test.

    :param name: The name of the subsuite to run (default None)
    """
    def __init__(self, name=None):
        self.name = name

    def __call__(self, tests, values):
        # Look for conditional subsuites, and replace them with the subsuite
        # itself (if the condition is true), or nothing.
        for test in tests:
            subsuite = test.get('subsuite', '')
            if ',' in subsuite:
                try:
                    subsuite, cond = subsuite.split(',')
                except ValueError:
                    raise ParseError("subsuite condition can't contain commas")
                matched = parse(cond, **values)
                if matched:
                    test['subsuite'] = subsuite
                else:
                    test['subsuite'] = ''

            # Filter on current subsuite
            if self.name is None:
                if not test.get('subsuite'):
                    yield test
            else:
                if test.get('subsuite') == self.name:
                    yield test


# filter container

DEFAULT_FILTERS = (
    skip_if,
    run_if,
    fail_if,
)
"""
By default :func:`~.active_tests` will run the :func:`~.skip_if`,
:func:`~.run_if` and :func:`~.fail_if` filters.
"""


class filterlist(MutableSequence):
    """
    A MutableSequence that raises TypeError when adding a non-callable and
    ValueError if the item is already added.
    """

    def __init__(self, items=None):
        self.items = []
        if items:
            self.items = list(items)

    def _validate(self, item):
        if not callable(item):
            raise TypeError("Filters must be callable!")
        if item in self:
            raise ValueError("Filter {} is already applied!".format(item))

    def __getitem__(self, key):
        return self.items[key]

    def __setitem__(self, key, value):
        self._validate(value)
        self.items[key] = value

    def __delitem__(self, key):
        del self.items[key]

    def __len__(self):
        return len(self.items)

    def insert(self, index, value):
        self._validate(value)
        self.items.insert(index, value)
