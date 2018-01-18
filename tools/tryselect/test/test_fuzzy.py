# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest
from moztest.resolve import TestResolver

from tryselect.selectors import fuzzy


@pytest.fixture
def patch_resolver(monkeypatch):
    def inner(suites, tests):
        def fake_test_metadata(*args, **kwargs):
            return suites, tests
        monkeypatch.setattr(TestResolver, 'resolve_metadata', fake_test_metadata)
    return inner


def test_filter_by_paths(patch_resolver):
    tasks = ['foobar/xpcshell-1', 'foobar/mochitest', 'foobar/xpcshell']

    patch_resolver(['xpcshell'], {})
    assert fuzzy.filter_by_paths(tasks, 'dummy') == []

    patch_resolver([], [{'flavor': 'xpcshell'}])
    assert fuzzy.filter_by_paths(tasks, 'dummy') == ['foobar/xpcshell-1', 'foobar/xpcshell']


if __name__ == '__main__':
    mozunit.main()
