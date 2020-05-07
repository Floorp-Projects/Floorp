# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import json
import shutil
import sys
import unittest
import tempfile

import pytest
from mock import patch
from mozunit import main, MockedOpen
from taskgraph import decision
from taskgraph.util.yaml import load_yaml


FAKE_GRAPH_CONFIG = {'product-dir': 'browser', 'taskgraph': {}}


class TestDecision(unittest.TestCase):

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
            self.assertEqual(load_yaml(decision.ARTIFACTS_DIR, "artifact.yml"), data)
        finally:
            if os.path.exists(tmpdir):
                shutil.rmtree(tmpdir)
            decision.ARTIFACTS_DIR = 'artifacts'


class TestGetDecisionParameters(unittest.TestCase):

    ttc_file = os.path.join(os.getcwd(), 'try_task_config.json')

    def setUp(self):
        self.options = {
            'base_repository': 'https://hg.mozilla.org/mozilla-unified',
            'head_repository': 'https://hg.mozilla.org/mozilla-central',
            'head_rev': 'abcd',
            'head_ref': 'ef01',
            'message': '',
            'project': 'mozilla-central',
            'pushlog_id': '143',
            'pushdate': 1503691511,
            'owner': 'nobody@mozilla.com',
            'tasks_for': 'hg-push',
            'level': '3',
        }

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    @patch('taskgraph.decision.get_hg_revision_branch')
    def test_simple_options(self, mock_get_hg_revision_branch):
        mock_get_hg_revision_branch.return_value = 'default'
        with MockedOpen({self.ttc_file: None}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
        self.assertEqual(params['pushlog_id'], '143')
        self.assertEqual(params['build_date'], 1503691511)
        self.assertEqual(params['hg_branch'], 'default')
        self.assertEqual(params['moz_build_date'], '20170825200511')
        self.assertEqual(params['try_mode'], None)
        self.assertEqual(params['try_options'], None)
        self.assertEqual(params['try_task_config'], {})

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    @patch('taskgraph.decision.get_hg_revision_branch')
    def test_no_email_owner(self, mock_get_hg_revision_branch):
        mock_get_hg_revision_branch.return_value = 'default'
        self.options['owner'] = 'ffxbld'
        with MockedOpen({self.ttc_file: None}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
        self.assertEqual(params['owner'], 'ffxbld@noreply.mozilla.org')

    @patch('taskgraph.decision.get_hg_revision_branch')
    @patch('taskgraph.decision.get_hg_commit_message')
    def test_try_options(self, mock_get_hg_commit_message, mock_get_hg_revision_branch):
        mock_get_hg_commit_message.return_value = 'try: -b do -t all --artifact'
        mock_get_hg_revision_branch.return_value = 'default'
        self.options['project'] = 'try'
        with MockedOpen({self.ttc_file: None}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
        self.assertEqual(params['try_mode'], 'try_option_syntax')
        self.assertEqual(params['try_options']['build_types'], 'do')
        self.assertEqual(params['try_options']['unittests'], 'all')
        self.assertEqual(params['try_task_config'], {
            'gecko-profile': False,
            'use-artifact-builds': True,
            'env': {},
        })

    @patch('taskgraph.decision.get_hg_revision_branch')
    @patch('taskgraph.decision.get_hg_commit_message')
    def test_try_task_config(self, mock_get_hg_commit_message, mock_get_hg_revision_branch):
        mock_get_hg_commit_message.return_value = 'Fuzzy query=foo'
        mock_get_hg_revision_branch.return_value = 'default'
        ttc = {'tasks': ['a', 'b']}
        self.options['project'] = 'try'
        with MockedOpen({self.ttc_file: json.dumps(ttc)}):
            params = decision.get_decision_parameters(FAKE_GRAPH_CONFIG, self.options)
            self.assertEqual(params['try_mode'], 'try_task_config')
            self.assertEqual(params['try_options'], None)
            self.assertEqual(params['try_task_config'], ttc)

    def test_try_syntax_from_message_empty(self):
        self.assertEqual(decision.try_syntax_from_message(''), '')

    def test_try_syntax_from_message_no_try_syntax(self):
        self.assertEqual(decision.try_syntax_from_message('abc | def'), '')

    def test_try_syntax_from_message_initial_try_syntax(self):
        self.assertEqual(decision.try_syntax_from_message('try: -f -o -o'), 'try: -f -o -o')

    def test_try_syntax_from_message_initial_try_syntax_multiline(self):
        self.assertEqual(
            decision.try_syntax_from_message('try: -f -o -o\nabc\ndef'),
            'try: -f -o -o')

    def test_try_syntax_from_message_embedded_try_syntax_multiline(self):
        self.assertEqual(
            decision.try_syntax_from_message('some stuff\ntry: -f -o -o\nabc\ndef'),
            'try: -f -o -o')


if __name__ == '__main__':
    main()
