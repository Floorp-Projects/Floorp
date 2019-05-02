# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import unittest

from compare_locales.lint import util
from compare_locales.paths.project import ProjectConfig
from compare_locales.paths.files import ProjectFiles
from compare_locales import mozpath


class MirrorReferenceTest(unittest.TestCase):
    def test_empty(self):
        files = ProjectFiles(None, [])
        get_reference_and_tests = util.mirror_reference_and_tests(files, 'tld')
        ref, tests = get_reference_and_tests('some/path/file.ftl')
        self.assertIsNone(ref)
        self.assertIsNone(tests)

    def test_no_tests(self):
        pc = ProjectConfig(None)
        pc.add_paths({
            'reference': 'some/path/file.ftl',
            'l10n': 'some/{locale}/file.ftl',
        })
        files = ProjectFiles(None, [pc])
        get_reference_and_tests = util.mirror_reference_and_tests(files, 'tld')
        ref, tests = get_reference_and_tests('some/path/file.ftl')
        self.assertEqual(mozpath.relpath(ref, 'tld'), 'some/path/file.ftl')
        self.assertEqual(tests, set())

    def test_with_tests(self):
        pc = ProjectConfig(None)
        pc.add_paths({
            'reference': 'some/path/file.ftl',
            'l10n': 'some/{locale}/file.ftl',
            'test': ['more_stuff'],
        })
        files = ProjectFiles(None, [pc])
        get_reference_and_tests = util.mirror_reference_and_tests(files, 'tld')
        ref, tests = get_reference_and_tests('some/path/file.ftl')
        self.assertEqual(mozpath.relpath(ref, 'tld'), 'some/path/file.ftl')
        self.assertEqual(tests, {'more_stuff'})


class L10nBaseReferenceTest(unittest.TestCase):
    def test_empty(self):
        files = ProjectFiles(None, [])
        get_reference_and_tests = util.l10n_base_reference_and_tests(files)
        ref, tests = get_reference_and_tests('some/path/file.ftl')
        self.assertIsNone(ref)
        self.assertIsNone(tests)

    def test_no_tests(self):
        pc = ProjectConfig(None)
        pc.add_environment(l10n_base='l10n_orig')
        pc.add_paths({
            'reference': 'some/path/file.ftl',
            'l10n': '{l10n_base}/{locale}/some/file.ftl',
        })
        pc.set_locales(['gecko'], deep=True)
        files = ProjectFiles('gecko', [pc])
        get_reference_and_tests = util.l10n_base_reference_and_tests(files)
        ref, tests = get_reference_and_tests('some/path/file.ftl')
        self.assertEqual(
            mozpath.relpath(ref, 'l10n_orig/gecko'),
            'some/file.ftl'
        )
        self.assertEqual(tests, set())

    def test_with_tests(self):
        pc = ProjectConfig(None)
        pc.add_environment(l10n_base='l10n_orig')
        pc.add_paths({
            'reference': 'some/path/file.ftl',
            'l10n': '{l10n_base}/{locale}/some/file.ftl',
            'test': ['more_stuff'],
        })
        pc.set_locales(['gecko'], deep=True)
        files = ProjectFiles('gecko', [pc])
        get_reference_and_tests = util.l10n_base_reference_and_tests(files)
        ref, tests = get_reference_and_tests('some/path/file.ftl')
        self.assertEqual(
            mozpath.relpath(ref, 'l10n_orig/gecko'),
            'some/file.ftl'
        )
        self.assertEqual(tests, {'more_stuff'})
