# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from datetime import datetime, timedelta
from functools import partial

import pytest
from taskgraph import graph, optimize
from taskgraph.optimize import OptimizationStrategy, All, Any, Not
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task
from mozunit import main


class Remove(OptimizationStrategy):
    def should_remove_task(self, task, params, arg):
        return True


class Replace(OptimizationStrategy):
    def should_replace_task(self, task, params, deadline, taskid):
        expires = datetime.utcnow() + timedelta(days=1)
        if deadline and expires < datetime.strptime(deadline, "%Y-%m-%dT%H:%M:%S.%fZ"):
            return False
        return taskid


def default_strategies():
    return {
        "never": OptimizationStrategy(),
        "remove": Remove(),
        "replace": Replace(),
    }


def make_task(
    label,
    optimization=None,
    task_def=None,
    optimized=None,
    task_id=None,
    dependencies=None,
    if_dependencies=None,
):
    task_def = task_def or {
        "sample": "task-def",
        "deadline": {"relative-datestamp": "1 hour"},
    }
    task = Task(
        kind="test",
        label=label,
        attributes={},
        task=task_def,
        if_dependencies=if_dependencies or [],
    )
    task.optimization = optimization
    task.task_id = task_id
    if dependencies is not None:
        task.task["dependencies"] = sorted(dependencies)
    return task


def make_graph(*tasks_and_edges, **kwargs):
    tasks = {t.label: t for t in tasks_and_edges if isinstance(t, Task)}
    edges = {e for e in tasks_and_edges if not isinstance(e, Task)}
    tg = TaskGraph(tasks, graph.Graph(set(tasks), edges))

    if kwargs.get("deps", True):
        # set dependencies based on edges
        for l, r, name in tg.graph.edges:
            tg.tasks[l].dependencies[name] = r

    return tg


def make_opt_graph(*tasks_and_edges):
    tasks = {t.task_id: t for t in tasks_and_edges if isinstance(t, Task)}
    edges = {e for e in tasks_and_edges if not isinstance(e, Task)}
    return TaskGraph(tasks, graph.Graph(set(tasks), edges))


def make_triangle(deps=True, **opts):
    """
    Make a "triangle" graph like this:

      t1 <-------- t3
       `---- t2 --'
    """
    return make_graph(
        make_task("t1", opts.get("t1")),
        make_task("t2", opts.get("t2")),
        make_task("t3", opts.get("t3")),
        ("t3", "t2", "dep"),
        ("t3", "t1", "dep2"),
        ("t2", "t1", "dep"),
        deps=deps,
    )


