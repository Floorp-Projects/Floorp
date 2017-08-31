# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from taskgraph import morph
from taskgraph.graph import Graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.task import Task

from mozunit import main


class MorphTestCase(unittest.TestCase):

    def make_taskgraph(self, tasks):
        label_to_taskid = {k: k + '-tid' for k in tasks}
        for label, task_id in label_to_taskid.iteritems():
            tasks[label].task_id = task_id
        graph = Graph(nodes=set(tasks), edges=set())
        taskgraph = TaskGraph(tasks, graph)
        return taskgraph, label_to_taskid


class TestIndexTask(MorphTestCase):

    def test_make_index_tasks(self):
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
                "b5d8b27a753725c1de41ffae2e338798f3b5cacd.firefox-l10n.linux64-opt.zh-CN"
            ],
            'deadline': 'soon',
            'metadata': {
                'description': 'desc',
                'owner': 'owner@foo.com',
                'source': 'https://source',
            },
        }
        task = Task(kind='test', label='a', attributes={}, task=task_def)
        docker_task = Task(kind='docker-image', label='build-docker-image-index-task',
                           attributes={}, task={})
        taskgraph, label_to_taskid = self.make_taskgraph({
            task.label: task,
            docker_task.label: docker_task,
        })

        index_task = morph.make_index_task(task, taskgraph, label_to_taskid)

        self.assertEqual(index_task.task['payload']['command'][0], 'insert-indexes.js')
        self.assertEqual(index_task.task['payload']['env']['TARGET_TASKID'], 'a-tid')

        # check the scope summary
        self.assertEqual(index_task.task['scopes'],
                         ['index:insert-task:gecko.v2.mozilla-central.*'])


class TestApplyJSONeTemplates(MorphTestCase):

    tasks = [
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

    def test_template_artifact(self):
        tg, label_to_taskid = self.make_taskgraph({
            t['label']: Task(**t) for t in self.tasks[:]
        })

        fn = morph.apply_jsone_templates({'artifact': {'enabled': 1}})
        morphed = fn(tg, label_to_taskid)[0]

        self.assertEqual(len(morphed.tasks), 2)

        for t in morphed.tasks.values():
            if t.kind == 'build':
                self.assertEqual(t.task['extra']['treeherder']['group'], 'tc')
                self.assertEqual(t.task['extra']['treeherder']['symbol'], 'Ba')
                self.assertEqual(t.task['payload']['env']['USE_ARTIFACT'], 1)
            else:
                self.assertEqual(t.task['extra']['treeherder']['group'], 'tc')
                self.assertEqual(t.task['extra']['treeherder']['symbol'], 't')
                self.assertNotIn('USE_ARTIFACT', t.task['payload']['env'])

    def test_template_env(self):
        tg, label_to_taskid = self.make_taskgraph({
            t['label']: Task(**t) for t in self.tasks[:]
        })

        fn = morph.apply_jsone_templates({'env': {'ENABLED': 1, 'FOO': 'BAZ'}})
        morphed = fn(tg, label_to_taskid)[0]

        self.assertEqual(len(morphed.tasks), 2)
        for t in morphed.tasks.values():
            self.assertEqual(len(t.task['payload']['env']), 2)
            self.assertEqual(t.task['payload']['env']['ENABLED'], 1)
            self.assertEqual(t.task['payload']['env']['FOO'], 'BAZ')

        fn = morph.apply_jsone_templates({'env': {'ENABLED': 0}})
        morphed = fn(tg, label_to_taskid)[0]

        self.assertEqual(len(morphed.tasks), 2)
        for t in morphed.tasks.values():
            self.assertEqual(len(t.task['payload']['env']), 2)
            self.assertEqual(t.task['payload']['env']['ENABLED'], 0)
            self.assertEqual(t.task['payload']['env']['FOO'], 'BAZ')


if __name__ == '__main__':
    main()
