# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function, unicode_literals

import pytest
from mozunit import main
from tryselect.selectors.auto import TRY_AUTO_PARAMETERS

from taskgraph.util.bugbug import push_schedules
from taskgraph.util.chunking import BugbugLoader


pytestmark = pytest.mark.slow
PARAMS = TRY_AUTO_PARAMETERS.copy()
PARAMS.update(
    {
        "head_repository": "https://hg.mozilla.org/try",
        "project": "try",
        "target_kind": "test",
        # These ensure this isn't considered a backstop. The pushdate must
        # be slightly higher than the one in data/pushes.json, and
        # pushlog_id must not be a multiple of 10.
        "pushdate": 1593029536,
        "pushlog_id": "2",
    }
)


def test_generate_graph(optimized_task_graph):
    """Simply tests that generating the graph does not fail."""
    assert len(optimized_task_graph.tasks) > 0


def test_only_important_manifests(params, full_task_graph, filter_tasks):
    data = push_schedules(params["project"], params["head_rev"])
    important_manifests = {
        m
        for m, c in data.get("groups", {}).items()
        if c >= BugbugLoader.CONFIDENCE_THRESHOLD
    }

    # Ensure we don't schedule unimportant manifests.
    for task in filter_tasks(full_task_graph, lambda t: t.kind == "test"):
        attr = task.attributes.get

        if "test_manifests" in task.attributes:
            unimportant = [
                t for t in attr("test_manifests") if t not in important_manifests
            ]

            # Manifest scheduling is disabled for mochitest-ally.
            if attr("unittest_suite") == "mochitest-a11y":
                assert len(unimportant) > 0
            else:
                assert unimportant == []


@pytest.mark.parametrize(
    "func,min_expected",
    (
        pytest.param(
            lambda t: (
                t.kind == "test"
                and t.attributes["unittest_suite"] == "mochitest-browser-chrome"
            ),
            5,
            id="mochitest-browser-chrome",
        ),
    ),
)
def test_tasks_are_scheduled(optimized_task_graph, filter_tasks, func, min_expected):
    """Ensure the specified tasks are scheduled on mozilla-central."""
    tasks = [t.label for t in filter_tasks(optimized_task_graph, func)]
    assert len(tasks) >= min_expected


@pytest.mark.parametrize(
    "func",
    (
        pytest.param(
            lambda t: t.kind == "build"
            and "shippable" in t.attributes["build_platform"],
            id="no shippable builds",
        ),
        pytest.param(
            lambda t: t.kind == "build" and "fuzzing" in t.attributes["build_platform"],
            id="no fuzzing builds",
        ),
        pytest.param(
            lambda t: t.kind == "build" and "ccov" in t.attributes["build_platform"],
            id="no ccov builds",
        ),
        # We should only assert that we have no signed builds on platforms that don't run
        # xpcshell tests.
        pytest.param(
            lambda t: t.kind == "build-signing",
            id="no build-signing",
            marks=pytest.mark.xfail(reason="some xpcshell tests require signed builds"),
        ),
        pytest.param(
            lambda t: t.kind == "upload-symbols",
            id="no upload-symbols",
        ),
    ),
)
def test_tasks_are_not_scheduled(
    optimized_task_graph, filter_tasks, print_dependents, func
):
    # We can still sometimes get macosx64-shippable builds with |mach try
    # auto| due to TV tasks (since there is no 'opt' alternative for
    # macosx). Otherwise there shouldn't be any other shippable builds.
    tasks = [t.label for t in filter_tasks(optimized_task_graph, func)]
    for t in tasks:
        print_dependents(optimized_task_graph, t)
    assert tasks == []


if __name__ == "__main__":
    main()
