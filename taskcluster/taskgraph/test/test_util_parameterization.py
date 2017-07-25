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
        now = datetime.datetime(2018, 01, 01)
        input = {
            "key": "value",
            "numeric": 10,
            "list": ["a", True, False, None],
        }
        self.assertEqual(resolve_timestamps(now, input), input)

    def test_buried_replacement(self):
        now = datetime.datetime(2018, 01, 01)
        input = {"key": [{"key2": [{'relative-datestamp': '1 day'}]}]}
        self.assertEqual(resolve_timestamps(now, input),
                         {"key": [{"key2": ['2018-01-02T00:00:00Z']}]})

    def test_appears_with_other_keys(self):
        now = datetime.datetime(2018, 01, 01)
        input = [{'relative-datestamp': '1 day', 'another-key': True}]
        self.assertEqual(resolve_timestamps(now, input),
                         [{'relative-datestamp': '1 day', 'another-key': True}])


class TestTaskRefs(unittest.TestCase):

    def test_no_change(self):
        input = {"key": "value", "numeric": 10, "list": ["a", True, False, None]}
        self.assertEqual(resolve_task_references('lable', input, {}), input)

    def test_buried_replacement(self):
        input = {"key": [{"key2": [{'task-reference': 'taskid=<toolchain>'}]}]}
        self.assertEqual(resolve_task_references('lable', input, {'toolchain': 'abcd'}),
                         {u'key': [{u'key2': [u'taskid=abcd']}]})

    def test_appears_with_other_keys(self):
        input = [{'task-reference': '<toolchain>', 'another-key': True}]
        self.assertEqual(resolve_task_references('lable', input, {'toolchain': 'abcd'}),
                         [{'task-reference': '<toolchain>', 'another-key': True}])

    def test_multiple_subs(self):
        input = [{'task-reference': 'toolchain=<toolchain>, build=<build>'}]
        self.assertEqual(
            resolve_task_references('lable', input, {'toolchain': 'abcd', 'build': 'def'}),
            ['toolchain=abcd, build=def'])

    def test_escaped(self):
        input = [{'task-reference': '<<><toolchain>>'}]
        self.assertEqual(resolve_task_references('lable', input, {'toolchain': 'abcd'}),
                         ['<abcd>'])


if __name__ == '__main__':
    main()
