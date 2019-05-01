# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import json
from mock import patch
from mozunit import main, MockedOpen
from taskgraph.decision import read_artifact
from taskgraph.actions.util import (
    relativize_datestamps,
    combine_task_graph_files,
)

TASK_DEF = {
    'created': '2017-10-10T18:33:03.460Z',
    # note that this is not an even number of seconds off!
    'deadline': '2017-10-11T18:33:03.461Z',
    'dependencies': [],
    'expires': '2018-10-10T18:33:04.461Z',
    'payload': {
        'artifacts': {
            'public': {
                'expires': '2018-10-10T18:33:03.463Z',
                'path': '/builds/worker/artifacts',
                'type': 'directory',
            },
        },
        'maxRunTime': 1800,
    },
}


class TestRelativize(unittest.TestCase):

    def test_relativize(self):
        rel = relativize_datestamps(TASK_DEF)
        import pprint
        pprint.pprint(rel)
        assert rel['created'] == {'relative-datestamp': '0 seconds'}
        assert rel['deadline'] == {'relative-datestamp': '86400 seconds'}
        assert rel['expires'] == {'relative-datestamp': '31536001 seconds'}
        assert rel['payload']['artifacts']['public']['expires'] == \
            {'relative-datestamp': '31536000 seconds'}


class TestCombineTaskGraphFiles(unittest.TestCase):

    def test_no_suffixes(self):
        with MockedOpen({}):
            combine_task_graph_files([])
            self.assertRaises(Exception, open('artifacts/to-run.json'))

    @patch('taskgraph.actions.util.rename_artifact')
    def test_one_suffix(self, rename_artifact):
        combine_task_graph_files(['0'])
        rename_artifact.assert_any_call('task-graph-0.json', 'task-graph.json')
        rename_artifact.assert_any_call('label-to-taskid-0.json', 'label-to-taskid.json')
        rename_artifact.assert_any_call('to-run-0.json', 'to-run.json')

    def test_several_suffixes(self):
        files = {
            'artifacts/task-graph-0.json': json.dumps({'taska': {}}),
            'artifacts/label-to-taskid-0.json': json.dumps({'taska': 'TASKA'}),
            'artifacts/to-run-0.json': json.dumps(['taska']),
            'artifacts/task-graph-1.json': json.dumps({'taskb': {}}),
            'artifacts/label-to-taskid-1.json': json.dumps({'taskb': 'TASKB'}),
            'artifacts/to-run-1.json': json.dumps(['taskb']),
        }
        with MockedOpen(files):
            combine_task_graph_files(['0', '1'])
            self.assertEqual(read_artifact('task-graph.json'), {
                'taska': {},
                'taskb': {},
            })
            self.assertEqual(read_artifact('label-to-taskid.json'), {
                'taska': 'TASKA',
                'taskb': 'TASKB',
            })
            self.assertEqual(sorted(read_artifact('to-run.json')), [
                'taska',
                'taskb',
            ])


if __name__ == '__main__':
    main()
