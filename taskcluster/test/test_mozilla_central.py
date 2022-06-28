# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function, unicode_literals

import pytest
from mozunit import main

pytestmark = pytest.mark.slow
PARAMS = {
    "head_repository": "https://hg.mozilla.org/mozilla-central",
    "project": "central",
}


def test_generate_graph(optimized_task_graph):
    """Simply tests that generating the graph does not fail."""
    assert len(optimized_task_graph.tasks) > 0


@pytest.mark.parametrize(
    "func,min_expected",
    (
        pytest.param(
            lambda t: t.kind == "build" and "fuzzing" in t.attributes["build_platform"],
            5,
            id="fuzzing builds",
        ),
    ),
)
def test_tasks_are_scheduled(optimized_task_graph, filter_tasks, func, min_expected):
    """Ensure the specified tasks are scheduled on mozilla-central."""
    tasks = [t.label for t in filter_tasks(optimized_task_graph, func)]
    print(tasks)
    assert len(tasks) >= min_expected


def test_test_setting(full_task_graph, filter_tasks):
    """Verify that all test tasks' ``test-setting`` object conforms to the schema."""
    from gecko_taskgraph.transforms.test.other import test_setting_description_schema
    from taskgraph.util.schema import validate_schema

    tasks = filter_tasks(full_task_graph, lambda t: t.kind == "test")

    failures = []
    for task in tasks:
        try:
            validate_schema(
                test_setting_description_schema,
                dict(task.task["extra"]["test-setting"]),
                task.label,
            )
        except Exception as e:
            failures.append(e)

    if failures:
        more = None
        # Only display a few failures at once.
        if len(failures) > 10:
            more = len(failures) - 10
            failures = failures[:10]

        failstr = "\n\n" + "\n\n".join(str(e) for e in failures)
        if more:
            failstr += "\n\n" + f"... and {more} more"

        pytest.fail(f"Task validation errors:{failstr}")


if __name__ == "__main__":
    main()
