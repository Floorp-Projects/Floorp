# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest

from compare_locales.paths import ProjectConfig
from . import SetupMixin


class TestConfigRules(SetupMixin, unittest.TestCase):

    def test_filter_empty(self):
        'Test that an empty config works'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/browser/**'
        })
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.file, entity='one_entity')
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file, entity='one_entity')
        self.assertEqual(rv, 'ignore')

    def test_single_file_rule(self):
        'Test a single rule for just a single file, no key'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/browser/**'
        })
        self.cfg.add_rules({
            'path': '/tmp/somedir/{locale}/browser/one/two/file.ftl',
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.file, 'one_entity')
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file, 'one_entity')
        self.assertEqual(rv, 'ignore')

    def test_single_key_rule(self):
        'Test a single rule with file and key'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/browser/**'
        })
        self.cfg.add_rules({
            'path': '/tmp/somedir/{locale}/browser/one/two/file.ftl',
            'key': 'one_entity',
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.file, 'one_entity')
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file, 'one_entity')
        self.assertEqual(rv, 'ignore')

    def test_single_non_matching_key_rule(self):
        'Test a single key rule with regex special chars that should not match'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/**'
        })
        self.cfg.add_rules({
            'path': '/tmp/somedir/{locale}/browser/one/two/file.ftl',
            'key': '.ne_entit.',
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file, 'one_entity')
        self.assertEqual(rv, 'error')

    def test_single_matching_re_key_rule(self):
        'Test a single key with regular expression'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/**'
        })
        self.cfg.add_rules({
            'path': '/tmp/somedir/{locale}/browser/one/two/file.ftl',
            'key': 're:.ne_entit.$',
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file, 'one_entity')
        self.assertEqual(rv, 'ignore')

    def test_double_file_rule(self):
        'Test path shortcut, one for each of our files'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/**'
        })
        self.cfg.add_rules({
            'path': [
                '/tmp/somedir/{locale}/browser/one/two/file.ftl',
                '/tmp/somedir/{locale}/toolkit/two/one/file.ftl',
            ],
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'ignore')

    def test_double_file_key_rule(self):
        'Test path and key shortcut, one key matching, one not'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/**'
        })
        self.cfg.add_rules({
            'path': [
                '/tmp/somedir/{locale}/browser/one/two/file.ftl',
                '/tmp/somedir/{locale}/toolkit/two/one/file.ftl',
            ],
            'key': [
                'one_entity',
                'other_entity',
            ],
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.file, 'one_entity')
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'error')
        rv = self.cfg.filter(self.other_file, 'one_entity')
        self.assertEqual(rv, 'ignore')

    def test_single_wildcard_rule(self):
        'Test single wildcard'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/browser/**'
        })
        self.cfg.add_rules({
            'path': [
                '/tmp/somedir/{locale}/browser/one/*/*',
            ],
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'ignore')

    def test_double_wildcard_rule(self):
        'Test double wildcard'
        self.cfg.add_paths({
            'l10n': '/tmp/somedir/{locale}/**'
        })
        self.cfg.add_rules({
            'path': [
                '/tmp/somedir/{locale}/**',
            ],
            'action': 'ignore'
        })
        rv = self.cfg.filter(self.file)
        self.assertEqual(rv, 'ignore')
        rv = self.cfg.filter(self.other_file)
        self.assertEqual(rv, 'ignore')


class TestProjectConfig(unittest.TestCase):
    def test_children(self):
        pc = ProjectConfig(None)
        child = ProjectConfig(None)
        pc.add_child(child)
        self.assertListEqual([pc, child], list(pc.configs))

    def test_locales_in_children(self):
        pc = ProjectConfig(None)
        child = ProjectConfig(None)
        child.add_paths({
            'l10n': '/tmp/somedir/{locale}/toolkit/**',
        })
        child.set_locales([])
        pc.add_child(child)
        self.assertListEqual(pc.all_locales, [])
        pc.set_locales(['de', 'fr'])
        self.assertListEqual(child.locales, [])
        self.assertListEqual(pc.all_locales, ['de', 'fr'])

    def test_locales_in_paths(self):
        pc = ProjectConfig(None)
        child = ProjectConfig(None)
        child.add_paths({
            'l10n': '/tmp/somedir/{locale}/toolkit/**',
            'locales': ['it']
        })
        child.set_locales([])
        pc.add_child(child)
        self.assertListEqual(pc.all_locales, ['it'])
        pc.set_locales(['de', 'fr'])
        self.assertListEqual(pc.all_locales, ['de', 'fr', 'it'])


class TestSameConfig(unittest.TestCase):

    def test_path(self):
        one = ProjectConfig('one.toml')
        one.set_locales(['ab'])
        self.assertTrue(one.same(ProjectConfig('one.toml')))
        self.assertFalse(one.same(ProjectConfig('two.toml')))

    def test_paths(self):
        one = ProjectConfig('one.toml')
        one.set_locales(['ab'])
        one.add_paths({
            'l10n': '/tmp/somedir/{locale}/**'
        })
        other = ProjectConfig('one.toml')
        self.assertFalse(one.same(other))
        other.add_paths({
            'l10n': '/tmp/somedir/{locale}/**'
        })
        self.assertTrue(one.same(other))

    def test_children(self):
        one = ProjectConfig('one.toml')
        one.add_child(ProjectConfig('inner.toml'))
        one.set_locales(['ab'])
        other = ProjectConfig('one.toml')
        self.assertFalse(one.same(other))
        other.add_child(ProjectConfig('inner.toml'))
        self.assertTrue(one.same(other))
