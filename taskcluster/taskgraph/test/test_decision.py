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

from taskgraph import decision
from mozunit import main, MockedOpen


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
            with open(os.path.join(decision.ARTIFACTS_DIR, "artifact.yml")) as f:
                self.assertEqual(yaml.safe_load(f), data)
        finally:
            if os.path.exists(tmpdir):
                shutil.rmtree(tmpdir)
            decision.ARTIFACTS_DIR = 'artifacts'


class TestGetDecisionParameters(unittest.TestCase):

    def setUp(self):
        self.options = {
            'base_repository': 'https://hg.mozilla.org/mozilla-unified',
            'head_repository': 'https://hg.mozilla.org/mozilla-central',
            'head_rev': 'abcd',
            'head_ref': 'ef01',
            'message': '',
            'project': 'mozilla-central',
            'pushlog_id': 143,
            'pushdate': 1503691511,
            'owner': 'nobody@mozilla.com',
            'level': 3,
        }

    def test_simple_options(self):
        params = decision.get_decision_parameters(self.options)
        self.assertEqual(params['pushlog_id'], 143)
        self.assertEqual(params['build_date'], 1503691511)
        self.assertEqual(params['moz_build_date'], '20170825200511')
        self.assertEqual(params['try_mode'], None)
        self.assertEqual(params['try_options'], None)
        self.assertEqual(params['try_task_config'], None)

    def test_no_email_owner(self):
        self.options['owner'] = 'ffxbld'
        params = decision.get_decision_parameters(self.options)
        self.assertEqual(params['owner'], 'ffxbld@noreply.mozilla.org')

    def test_try_options(self):
        self.options['message'] = 'try: -b do -t all'
        self.options['project'] = 'try'
        params = decision.get_decision_parameters(self.options)
        self.assertEqual(params['try_mode'], 'try_option_syntax')
        self.assertEqual(params['try_options']['build_types'], 'do')
        self.assertEqual(params['try_options']['unittests'], 'all')
        self.assertEqual(params['try_task_config'], None)

    def test_try_task_config(self):
        ttc = {'tasks': ['a', 'b'], 'templates': {}}
        ttc_file = os.path.join(os.getcwd(), 'try_task_config.json')
        self.options['project'] = 'try'
        with MockedOpen({ttc_file: json.dumps(ttc)}):
            params = decision.get_decision_parameters(self.options)
            self.assertEqual(params['try_mode'], 'try_task_config')
            self.assertEqual(params['try_options'], None)
            self.assertEqual(params['try_task_config'], ttc)


if __name__ == '__main__':
    main()
