# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from taskgraph.parameters import Parameters, load_parameters_file, PARAMETER_NAMES
from mozunit import main, MockedOpen


class TestParameters(unittest.TestCase):

    vals = {n: n for n in PARAMETER_NAMES}

    def test_Parameters_immutable(self):
        p = Parameters(**self.vals)

        def assign():
            p['head_ref'] = 20
        self.assertRaises(Exception, assign)

    def test_Parameters_missing_KeyError(self):
        p = Parameters(**self.vals)
        self.assertRaises(KeyError, lambda: p['z'])

    def test_Parameters_invalid_KeyError(self):
        """even if the value is present, if it's not a valid property, raise KeyError"""
        p = Parameters(xyz=10, **self.vals)
        self.assertRaises(KeyError, lambda: p['xyz'])

    def test_Parameters_get(self):
        p = Parameters(head_ref=10, level=20)
        self.assertEqual(p['head_ref'], 10)

    def test_Parameters_check(self):
        p = Parameters(**self.vals)
        p.check()  # should not raise

    def test_Parameters_check_missing(self):
        p = Parameters()
        self.assertRaises(Exception, lambda: p.check())

    def test_Parameters_check_extra(self):
        p = Parameters(xyz=10, **self.vals)
        self.assertRaises(Exception, lambda: p.check())

    def test_load_parameters_file_yaml(self):
        with MockedOpen({"params.yml": "some: data\n"}):
            self.assertEqual(
                    load_parameters_file('params.yml'),
                    {'some': 'data'})

    def test_load_parameters_file_json(self):
        with MockedOpen({"params.json": '{"some": "data"}'}):
            self.assertEqual(
                    load_parameters_file('params.json'),
                    {'some': 'data'})


if __name__ == '__main__':
    main()
