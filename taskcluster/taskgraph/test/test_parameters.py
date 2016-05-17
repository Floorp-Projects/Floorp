# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from ..parameters import Parameters, load_parameters_file
from mozunit import main, MockedOpen

class TestParameters(unittest.TestCase):

    def test_Parameters_immutable(self):
        p = Parameters(x=10, y=20)
        def assign():
            p['x'] = 20
        self.assertRaises(Exception, assign)

    def test_Parameters_KeyError(self):
        p = Parameters(x=10, y=20)
        self.assertRaises(KeyError, lambda: p['z'])

    def test_Parameters_get(self):
        p = Parameters(x=10, y=20)
        self.assertEqual(p['x'], 10)

    def test_load_parameters_file_yaml(self):
        with MockedOpen({"params.yml": "some: data\n"}):
            self.assertEqual(
                    load_parameters_file({'parameters': 'params.yml'}),
                    {'some': 'data'})

    def test_load_parameters_file_json(self):
        with MockedOpen({"params.json": '{"some": "data"}'}):
            self.assertEqual(
                    load_parameters_file({'parameters': 'params.json'}),
                    {'some': 'data'})

if __name__ == '__main__':
    main()
