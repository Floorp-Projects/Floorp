#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
import taskcluster_graph.build_task as build_task

class TestBuildTask(unittest.TestCase):

    def test_validate_missing_extra(self):
        with self.assertRaises(build_task.BuildTaskValidationException):
            build_task.validate({})

    def test_validate_valid(self):
        with self.assertRaises(build_task.BuildTaskValidationException):
            build_task.validate({
                'extra': {
                    'locations': {
                        'build': '',
                        'tests': ''
                    }
                }
            })

if __name__ == '__main__':
    mozunit.main()

