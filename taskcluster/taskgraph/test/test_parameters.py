# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from taskgraph.parameters import (
    Parameters,
    ParameterMismatch,
    load_parameters_file,
    PARAMETERS,
    COMM_PARAMETERS,
)
from mozunit import main, MockedOpen


class TestParameters(unittest.TestCase):

    vals = {n: n for n in PARAMETERS.keys()}

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
        self.assertRaises(ParameterMismatch, lambda: p.check())

        p = Parameters(strict=False)
        p.check()  # should not raise

    def test_Parameters_check_extra(self):
        p = Parameters(xyz=10, **self.vals)
        self.assertRaises(ParameterMismatch, lambda: p.check())

        p = Parameters(strict=False, xyz=10, **self.vals)
        p.check()  # should not raise

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

    def test_load_parameters_override(self):
        """
        When ``load_parameters_file`` is passed overrides, they are included in
        the generated parameters.
        """
        self.assertEqual(
            load_parameters_file('', overrides={'some': 'data'}),
            {'some': 'data'})

    def test_load_parameters_override_file(self):
        """
        When ``load_parameters_file`` is passed overrides, they overwrite data
        loaded from a file.
        """
        with MockedOpen({"params.json": '{"some": "data"}'}):
            self.assertEqual(
                load_parameters_file('params.json', overrides={'some': 'other'}),
                {'some': 'other'})


class TestCommParameters(unittest.TestCase):
    vals = {n: n for n in PARAMETERS.keys() + COMM_PARAMETERS.keys()}

    def test_Parameters_check(self):
        """
        Specifying all of the gecko and comm parameters doesn't result in an error.
        """
        p = Parameters(**self.vals)
        p.check()  # should not raise

    def test_Parameters_check_missing(self):
        """
        If any of the comm parameters are specified, all of them must be specified.
        """
        vals = self.vals.copy()
        del vals[next(iter(COMM_PARAMETERS.keys()))]
        p = Parameters(**vals)
        self.assertRaises(Exception, p.check)

    def test_Parameters_check_extra(self):
        """
        If parameters other than the global and comm parameters are specified,
        an error is reported.
        """
        p = Parameters(extra="data", **self.vals)
        self.assertRaises(Exception, p.check)


if __name__ == '__main__':
    main()
