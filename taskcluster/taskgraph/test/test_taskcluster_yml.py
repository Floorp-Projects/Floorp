# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import sys

import jsone
import pprint

import pytest
import slugid
import unittest

from mozunit import main

from taskgraph.util.yaml import load_yaml
from taskgraph.util.time import current_json_time
from taskgraph import GECKO


class TestTaskclusterYml(unittest.TestCase):

    @property
    def taskcluster_yml(self):
        return load_yaml(GECKO, ".taskcluster.yml")

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    def test_push(self):
        context = {
            "tasks_for": "hg-push",
            "push": {
                "revision": "e8d2d9aff5026ef1f1777b781b47fdcbdb9d8f20",
                "owner": "dustin@mozilla.com",
                "pushlog_id": 1556565286,
                "pushdate": 112957,
            },
            "repository": {
                "url": "https://hg.mozilla.org/mozilla-central",
                "project": "mozilla-central",
                "level": "3",
            },
            "ownTaskId": slugid.nice().encode("ascii"),
        }
        rendered = jsone.render(self.taskcluster_yml, context)
        pprint.pprint(rendered)
        self.assertEqual(
            rendered["tasks"][0]["metadata"]["name"], "Gecko Decision Task"
        )

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    def test_cron(self):
        context = {
            "tasks_for": "cron",
            "repository": {
                "url": "https://hg.mozilla.org/mozilla-central",
                "project": "mozilla-central",
                "level": 3,
            },
            "push": {
                "revision": "e8aebe488b2f2e567940577de25013d00e818f7c",
                "pushlog_id": -1,
                "pushdate": 0,
                "owner": "cron",
            },
            "cron": {
                "task_id": "<cron task id>",
                "job_name": "test",
                "job_symbol": "T",
                "quoted_args": "abc def",
            },
            "now": current_json_time(),
            "ownTaskId": slugid.nice().encode("ascii"),
        }
        rendered = jsone.render(self.taskcluster_yml, context)
        pprint.pprint(rendered)
        self.assertEqual(
            rendered["tasks"][0]["metadata"]["name"], "Decision Task for cron job test"
        )

    @pytest.mark.xfail(
        sys.version_info >= (3, 0), reason="python3 migration is not complete"
    )
    def test_action(self):
        context = {
            "tasks_for": "action",
            "repository": {
                "url": "https://hg.mozilla.org/mozilla-central",
                "project": "mozilla-central",
                "level": 3,
            },
            "push": {
                "revision": "e8d2d9aff5026ef1f1777b781b47fdcbdb9d8f20",
                "owner": "dustin@mozilla.com",
                "pushlog_id": 1556565286,
                "pushdate": 112957,
            },
            "action": {
                "name": "test-action",
                "title": "Test Action",
                "description": "Just testing",
                "taskGroupId": slugid.nice().encode("ascii"),
                "symbol": "t",
                "repo_scope": "assume:repo:hg.mozilla.org/try:action:generic",
                "cb_name": "test_action",
            },
            "input": {},
            "parameters": {},
            "now": current_json_time(),
            "taskId": slugid.nice().encode("ascii"),
            "ownTaskId": slugid.nice().encode("ascii"),
            "clientId": "testing/testing/testing",
        }
        rendered = jsone.render(self.taskcluster_yml, context)
        pprint.pprint(rendered)
        self.assertEqual(
            rendered["tasks"][0]["metadata"]["name"], "Action: Test Action"
        )

    def test_unknown(self):
        context = {"tasks_for": "bitkeeper-push"}
        rendered = jsone.render(self.taskcluster_yml, context)
        pprint.pprint(rendered)
        self.assertEqual(rendered["tasks"], [])


if __name__ == "__main__":
    main()
