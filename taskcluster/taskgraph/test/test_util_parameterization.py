# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import datetime

from mozunit import main
from taskgraph.util.parameterization import (
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
        input = {"key": [{"key2": [{'relative-datestamp': '1 day'}]}]}
        self.assertEqual(resolve_timestamps(now, input),
                         {"key": [{"key2": ['2018-01-02T00:00:00Z']}]})

    def test_appears_with_other_keys(self):
        now = datetime.datetime(2018, 1, 1)
        input = [{'relative-datestamp': '1 day', 'another-key': True}]
        self.assertEqual(resolve_timestamps(now, input),
                         [{'relative-datestamp': '1 day', 'another-key': True}])


class TestTaskRefs(unittest.TestCase):

    def do(self, input, output):
        taskid_for_edge_name = {'edge%d' % n: 'tid%d' % n for n in range(1, 4)}
        self.assertEqual(resolve_task_references('subject', input, taskid_for_edge_name), output)

    def test_no_change(self):
        "resolve_task_references does nothing when there are no task references"
        self.do({'in-a-list': ['stuff', {'property': '<edge1>'}]},
                {'in-a-list': ['stuff', {'property': '<edge1>'}]})

    def test_in_list(self):
        "resolve_task_references resolves task references in a list"
        self.do({'in-a-list': ['stuff', {'task-reference': '<edge1>'}]},
                {'in-a-list': ['stuff', 'tid1']})

    def test_in_dict(self):
        "resolve_task_references resolves task references in a dict"
        self.do({'in-a-dict': {'stuff': {'task-reference': '<edge2>'}}},
                {'in-a-dict': {'stuff': 'tid2'}})

    def test_multiple(self):
        "resolve_task_references resolves multiple references in the same string"
        self.do({'multiple': {'task-reference': 'stuff <edge1> stuff <edge2> after'}},
                {'multiple': 'stuff tid1 stuff tid2 after'})

    def test_embedded(self):
        "resolve_task_references resolves ebmedded references"
        self.do({'embedded': {'task-reference': 'stuff before <edge3> stuff after'}},
                {'embedded': 'stuff before tid3 stuff after'})

    def test_escaping(self):
        "resolve_task_references resolves escapes in task references"
        self.do({'escape': {'task-reference': '<<><edge3>>'}},
                {'escape': '<tid3>'})

    def test_multikey(self):
        "resolve_task_references is ignored when there is another key in the dict"
        self.do({'escape': {'task-reference': '<edge3>', 'another-key': True}},
                {'escape': {'task-reference': '<edge3>', 'another-key': True}})

    def test_invalid(self):
        "resolve_task_references raises a KeyError on reference to an invalid task"
        self.assertRaisesRegexp(
            KeyError,
            "task 'subject' has no dependency named 'no-such'",
            lambda: resolve_task_references('subject', {'task-reference': '<no-such>'}, {})
        )


if __name__ == '__main__':
    main()