@pytest.mark.parametrize(
    "graph,kwargs,exp_removed",
    (
        # A graph full of optimization=never has nothing removed
        pytest.param(
            make_triangle(),
            {},
            # expectations
            set(),
            id="never",
        ),
        # A graph full of optimization=remove removes everything
        pytest.param(
            make_triangle(
                t1={"remove": None},
                t2={"remove": None},
                t3={"remove": None},
            ),
            {},
            # expectations
            {"t1", "t2", "t3"},
            id="all",
        ),
        # Tasks with the 'any' composite strategy are removed when any substrategy says to
        pytest.param(
            make_triangle(
                t1={"any": None},
                t2={"any": None},
                t3={"any": None},
            ),
            {"strategies": lambda: {"any": Any("never", "remove")}},
            # expectations
            {"t1", "t2", "t3"},
            id="composite_strategies_any",
        ),
        # Tasks with the 'all' composite strategy are removed when all substrategies say to
        pytest.param(
            make_triangle(
                t1={"all": None},
                t2={"all": None},
                t3={"all": None},
            ),
            {"strategies": lambda: {"all": All("never", "remove")}},
            # expectations
            set(),
            id="composite_strategies_all",
        ),
        # Tasks with the 'not' composite strategy are removed when the substrategy says not to
        pytest.param(
            make_graph(
                make_task("t1", {"not-never": None}),
                make_task("t2", {"not-remove": None}),
            ),
            {
                "strategies": lambda: {
                    "not-never": Not("never"),
                    "not-remove": Not("remove"),
                }
            },
            # expectations
            {"t1"},
            id="composite_strategies_not",
        ),
        # Removable tasks that are depended on by non-removable tasks are not removed
        pytest.param(
            make_triangle(
                t1={"remove": None},
                t3={"remove": None},
            ),
            {},
            # expectations
            {"t3"},
            id="blocked",
        ),
        # Removable tasks that are marked do_not_optimize are not removed
        pytest.param(
            make_triangle(
                t1={"remove": None},
                t2={"remove": None},  # but do_not_optimize
                t3={"remove": None},
            ),
            {"do_not_optimize": {"t2"}},
            # expectations
            {"t3"},
            id="do_not_optimize",
        ),
        # Tasks with 'if_dependencies' are removed when deps are not run
        pytest.param(
            make_graph(
                make_task("t1", {"remove": None}),
                make_task("t2", {"remove": None}),
                make_task("t3", {"never": None}, if_dependencies=["t1", "t2"]),
                make_task("t4", {"never": None}, if_dependencies=["t1"]),
                ("t3", "t2", "dep"),
                ("t3", "t1", "dep2"),
                ("t2", "t1", "dep"),
                ("t4", "t1", "dep3"),
            ),
            {"requested_tasks": {"t3", "t4"}},
            # expectations
            {"t1", "t2", "t3", "t4"},
            id="if_deps_removed",
        ),
        # Parents of tasks with 'if_dependencies' are also removed even if requested
        pytest.param(
            make_graph(
                make_task("t1", {"remove": None}),
                make_task("t2", {"remove": None}),
                make_task("t3", {"never": None}, if_dependencies=["t1", "t2"]),
                make_task("t4", {"never": None}, if_dependencies=["t1"]),
                ("t3", "t2", "dep"),
                ("t3", "t1", "dep2"),
                ("t2", "t1", "dep"),
                ("t4", "t1", "dep3"),
            ),
            {},
            # expectations
            {"t1", "t2", "t3", "t4"},
            id="if_deps_parents_removed",
        ),
        # Tasks with 'if_dependencies' are kept if at least one of said dependencies are kept
        pytest.param(
            make_graph(
                make_task("t1", {"never": None}),
                make_task("t2", {"remove": None}),
                make_task("t3", {"never": None}, if_dependencies=["t1", "t2"]),
                make_task("t4", {"never": None}, if_dependencies=["t1"]),
                ("t3", "t2", "dep"),
                ("t3", "t1", "dep2"),
                ("t2", "t1", "dep"),
                ("t4", "t1", "dep3"),
            ),
            {},
            # expectations
            set(),
            id="if_deps_kept",
        ),
        # Ancestor of task with 'if_dependencies' does not cause it to be kept
        pytest.param(
            make_graph(
                make_task("t1", {"never": None}),
                make_task("t2", {"remove": None}),
                make_task("t3", {"never": None}, if_dependencies=["t2"]),
                ("t3", "t2", "dep"),
                ("t2", "t1", "dep2"),
            ),
            {},
            # expectations
            {"t2", "t3"},
            id="if_deps_ancestor_does_not_keep",
        ),
        # Unhandled edge case where 't1' and 't2' are kept even though they
        # don't have any dependents and are not in 'requested_tasks'
        pytest.param(
            make_graph(
                make_task("t1", {"never": None}),
                make_task("t2", {"never": None}, if_dependencies=["t1"]),
                make_task("t3", {"remove": None}),
                make_task("t4", {"never": None}, if_dependencies=["t3"]),
                ("t2", "t1", "e1"),
                ("t4", "t2", "e2"),
                ("t4", "t3", "e3"),
            ),
            {"requested_tasks": {"t3", "t4"}},
            # expectations
            {"t1", "t2", "t3", "t4"},
            id="if_deps_edge_case_1",
            marks=pytest.mark.xfail,
        ),
    ),
)
def test_remove_tasks(monkeypatch, graph, kwargs, exp_removed):
    """Tests the `remove_tasks` function.

    Each test case takes three arguments:

    1. A `TaskGraph` instance.
    2. Keyword arguments to pass into `remove_tasks`.
    3. The set of task labels that are expected to be removed.
    """
    # set up strategies
    strategies = default_strategies()
    monkeypatch.setattr(optimize, "registry", strategies)
    extra = kwargs.pop("strategies", None)
    if extra:
        if callable(extra):
            extra = extra()
        strategies.update(extra)

    kwargs.setdefault("params", {})
    kwargs.setdefault("do_not_optimize", set())
    kwargs.setdefault("requested_tasks", graph)

    got_removed = optimize.remove_tasks(
        target_task_graph=graph,
        optimizations=optimize._get_optimizations(graph, strategies),
        **kwargs
    )
    assert got_removed == exp_removed


