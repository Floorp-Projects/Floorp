# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import pytest
from mozunit import main

pytestmark = pytest.mark.slow
PARAMS = {
    "backstop": True,
    "head_repository": "https://hg.mozilla.org/integration/autoland",
    "project": "autoland",
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
    assert len(tasks) >= min_expected


@pytest.mark.parametrize(
    "func",
    (
        pytest.param(
            lambda t: t.kind == "build" and "ccov" in t.attributes["build_platform"],
            id="no ccov builds",
        ),
    ),
)
def test_tasks_are_not_scheduled(
    optimized_task_graph, filter_tasks, print_dependents, func
):
    """Ensure the specified tasks are not scheduled on autoland."""
    tasks = [t.label for t in filter_tasks(optimized_task_graph, func)]
    for t in tasks:
        print_dependents(optimized_task_graph, t)
    assert tasks == []


if __name__ == "__main__":
    main()
