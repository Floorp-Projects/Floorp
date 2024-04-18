# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import unittest
from pprint import pprint
from unittest.mock import patch

import pytest
from mozunit import MockedOpen, main
from taskgraph import create
from taskgraph.util import taskcluster

from gecko_taskgraph import actions
from gecko_taskgraph.actions.util import combine_task_graph_files, relativize_datestamps
from gecko_taskgraph.decision import read_artifact

TASK_DEF = {
    "created": "2017-10-10T18:33:03.460Z",
    # note that this is not an even number of seconds off!
    "deadline": "2017-10-11T18:33:03.461Z",
    "dependencies": [],
    "expires": "2018-10-10T18:33:04.461Z",
    "payload": {
        "artifacts": {
            "public": {
                "expires": "2018-10-10T18:33:03.463Z",
                "path": "/builds/worker/artifacts",
                "type": "directory",
            },
        },
        "maxRunTime": 1800,
    },
}


@pytest.fixture(scope="module", autouse=True)
def enable_test_mode():
    create.testing = True
    taskcluster.testing = True


class TestRelativize(unittest.TestCase):
    def test_relativize(self):
        rel = relativize_datestamps(TASK_DEF)
        import pprint

        pprint.pprint(rel)
        assert rel["created"] == {"relative-datestamp": "0 seconds"}
        assert rel["deadline"] == {"relative-datestamp": "86400 seconds"}
        assert rel["expires"] == {"relative-datestamp": "31536001 seconds"}
        assert rel["payload"]["artifacts"]["public"]["expires"] == {
            "relative-datestamp": "31536000 seconds"
        }


class TestCombineTaskGraphFiles(unittest.TestCase):
    def test_no_suffixes(self):
        with MockedOpen({}):
            combine_task_graph_files([])
            self.assertRaises(Exception, open("artifacts/to-run.json"))

    @patch("gecko_taskgraph.actions.util.rename_artifact")
    def test_one_suffix(self, rename_artifact):
        combine_task_graph_files(["0"])
        rename_artifact.assert_any_call("task-graph-0.json", "task-graph.json")
        rename_artifact.assert_any_call(
            "label-to-taskid-0.json", "label-to-taskid.json"
        )
        rename_artifact.assert_any_call("to-run-0.json", "to-run.json")

    def test_several_suffixes(self):
        files = {
            "artifacts/task-graph-0.json": json.dumps({"taska": {}}),
            "artifacts/label-to-taskid-0.json": json.dumps({"taska": "TASKA"}),
            "artifacts/to-run-0.json": json.dumps(["taska"]),
            "artifacts/task-graph-1.json": json.dumps({"taskb": {}}),
            "artifacts/label-to-taskid-1.json": json.dumps({"taskb": "TASKB"}),
            "artifacts/to-run-1.json": json.dumps(["taskb"]),
        }
        with MockedOpen(files):
            combine_task_graph_files(["0", "1"])
            self.assertEqual(
                read_artifact("task-graph.json"),
                {
                    "taska": {},
                    "taskb": {},
                },
            )
            self.assertEqual(
                read_artifact("label-to-taskid.json"),
                {
                    "taska": "TASKA",
                    "taskb": "TASKB",
                },
            )
            self.assertEqual(
                sorted(read_artifact("to-run.json")),
                [
                    "taska",
                    "taskb",
                ],
            )


def is_subset(subset, superset):
    if isinstance(subset, dict):
        return all(
            key in superset and is_subset(val, superset[key])
            for key, val in subset.items()
        )

    if isinstance(subset, list) or isinstance(subset, set):
        return all(
            any(is_subset(subitem, superitem) for superitem in superset)
            for subitem in subset
        )

    if isinstance(subset, str):
        return subset in superset

    # assume that subset is a plain value if none of the above match
    return subset == superset


@pytest.mark.parametrize(
    "task_def,expected",
    [
        pytest.param(
            {"tags": {"kind": "decision-task"}},
            {
                "hookPayload": {
                    "decision": {
                        "action": {"cb_name": "retrigger-decision"},
                    },
                },
            },
            id="retrigger_decision",
        ),
        pytest.param(
            {"tags": {"action": "backfill-task"}},
            {
                "hookPayload": {
                    "decision": {
                        "action": {"cb_name": "retrigger-decision"},
                    },
                },
            },
            id="retrigger_backfill",
        ),
    ],
)
def test_extract_applicable_action(
    responses, monkeypatch, actions_json, task_def, expected
):
    base_url = "https://taskcluster"
    decision_task_id = "dddd"
    task_id = "tttt"

    monkeypatch.setenv("TASK_ID", task_id)
    monkeypatch.setenv("TASKCLUSTER_ROOT_URL", base_url)
    monkeypatch.setenv("TASKCLUSTER_PROXY_URL", base_url)
    responses.add(
        responses.GET,
        f"{base_url}/api/queue/v1/task/{task_id}",
        status=200,
        json=task_def,
    )
    action = actions.util._extract_applicable_action(
        actions_json, "retrigger", decision_task_id, task_id
    )
    pprint(action, indent=2)
    assert is_subset(expected, action)


if __name__ == "__main__":
    main()
