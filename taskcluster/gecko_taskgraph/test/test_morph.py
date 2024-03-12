# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import pytest
from mozunit import main
from taskgraph.graph import Graph
from taskgraph.parameters import Parameters
from taskgraph.task import Task
from taskgraph.taskgraph import TaskGraph

from gecko_taskgraph import morph


@pytest.fixture
def make_taskgraph():
    def inner(tasks):
        label_to_taskid = {k: k + "-tid" for k in tasks}
        for label, task_id in label_to_taskid.items():
            tasks[label].task_id = task_id
        graph = Graph(nodes=set(tasks), edges=set())
        taskgraph = TaskGraph(tasks, graph)
        return taskgraph, label_to_taskid

    return inner


@pytest.mark.parametrize(
    "params,expected",
    (
        pytest.param(
            {
                "try_mode": "try_task_config",
                "try_task_config": {
                    "rebuild": 10,
                    "tasks": ["b"],
                },
                "project": "try",
            },
            {"b": 10},
            id="duplicates no chunks",
        ),
        pytest.param(
            {
                "try_mode": "try_task_config",
                "try_task_config": {
                    "rebuild": 10,
                    "tasks": ["a-*"],
                },
                "project": "try",
            },
            {"a-1": 10, "a-2": 10},
            id="duplicates with chunks",
        ),
        pytest.param(
            {
                "try_mode": "try_task_config",
                "try_task_config": {
                    "rebuild": 10,
                    "tasks": ["a-*", "b"],
                },
                "project": "try",
            },
            {"a-1": 10, "a-2": 10, "b": 10},
            id="duplicates with and without chunks",
        ),
        pytest.param(
            {
                "try_mode": "try_task_config",
                "try_task_config": {
                    "tasks": ["a-*"],
                },
                "project": "try",
            },
            {"a-1": -1, "a-2": -1},
            id="no rebuild, no duplicates",
        ),
        pytest.param(
            {
                "try_mode": "try_task_config",
                "try_task_config": {
                    "rebuild": 0,
                    "tasks": ["a-*"],
                },
                "project": "try",
            },
            {"a-1": -1, "a-2": -1},
            id="rebuild of zero",
        ),
        pytest.param(
            {
                "try_mode": "try_task_config",
                "try_task_config": {
                    "rebuild": 100,
                    "tasks": ["a-*"],
                },
                "project": "try",
            },
            {"a-1": 100, "a-2": 100},
            id="rebuild 100",
        ),
    ),
)
def test_try_task_duplicates(make_taskgraph, graph_config, params, expected):
    taskb = Task(kind="test", label="b", attributes={}, task={})
    task1 = Task(kind="test", label="a-1", attributes={}, task={})
    task2 = Task(kind="test", label="a-2", attributes={}, task={})
    taskgraph, label_to_taskid = make_taskgraph(
        {
            taskb.label: taskb,
            task1.label: task1,
            task2.label: task2,
        }
    )

    taskgraph, label_to_taskid = morph._add_try_task_duplicates(
        taskgraph, label_to_taskid, params, graph_config
    )
    for label in expected:
        task = taskgraph.tasks[label]
        assert task.attributes.get("task_duplicates", -1) == expected[label]


def test_make_index_tasks(make_taskgraph, graph_config):
    task_def = {
        "routes": [
            "index.gecko.v2.mozilla-central.latest.firefox-l10n.linux64-opt.es-MX",
            "index.gecko.v2.mozilla-central.latest.firefox-l10n.linux64-opt.fy-NL",
            "index.gecko.v2.mozilla-central.latest.firefox-l10n.linux64-opt.sk",
            "index.gecko.v2.mozilla-central.latest.firefox-l10n.linux64-opt.sl",
            "index.gecko.v2.mozilla-central.latest.firefox-l10n.linux64-opt.uk",
            "index.gecko.v2.mozilla-central.latest.firefox-l10n.linux64-opt.zh-CN",
            "index.gecko.v2.mozilla-central.pushdate."
            "2017.04.04.20170404100210.firefox-l10n.linux64-opt.es-MX",
            "index.gecko.v2.mozilla-central.pushdate."
            "2017.04.04.20170404100210.firefox-l10n.linux64-opt.fy-NL",
            "index.gecko.v2.mozilla-central.pushdate."
            "2017.04.04.20170404100210.firefox-l10n.linux64-opt.sk",
            "index.gecko.v2.mozilla-central.pushdate."
            "2017.04.04.20170404100210.firefox-l10n.linux64-opt.sl",
            "index.gecko.v2.mozilla-central.pushdate."
            "2017.04.04.20170404100210.firefox-l10n.linux64-opt.uk",
            "index.gecko.v2.mozilla-central.pushdate."
            "2017.04.04.20170404100210.firefox-l10n.linux64-opt.zh-CN",
            "index.gecko.v2.mozilla-central.revision."
            "b5d8b27a753725c1de41ffae2e338798f3b5cacd.firefox-l10n.linux64-opt.es-MX",
            "index.gecko.v2.mozilla-central.revision."
            "b5d8b27a753725c1de41ffae2e338798f3b5cacd.firefox-l10n.linux64-opt.fy-NL",
            "index.gecko.v2.mozilla-central.revision."
            "b5d8b27a753725c1de41ffae2e338798f3b5cacd.firefox-l10n.linux64-opt.sk",
            "index.gecko.v2.mozilla-central.revision."
            "b5d8b27a753725c1de41ffae2e338798f3b5cacd.firefox-l10n.linux64-opt.sl",
            "index.gecko.v2.mozilla-central.revision."
            "b5d8b27a753725c1de41ffae2e338798f3b5cacd.firefox-l10n.linux64-opt.uk",
            "index.gecko.v2.mozilla-central.revision."
            "b5d8b27a753725c1de41ffae2e338798f3b5cacd.firefox-l10n.linux64-opt.zh-CN",
        ],
        "deadline": "soon",
        "metadata": {
            "description": "desc",
            "owner": "owner@foo.com",
            "source": "https://source",
        },
        "extra": {
            "index": {"rank": 1540722354},
        },
    }
    task = Task(kind="test", label="a", attributes={}, task=task_def)
    docker_task = Task(
        kind="docker-image", label="docker-image-index-task", attributes={}, task={}
    )
    taskgraph, label_to_taskid = make_taskgraph(
        {
            task.label: task,
            docker_task.label: docker_task,
        }
    )

    index_paths = [
        r.split(".", 1)[1] for r in task_def["routes"] if r.startswith("index.")
    ]
    index_task = morph.make_index_task(
        task,
        taskgraph,
        label_to_taskid,
        Parameters(strict=False),
        graph_config,
        index_paths=index_paths,
        index_rank=1540722354,
        purpose="index-task",
        dependencies={},
    )

    assert index_task.task["payload"]["command"][0] == "insert-indexes.js"
    assert index_task.task["payload"]["env"]["TARGET_TASKID"] == "a-tid"
    assert index_task.task["payload"]["env"]["INDEX_RANK"] == 1540722354

    # check the scope summary
    assert index_task.task["scopes"] == ["index:insert-task:gecko.v2.mozilla-central.*"]


if __name__ == "__main__":
    main()
