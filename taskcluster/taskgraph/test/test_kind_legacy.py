# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..kind.legacy import (
    LegacyKind,
    TASKID_PLACEHOLDER,
    validate_build_task,
    BuildTaskValidationException
)
from ..types import Task
from mozunit import main


class TestLegacyKind(unittest.TestCase):
    # NOTE: much of LegacyKind is copy-pasted from the old legacy code, which
    # is emphatically *not* designed for testing, so this test class does not
    # attempt to test the entire class.

    def setUp(self):
        self.kind = LegacyKind('/root', {})


class TestValidateBuildTask(unittest.TestCase):

    def test_validate_missing_extra(self):
        with self.assertRaises(BuildTaskValidationException):
            validate_build_task({})

    def test_validate_valid(self):
        with self.assertRaises(BuildTaskValidationException):
            validate_build_task({
                'extra': {
                    'locations': {
                        'build': '',
                        'tests': ''
                    }
                }
            })


if __name__ == '__main__':
    main()
