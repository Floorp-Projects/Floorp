# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest


TASKS = [
    {
        'kind': 'build',
        'label': 'build-windows',
        'attributes': {
            'build_platform': 'windows',
        },
    },
    {
        'kind': 'test',
        'label': 'test-windows-mochitest-e10s',
        'attributes': {
            'unittest_suite': 'mochitest',
            'unittest_flavor': 'browser-chrome',
            'mochitest_try_name': 'mochitest',
        },
    },
]


@pytest.fixture(autouse=True)
def skip_taskgraph_generation(monkeypatch, tg):

    def fake_generate_tasks(*args, **kwargs):
        return tg

    from tryselect import tasks
    monkeypatch.setattr(tasks, 'generate_tasks', fake_generate_tasks)


@pytest.mark.xfail(strict=False, reason="Bug 1635204: "
                                        "test_shared_presets[sample-suites] is flaky")
def test_shared_presets(run_mach, shared_name, shared_preset):
    """This test makes sure that we don't break any of the in-tree presets when
    renaming/removing variables in any of the selectors.
    """
    assert 'description' in shared_preset
    assert 'selector' in shared_preset

    selector = shared_preset['selector']
    if selector == 'fuzzy':
        assert 'query' in shared_preset
        assert isinstance(shared_preset['query'], list)

    # Run the preset and assert there were no exceptions.
    assert run_mach(['try', '--no-push', '--preset', shared_name]) == 0


if __name__ == '__main__':
    mozunit.main()
