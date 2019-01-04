# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import pytest
from mock import MagicMock
from moztest.resolve import TestResolver

from tryselect import push


@pytest.fixture
def patch_resolver(monkeypatch):
    def inner(suites, tests):
        def fake_test_metadata(*args, **kwargs):
            return suites, tests
        monkeypatch.setattr(TestResolver, 'resolve_metadata', fake_test_metadata)
    return inner


@pytest.fixture(autouse=True)
def patch_vcs(monkeypatch):
    attrs = {
        'path': push.vcs.path,
    }
    mock = MagicMock()
    mock.configure_mock(**attrs)
    monkeypatch.setattr(push, 'vcs', mock)


def pytest_generate_tests(metafunc):
    if all(fixture in metafunc.fixturenames for fixture in ('template', 'args', 'expected')):
        def load_tests():
            for template, tests in metafunc.module.TEMPLATE_TESTS.items():
                for args, expected in tests:
                    yield (template, args, expected)

        tests = list(load_tests())
        ids = ['{} {}'.format(t[0], ' '.join(t[1])).strip() for t in tests]
        metafunc.parametrize('template,args,expected', tests, ids=ids)
