# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest

from tryselect.tasks import cache_key, filter_tasks_by_paths, resolve_tests_by_suite


def test_filter_tasks_by_paths(patch_resolver):
    tasks = ["foobar/xpcshell-1", "foobar/mochitest", "foobar/xpcshell"]

    patch_resolver(["xpcshell"], {})
    assert list(filter_tasks_by_paths(tasks, "dummy")) == []

    patch_resolver([], [{"flavor": "xpcshell"}])
    assert list(filter_tasks_by_paths(tasks, "dummy")) == [
        "foobar/xpcshell-1",
        "foobar/xpcshell",
    ]


def test_resolve_tests_by_suite(patch_resolver):
    patch_resolver([], [{"flavor": "xpcshell", "srcdir_relpath": "xpcshell.js"}])
    assert resolve_tests_by_suite(["xpcshell.js"]) == {
        "xpcshell": ["xpcshell.js"],
    }

    patch_resolver(
        [],
        [
            {
                "flavor": "xpcshell",
                "srcdir_relpath": "xpcshell.js",
                "manifest_relpath": "xpcshell.ini",
            },
        ],
    )
    assert resolve_tests_by_suite(["xpcshell.ini"]) == {
        "xpcshell": ["xpcshell.ini"],
    }

    patch_resolver(
        [],
        [
            {"flavor": "xpcshell", "srcdir_relpath": "xpcshell.js"},
            {"flavor": "mochitest", "srcdir_relpath": "mochitest.js"},
        ],
    )
    assert resolve_tests_by_suite(["xpcshell.js", "mochitest.js"]) == {
        "xpcshell": ["xpcshell.js"],
        "mochitest-plain": ["mochitest.js"],
    }


@pytest.mark.parametrize(
    "attr,params,disable_target_task_filter,expected",
    (
        ("target_task_set", None, False, "target_task_set"),
        ("target_task_set", {"project": "autoland"}, False, "target_task_set"),
        ("target_task_set", {"project": "mozilla-central"}, False, "target_task_set"),
        ("target_task_set", None, True, "target_task_set-uncommon"),
        ("full_task_set", {"project": "pine"}, False, "full_task_set-pine"),
        ("full_task_set", None, True, "full_task_set"),
    ),
)
def test_cache_key(attr, params, disable_target_task_filter, expected):
    assert cache_key(attr, params, disable_target_task_filter) == expected


if __name__ == "__main__":
    mozunit.main()
