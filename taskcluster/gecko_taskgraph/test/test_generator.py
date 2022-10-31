# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozunit import main
from taskgraph.graph import Graph

from gecko_taskgraph.generator import load_tasks_for_kind
from gecko_taskgraph import (
    generator,
)

from conftest import (
    FakeKind,
    WithFakeKind,
    fake_load_graph_config,
)


def test_kind_ordering(maketgg):
    "When task kinds depend on each other, they are loaded in postorder"
    tgg = maketgg(
        kinds=[
            ("_fake3", {"kind-dependencies": ["_fake2", "_fake1"]}),
            ("_fake2", {"kind-dependencies": ["_fake1"]}),
            ("_fake1", {"kind-dependencies": []}),
        ]
    )
    tgg._run_until("full_task_set")
    assert tgg.loaded_kinds == ["_fake1", "_fake2", "_fake3"]


def test_full_task_set(maketgg):
    "The full_task_set property has all tasks"
    tgg = maketgg()
    assert tgg.full_task_set.graph == Graph(
        {"_fake-t-0", "_fake-t-1", "_fake-t-2"}, set()
    )
    assert sorted(tgg.full_task_set.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_fake-t-2"]
    )


def test_full_task_graph(maketgg):
    "The full_task_graph property has all tasks, and links"
    tgg = maketgg()
    assert tgg.full_task_graph.graph == Graph(
        {"_fake-t-0", "_fake-t-1", "_fake-t-2"},
        {
            ("_fake-t-1", "_fake-t-0", "prev"),
            ("_fake-t-2", "_fake-t-1", "prev"),
        },
    )
    assert sorted(tgg.full_task_graph.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_fake-t-2"]
    )


def test_target_task_set(maketgg):
    "The target_task_set property has the targeted tasks"
    tgg = maketgg(["_fake-t-1"])
    assert tgg.target_task_set.graph == Graph({"_fake-t-1"}, set())
    assert set(tgg.target_task_set.tasks.keys()) == {"_fake-t-1"}


def test_target_task_graph(maketgg):
    "The target_task_graph property has the targeted tasks and deps"
    tgg = maketgg(["_fake-t-1"])
    assert tgg.target_task_graph.graph == Graph(
        {"_fake-t-0", "_fake-t-1"}, {("_fake-t-1", "_fake-t-0", "prev")}
    )
    assert sorted(tgg.target_task_graph.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1"]
    )


def test_always_target_tasks(maketgg):
    "The target_task_graph includes tasks with 'always_target'"
    tgg_args = {
        "target_tasks": ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1"],
        "kinds": [
            ("_fake", {"job-defaults": {"optimization": {"odd": None}}}),
            (
                "_ignore",
                {
                    "job-defaults": {
                        "attributes": {"always_target": True},
                        "optimization": {"even": None},
                    }
                },
            ),
        ],
        "params": {"optimize_target_tasks": False},
    }
    tgg = maketgg(**tgg_args)
    assert sorted(tgg.target_task_set.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1"]
    )
    assert sorted(tgg.target_task_graph.tasks.keys()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1", "_ignore-t-2"]
    )
    assert sorted(t.label for t in tgg.optimized_task_graph.tasks.values()) == sorted(
        ["_fake-t-0", "_fake-t-1", "_ignore-t-0", "_ignore-t-1"]
    )


def test_optimized_task_graph(maketgg):
    "The optimized task graph contains task ids"
    tgg = maketgg(["_fake-t-2"])
    tid = tgg.label_to_taskid
    assert tgg.optimized_task_graph.graph == Graph(
        {tid["_fake-t-0"], tid["_fake-t-1"], tid["_fake-t-2"]},
        {
            (tid["_fake-t-1"], tid["_fake-t-0"], "prev"),
            (tid["_fake-t-2"], tid["_fake-t-1"], "prev"),
        },
    )


def test_load_tasks_for_kind(monkeypatch):
    """
    `load_tasks_for_kinds` will load the tasks for the provided kind
    """
    FakeKind.loaded_kinds = []
    monkeypatch.setattr(generator, "TaskGraphGenerator", WithFakeKind)
    monkeypatch.setattr(generator, "load_graph_config", fake_load_graph_config)

    tasks = load_tasks_for_kind(
        {"_kinds": [("_example-kind", []), ("docker-image", [])]},
        "_example-kind",
        "/root",
    )
    assert "t-1" in tasks and tasks["t-1"].label == "_example-kind-t-1"


if __name__ == "__main__":
    main()
