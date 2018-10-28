# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import pytest

from taskgraph import morph
from taskgraph.graph import Graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task

from mozunit import main


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


def test_make_index_tasks(make_taskgraph):
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

    index_task = morph.make_index_task(task, taskgraph, label_to_taskid)

    assert index_task.task['payload']['command'][0] == 'insert-indexes.js'
    assert index_task.task['payload']['env']['TARGET_TASKID'] == 'a-tid'
    assert index_task.task['payload']['env']['INDEX_RANK'] == 1540722354

    # check the scope summary
    assert index_task.task['scopes'] == ['index:insert-task:gecko.v2.mozilla-central.*']


TASKS = [
    {
        'kind': 'build',
        'label': 'a',
        'attributes': {},
        'task': {
            'extra': {
                'treeherder': {
                    'group': 'tc',
                    'symbol': 'B'
                }
            },
            'payload': {
                'env': {
                    'FOO': 'BAR'
                }
            },
            'tags': {
                'kind': 'build'
            }
        }
    },
    {
        'kind': 'test',
        'label': 'b',
        'attributes': {},
        'task': {
            'extra': {
                'suite': {'name': 'talos'},
                'treeherder': {
                    'group': 'tc',
                    'symbol': 't'
                }
            },
            'payload': {
                'env': {
                    'FOO': 'BAR'
                }
            },
            'tags': {
                'kind': 'test'
            }
        }
    },
]


@pytest.fixture
def get_morphed(make_taskgraph):
    def inner(try_task_config, tasks=None):
        tasks = tasks or TASKS
        taskgraph = make_taskgraph({
            t['label']: Task(**t) for t in tasks[:]
        })

        fn = morph.apply_jsone_templates(try_task_config)
        return fn(*taskgraph)[0]
    return inner


def test_template_artifact(get_morphed):
    morphed = get_morphed({
        'templates': {
            'artifact': {'enabled': 1}
        },
    })

    assert len(morphed.tasks) == 2

    for t in morphed.tasks.values():
        if t.kind == 'build':
            assert t.task['extra']['treeherder']['group'] == 'tc'
            assert t.task['extra']['treeherder']['symbol'] == 'Ba'
            assert t.task['payload']['env']['USE_ARTIFACT'] == 1
        else:
            assert t.task['extra']['treeherder']['group'] == 'tc'
            assert t.task['extra']['treeherder']['symbol'] == 't'
            assert 'USE_ARTIFACT' not in t.task['payload']['env']


def test_template_env(get_morphed):
    morphed = get_morphed({
        'templates': {
            'env': {
                'ENABLED': 1,
                'FOO': 'BAZ',
            }
        },
    })

    assert len(morphed.tasks) == 2
    for t in morphed.tasks.values():
        assert len(t.task['payload']['env']) == 2
        assert t.task['payload']['env']['ENABLED'] == 1
        assert t.task['payload']['env']['FOO'] == 'BAZ'

    morphed = get_morphed({
        'templates': {
            'env': {
                'ENABLED': 0,
                'FOO': 'BAZ',
            }
        },
    })

    assert len(morphed.tasks) == 2
    for t in morphed.tasks.values():
        assert len(t.task['payload']['env']) == 2
        assert t.task['payload']['env']['ENABLED'] == 0
        assert t.task['payload']['env']['FOO'] == 'BAZ'


def test_template_rebuild(get_morphed):
    morphed = get_morphed({
        'tasks': ['b'],
        'templates': {
            'rebuild': 4,
        },
    })
    tasks = morphed.tasks.values()
    assert len(tasks) == 2

    for t in tasks:
        if t.label == 'a':
            assert 'task_duplicates' not in t.attributes
        elif t.label == 'b':
            assert 'task_duplicates' in t.attributes
            assert t.attributes['task_duplicates'] == 4


@pytest.mark.parametrize('command', (
    ['foo --bar'],
    ['foo', '--bar'],
    [['foo']],
    [['foo', '--bar']],
))
def test_template_talos_profile(get_morphed, command):
    tasks = TASKS[:]
    for t in tasks:
        t['task']['payload']['command'] = command

    morphed = get_morphed({
        'templates': {
            'talos-profile': True,
        }
    }, tasks)

    for t in morphed.tasks.values():
        command = t.task['payload']['command']
        if isinstance(command[0], list):
            command = command[0]
        command = ' '.join(command)

        if t.label == 'a':
            assert not command.endswith('--geckoProfile')
        elif t.label == 'b':
            assert command.endswith('--geckoProfile')


if __name__ == '__main__':
    main()