@pytest.mark.parametrize(
    "graph,kwargs,exp_replaced,exp_removed,exp_label_to_taskid",
    (
        # A task cannot be replaced if it depends on one that was not replaced
        pytest.param(
            make_triangle(
                t1={"replace": "e1"},
                t3={"replace": "e3"},
            ),
            {},
            # expectations
            {"t1"},
            set(),
            {"t1": "e1"},
            id="blocked",
        ),
        # A task cannot be replaced if it should not be optimized
        pytest.param(
            make_triangle(
                t1={"replace": "e1"},
                t2={"replace": "xxx"},  # but do_not_optimize
                t3={"replace": "e3"},
            ),
            {"do_not_optimize": {"t2"}},
            # expectations
            {"t1"},
            set(),
            {"t1": "e1"},
            id="do_not_optimize",
        ),
        # No tasks are replaced when strategy is 'never'
        pytest.param(
            make_triangle(),
            {},
            # expectations
            set(),
            set(),
            {},
            id="never",
        ),
        # All replacable tasks are replaced when strategy is 'replace'
        pytest.param(
            make_triangle(
                t1={"replace": "e1"},
                t2={"replace": "e2"},
                t3={"replace": "e3"},
            ),
            {},
            # expectations
            {"t1", "t2", "t3"},
            set(),
            {"t1": "e1", "t2": "e2", "t3": "e3"},
            id="all",
        ),
        # A task can be replaced with nothing
        pytest.param(
            make_triangle(
                t1={"replace": "e1"},
                t2={"replace": True},
                t3={"replace": True},
            ),
            {},
            # expectations
            {"t1"},
            {"t2", "t3"},
            {"t1": "e1"},
            id="tasks_removed",
        ),
        # A task which expires before a dependents deadline is not a valid replacement.
        pytest.param(
            make_graph(
                make_task("t1", {"replace": "e1"}),
                make_task(
                    "t2", task_def={"deadline": {"relative-datestamp": "2 days"}}
                ),
                make_task(
                    "t3", task_def={"deadline": {"relative-datestamp": "1 minute"}}
                ),
                ("t2", "t1", "dep1"),
                ("t3", "t1", "dep2"),
            ),
            {},
            # expectations
            set(),
            set(),
            {},
            id="deadline",
        ),
    ),
)
def test_replace_tasks(
    graph,
    kwargs,
    exp_replaced,
    exp_removed,
    exp_label_to_taskid,
):
    """Tests the `replace_tasks` function.

    Each test case takes five arguments:

    1. A `TaskGraph` instance.
    2. Keyword arguments to pass into `replace_tasks`.
    3. The set of task labels that are expected to be replaced.
    4. The set of task labels that are expected to be removed.
    5. The expected label_to_taskid.
    """
    kwargs.setdefault("params", {})
    kwargs.setdefault("do_not_optimize", set())
    kwargs.setdefault("label_to_taskid", {})
    kwargs.setdefault("removed_tasks", set())
    kwargs.setdefault("existing_tasks", {})

    got_replaced = optimize.replace_tasks(
        target_task_graph=graph,
        optimizations=optimize._get_optimizations(graph, default_strategies()),
        **kwargs
    )
    assert got_replaced == exp_replaced
    assert kwargs["removed_tasks"] == exp_removed
    assert kwargs["label_to_taskid"] == exp_label_to_taskid


@pytest.mark.parametrize(
    "graph,kwargs,exp_subgraph,exp_label_to_taskid",
    (
        # Test get_subgraph returns a similarly-shaped subgraph when nothing is removed
        pytest.param(
            make_triangle(deps=False),
            {},
            make_opt_graph(
                make_task("t1", task_id="tid1", dependencies={}),
                make_task("t2", task_id="tid2", dependencies={"tid1"}),
                make_task("t3", task_id="tid3", dependencies={"tid1", "tid2"}),
                ("tid3", "tid2", "dep"),
                ("tid3", "tid1", "dep2"),
                ("tid2", "tid1", "dep"),
            ),
            {"t1": "tid1", "t2": "tid2", "t3": "tid3"},
            id="no_change",
        ),
        # Test get_subgraph returns a smaller subgraph when tasks are removed
        pytest.param(
            make_triangle(deps=False),
            {
                "removed_tasks": {"t2", "t3"},
            },
            # expectations
            make_opt_graph(make_task("t1", task_id="tid1", dependencies={})),
            {"t1": "tid1"},
            id="removed",
        ),
        # Test get_subgraph returns a smaller subgraph when tasks are replaced
        pytest.param(
            make_triangle(deps=False),
            {
                "replaced_tasks": {"t1", "t2"},
                "label_to_taskid": {"t1": "e1", "t2": "e2"},
            },
            # expectations
            make_opt_graph(make_task("t3", task_id="tid1", dependencies={"e1", "e2"})),
            {"t1": "e1", "t2": "e2", "t3": "tid1"},
            id="replaced",
        ),
    ),
)
def test_get_subgraph(monkeypatch, graph, kwargs, exp_subgraph, exp_label_to_taskid):
    """Tests the `get_subgraph` function.

    Each test case takes 4 arguments:

    1. A `TaskGraph` instance.
    2. Keyword arguments to pass into `get_subgraph`.
    3. The expected subgraph.
    4. The expected label_to_taskid.
    """
    monkeypatch.setattr(
        optimize, "slugid", partial(next, (b"tid%d" % i for i in range(1, 10)))
    )

    kwargs.setdefault("removed_tasks", set())
    kwargs.setdefault("replaced_tasks", set())
    kwargs.setdefault("label_to_taskid", {})
    kwargs.setdefault("decision_task_id", "DECISION-TASK")

    got_subgraph = optimize.get_subgraph(graph, **kwargs)
    assert got_subgraph.graph == exp_subgraph.graph
    assert got_subgraph.tasks == exp_subgraph.tasks
    assert kwargs["label_to_taskid"] == exp_label_to_taskid


def test_get_subgraph_removed_dep():
    "get_subgraph raises an Exception when a task depends on a removed task"
    graph = make_triangle()
    with pytest.raises(Exception):
        optimize.get_subgraph(graph, {"t2"}, set(), {})


if __name__ == "__main__":
    main()
