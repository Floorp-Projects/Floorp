# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest

from . import (
    SetupMixin,
)


class TestConfigLegacy(SetupMixin, unittest.TestCase):

    def test_filter_py_true(self):
        'Test filter.py just return bool(True)'
        def filter(mod, path, entity=None):
            return True
        self.cfg.set_filter_py(filter)
        with self.assertRaises(AssertionError):
            self.cfg.add_rules({})
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.file, entity='one_entity')
        self.assertEqual(rv, 'error')

    def test_filter_py_false(self):
        'Test filter.py just return bool(False)'
        def filter(mod, path, entity=None):
            return False
        self.cfg.set_filter_py(filter)
        with self.assertRaises(AssertionError):
            self.cfg.add_rules({})
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.file, entity='one_entity')
        self.assertEqual(rv, 'ignore')

    def test_filter_py_error(self):
        'Test filter.py just return str("error")'
        def filter(mod, path, entity=None):
            return 'error'
        self.cfg.set_filter_py(filter)
        with self.assertRaises(AssertionError):
            self.cfg.add_rules({})
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.file, entity='one_entity')
        self.assertEqual(rv, 'error')

    def test_filter_py_ignore(self):
        'Test filter.py just return str("ignore")'
        def filter(mod, path, entity=None):
            return 'ignore'
        self.cfg.set_filter_py(filter)
        with self.assertRaises(AssertionError):
            self.cfg.add_rules({})
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.file, entity='one_entity')
        self.assertEqual(rv, 'ignore')

    def test_filter_py_report(self):
        'Test filter.py just return str("report") and match to "warning"'
        def filter(mod, path, entity=None):
            return 'report'
        self.cfg.set_filter_py(filter)
        with self.assertRaises(AssertionError):
            self.cfg.add_rules({})
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'warning')
        rv = self.cfg.filter(self.file, entity='one_entity')
        self.assertEqual(rv, 'warning')

    def test_filter_py_module(self):
        'Test filter.py to return str("error") for browser or "ignore"'
        def filter(mod, path, entity=None):
            return 'error' if mod == 'browser' else 'ignore'
        self.cfg.set_filter_py(filter)
        with self.assertRaises(AssertionError):
            self.cfg.add_rules({})
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.file, entity='one_entity')
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file, entity='one_entity')
        self.assertEqual(rv, 'ignore')
