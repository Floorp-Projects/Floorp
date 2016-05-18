# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..kind.legacy import LegacyKind, TASKID_PLACEHOLDER
from ..types import Task
from mozunit import main


class TestLegacyKind(unittest.TestCase):
    # NOTE: much of LegacyKind is copy-pasted from the old legacy code, which
    # is emphatically *not* designed for testing, so this test class does not
    # attempt to test the entire class.

    def setUp(self):
        self.kind = LegacyKind('/root', {})

    def test_get_task_definition_artifact_sub(self):
        "get_task_definition correctly substiatutes artifact URLs"
        task_def = {
            'input_file': TASKID_PLACEHOLDER.format("G5BoWlCBTqOIhn3K3HyvWg"),
            'embedded': 'TASK={} FETCH=lazy'.format(
                TASKID_PLACEHOLDER.format('G5BoWlCBTqOIhn3K3HyvWg')),
        }
        task = Task(self.kind, 'label', task=task_def)
        dep_taskids = {TASKID_PLACEHOLDER.format('G5BoWlCBTqOIhn3K3HyvWg'): 'parent-taskid'}
        task_def = self.kind.get_task_definition(task, dep_taskids)
        self.assertEqual(task_def, {
            'input_file': 'parent-taskid',
            'embedded': 'TASK=parent-taskid FETCH=lazy',
        })


if __name__ == '__main__':
    main()
