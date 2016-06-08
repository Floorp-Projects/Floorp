# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import json
import yaml
import shutil
import unittest
import tempfile

from .. import decision
from ..graph import Graph
from ..types import Task, TaskGraph
from mozunit import main

class TestDecision(unittest.TestCase):

    def test_taskgraph_to_json(self):
        tasks = {
            'a': Task(kind=None, label='a', attributes={'attr': 'a-task'}),
            'b': Task(kind=None, label='b', task={'task': 'def'}),
        }
        graph = Graph(nodes=set('ab'), edges={('a', 'b', 'edgelabel')})
        taskgraph = TaskGraph(tasks, graph)

        res = decision.taskgraph_to_json(taskgraph)

        self.assertEqual(res, {
            'a': {
                'label': 'a',
                'attributes': {'attr': 'a-task'},
                'task': {},
                'dependencies': {'edgelabel': 'b'},
            },
            'b': {
                'label': 'b',
                'attributes': {},
                'task': {'task': 'def'},
                'dependencies': {},
            }
        })


    def test_write_artifact_json(self):
        data = [{'some': 'data'}]
        tmpdir = tempfile.mkdtemp()
        try:
            decision.ARTIFACTS_DIR = os.path.join(tmpdir, "artifacts")
            decision.write_artifact("artifact.json", data)
            with open(os.path.join(decision.ARTIFACTS_DIR, "artifact.json")) as f:
                self.assertEqual(json.load(f), data)
        finally:
            if os.path.exists(tmpdir):
                shutil.rmtree(tmpdir)
            decision.ARTIFACTS_DIR = 'artifacts'


    def test_write_artifact_yml(self):
        data = [{'some': 'data'}]
        tmpdir = tempfile.mkdtemp()
        try:
            decision.ARTIFACTS_DIR = os.path.join(tmpdir, "artifacts")
            decision.write_artifact("artifact.yml", data)
            with open(os.path.join(decision.ARTIFACTS_DIR, "artifact.yml")) as f:
                self.assertEqual(yaml.safe_load(f), data)
        finally:
            if os.path.exists(tmpdir):
                shutil.rmtree(tmpdir)
            decision.ARTIFACTS_DIR = 'artifacts'


if __name__ == '__main__':
    main()


