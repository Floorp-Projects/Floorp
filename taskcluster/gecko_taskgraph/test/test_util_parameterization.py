# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import unittest
import datetime
from unittest import mock
import os

from mozunit import main
from gecko_taskgraph.util.parameterization import (
    resolve_timestamps,
    resolve_task_references,
)


class TestTimestamps(unittest.TestCase):
    def test_no_change(self):
        now = datetime.datetime(2018, 1, 1)
        input = {
            "key": "value",
            "numeric": 10,
            "list": ["a", True, False, None],
        }
        self.assertEqual(resolve_timestamps(now, input), input)

    def test_buried_replacement(self):
        now = datetime.datetime(2018, 1, 1)
        input = {"key": [{"key2": [{"relative-datestamp": "1 day"}]}]}
        self.assertEqual(
            resolve_timestamps(now, input),
            {"key": [{"key2": ["2018-01-02T00:00:00.000Z"]}]},
        )

    def test_appears_with_other_keys(self):
        now = datetime.datetime(2018, 1, 1)
        input = [{"relative-datestamp": "1 day", "another-key": True}]
        self.assertEqual(
            resolve_timestamps(now, input),
            [{"relative-datestamp": "1 day", "another-key": True}],
        )


class TestTaskRefs(unittest.TestCase):
    def do(self, input, output):
        taskid_for_edge_name = {"edge%d" % n: "tid%d" % n for n in range(1, 4)}
        self.assertEqual(
            resolve_task_references(
                "subject",
                input,
                "tid-self",
                "tid-decision",
                taskid_for_edge_name,
            ),
            output,
        )

    def test_no_change(self):
        "resolve_task_references does nothing when there are no task references"
        self.do(
            {"in-a-list": ["stuff", {"property": "<edge1>"}]},
            {"in-a-list": ["stuff", {"property": "<edge1>"}]},
        )

    def test_in_list(self):
        "resolve_task_references resolves task references in a list"
        self.do(
            {"in-a-list": ["stuff", {"task-reference": "<edge1>"}]},
            {"in-a-list": ["stuff", "tid1"]},
        )

    def test_in_dict(self):
        "resolve_task_references resolves task references in a dict"
        self.do(
            {"in-a-dict": {"stuff": {"task-reference": "<edge2>"}}},
            {"in-a-dict": {"stuff": "tid2"}},
        )

    def test_multiple(self):
        "resolve_task_references resolves multiple references in the same string"
        self.do(
            {"multiple": {"task-reference": "stuff <edge1> stuff <edge2> after"}},
            {"multiple": "stuff tid1 stuff tid2 after"},
        )

    def test_embedded(self):
        "resolve_task_references resolves ebmedded references"
        self.do(
            {"embedded": {"task-reference": "stuff before <edge3> stuff after"}},
            {"embedded": "stuff before tid3 stuff after"},
        )

    def test_escaping(self):
        "resolve_task_references resolves escapes in task references"
        self.do({"escape": {"task-reference": "<<><edge3>>"}}, {"escape": "<tid3>"})

    def test_multikey(self):
        "resolve_task_references is ignored when there is another key in the dict"
        self.do(
            {"escape": {"task-reference": "<edge3>", "another-key": True}},
            {"escape": {"task-reference": "<edge3>", "another-key": True}},
        )

    def test_self(self):
        "resolve_task_references resolves `self` to the provided task id"
        self.do({"escape": {"task-reference": "<self>"}}, {"escape": "tid-self"})

    def test_decision(self):
        "resolve_task_references resolves `decision` to the provided decision task id"
        self.do(
            {"escape": {"task-reference": "<decision>"}}, {"escape": "tid-decision"}
        )

    def test_invalid(self):
        "resolve_task_references raises a KeyError on reference to an invalid task"
        self.assertRaisesRegexp(
            KeyError,
            "task 'subject' has no dependency named 'no-such'",
            lambda: resolve_task_references(
                "subject",
                {"task-reference": "<no-such>"},
                "tid-self",
                "tid-decision",
                {},
            ),
        )


class TestArtifactRefs(unittest.TestCase):
    def do(self, input, output):
        taskid_for_edge_name = {"edge%d" % n: "tid%d" % n for n in range(1, 4)}
        with mock.patch.dict(
            os.environ, {"TASKCLUSTER_ROOT_URL": "https://tc-tests.localhost"}
        ):
            self.assertEqual(
                resolve_task_references(
                    "subject", input, "tid-self", "tid-decision", taskid_for_edge_name
                ),
                output,
            )

    def test_in_list(self):
        "resolve_task_references resolves artifact references in a list"
        self.do(
            {"in-a-list": ["stuff", {"artifact-reference": "<edge1/public/foo/bar>"}]},
            {
                "in-a-list": [
                    "stuff",
                    "https://tc-tests.localhost/api/queue/v1"
                    "/task/tid1/artifacts/public/foo/bar",
                ]
            },
        )

    def test_in_dict(self):
        "resolve_task_references resolves artifact references in a dict"
        self.do(
            {"in-a-dict": {"stuff": {"artifact-reference": "<edge2/public/bar/foo>"}}},
            {
                "in-a-dict": {
                    "stuff": "https://tc-tests.localhost/api/queue/v1"
                    "/task/tid2/artifacts/public/bar/foo"
                }
            },
        )

    def test_in_string(self):
        "resolve_task_references resolves artifact references embedded in a string"
        self.do(
            {
                "stuff": {
                    "artifact-reference": "<edge1/public/filename> and <edge2/public/bar>"
                }
            },
            {
                "stuff": "https://tc-tests.localhost/api/queue/v1"
                "/task/tid1/artifacts/public/filename and "
                "https://tc-tests.localhost/api/queue/v1/task/tid2/artifacts/public/bar"
            },
        )

    def test_self(self):
        "resolve_task_references raises KeyError on artifact references to `self`"
        self.assertRaisesRegexp(
            KeyError,
            "task 'subject' can't reference artifacts of self",
            lambda: resolve_task_references(
                "subject",
                {"artifact-reference": "<self/public/artifact>"},
                "tid-self",
                "tid-decision",
                {},
            ),
        )

    def test_decision(self):
        "resolve_task_references resolves `decision` to the provided decision task id"
        self.do(
            {"stuff": {"artifact-reference": "<decision/public/artifact>"}},
            {
                "stuff": "https://tc-tests.localhost/api/queue/v1/task/tid-decision/"
                "artifacts/public/artifact"
            },
        )

    def test_invalid(self):
        "resolve_task_references raises a KeyError on reference to an invalid task"
        self.assertRaisesRegexp(
            KeyError,
            "task 'subject' has no dependency named 'no-such'",
            lambda: resolve_task_references(
                "subject",
                {"artifact-reference": "<no-such/public/artifact>"},
                "tid-self",
                "tid-decision",
                {},
            ),
        )

    def test_badly_formed(self):
        "resolve_task_references ignores badly-formatted artifact references"
        for inv in ["<edge1>", "edge1/foo>", "<edge1>/foo", "<edge1>foo"]:
            resolved = resolve_task_references(
                "subject", {"artifact-reference": inv}, "tid-self", "tid-decision", {}
            )
            self.assertEqual(resolved, inv)


if __name__ == "__main__":
    main()
