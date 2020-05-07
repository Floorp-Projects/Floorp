# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys

import pytest

from taskgraph import morph, GECKO
from taskgraph.config import load_graph_config
from taskgraph.graph import Graph
from taskgraph.parameters import Parameters
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task

from mozunit import main


@pytest.fixture(scope='module')
def graph_config():
    return load_graph_config(os.path.join(GECKO, 'taskcluster', 'ci'))


@pytest.fixture
def make_taskgraph():
    def inner(tasks):
        label_to_taskid = {k: k + '-tid' for k in tasks}
        for label, task_id in label_to_taskid.iteritems():
            tasks[label].task_id = task_id
        graph = Graph(nodes=set(tasks), edges=set())
        taskgraph = TaskGraph(tasks, graph)
        return taskgraph, label_to_taskid

    return inner


@pytest.mark.xfail(
    sys.version_info >= (3, 0), reason="python3 migration is not complete"
)
def test_make_index_tasks(make_taskgraph, graph_config):
    task_def = {
        'routes': [
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
        'deadline': 'soon',
        'metadata': {
            'description': 'desc',
            'owner': 'owner@foo.com',
            'source': 'https://source',
        },
        'extra': {
            'index': {'rank': 1540722354},
        },
    }
    task = Task(kind='test', label='a', attributes={}, task=task_def)
    docker_task = Task(kind='docker-image', label='build-docker-image-index-task',
                       attributes={}, task={})
    taskgraph, label_to_taskid = make_taskgraph({
        task.label: task,
        docker_task.label: docker_task,
    })

    index_task = morph.make_index_task(
        task, taskgraph, label_to_taskid, Parameters(strict=False), graph_config
    )

    assert index_task.task['payload']['command'][0] == 'insert-indexes.js'
    assert index_task.task['payload']['env']['TARGET_TASKID'] == 'a-tid'
    assert index_task.task['payload']['env']['INDEX_RANK'] == 1540722354

    # check the scope summary
    assert index_task.task['scopes'] == ['index:insert-task:gecko.v2.mozilla-central.*']


if __name__ == '__main__':
    main()
