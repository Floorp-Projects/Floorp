# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest

from compare_locales.paths import (
    ProjectConfig
)
from . import (
    MockProjectFiles,
    MockTOMLParser,
    Rooted,
)


class TestProjectPaths(Rooted, unittest.TestCase):
    def test_l10n_path(self):
        cfg = ProjectConfig(None)
        cfg.add_environment(l10n_base=self.root)
        cfg.set_locales(['de'])
        cfg.add_paths({
            'l10n': '{l10n_base}/{locale}/*'
        })
        mocks = [
            self.path(leaf)
            for leaf in (
                '/de/good.ftl',
                '/de/not/subdir/bad.ftl',
                '/fr/good.ftl',
                '/fr/not/subdir/bad.ftl',
            )
        ]
        files = MockProjectFiles(mocks, 'de', [cfg])
        self.assertListEqual(
            list(files),
            [
                (self.path('/de/good.ftl'), None, None, set())
            ]
        )
        self.assertTupleEqual(
            files.match(self.path('/de/good.ftl')),
            (self.path('/de/good.ftl'), None, None, set())
        )
        self.assertIsNone(files.match(self.path('/fr/something.ftl')))
        files = MockProjectFiles(mocks, 'de', [cfg], mergebase='merging')
        self.assertListEqual(
            list(files),
            [
                (self.path('/de/good.ftl'), None, 'merging/de/good.ftl', set())
            ]
        )
        self.assertTupleEqual(
            files.match(self.path('/de/something.ftl')),
            (self.path('/de/something.ftl'),
             None,
             'merging/de/something.ftl',
             set()))
        # 'fr' is not in the locale list, should return no files
        files = MockProjectFiles(mocks, 'fr', [cfg])
        self.assertListEqual(list(files), [])

    def test_single_reference_path(self):
        cfg = ProjectConfig(None)
        cfg.add_environment(l10n_base=self.path('/l10n'))
        cfg.set_locales(['de'])
        cfg.add_paths({
            'l10n': '{l10n_base}/{locale}/good.ftl',
            'reference': self.path('/reference/good.ftl')
        })
        mocks = [
            self.path('/reference/good.ftl'),
            self.path('/reference/not/subdir/bad.ftl'),
        ]
        files = MockProjectFiles(mocks, 'de', [cfg])
        self.assertListEqual(
            list(files),
            [
                (self.path('/l10n/de/good.ftl'),
                 self.path('/reference/good.ftl'),
                 None,
                 set()),
            ])
        self.assertTupleEqual(
            files.match(self.path('/reference/good.ftl')),
            (self.path('/l10n/de/good.ftl'),
             self.path('/reference/good.ftl'),
             None,
             set()),
            )
        self.assertTupleEqual(
            files.match(self.path('/l10n/de/good.ftl')),
            (self.path('/l10n/de/good.ftl'),
             self.path('/reference/good.ftl'),
             None,
             set()),
            )

    def test_reference_path(self):
        cfg = ProjectConfig(None)
        cfg.add_environment(l10n_base=self.path('/l10n'))
        cfg.set_locales(['de'])
        cfg.add_paths({
            'l10n': '{l10n_base}/{locale}/*',
            'reference': self.path('/reference/*')
        })
        mocks = [
            self.path(leaf)
            for leaf in [
                '/l10n/de/good.ftl',
                '/l10n/de/not/subdir/bad.ftl',
                '/l10n/fr/good.ftl',
                '/l10n/fr/not/subdir/bad.ftl',
                '/reference/ref.ftl',
                '/reference/not/subdir/bad.ftl',
            ]
        ]
        files = MockProjectFiles(mocks, 'de', [cfg])
        self.assertListEqual(
            list(files),
            [
                (self.path('/l10n/de/good.ftl'),
                 self.path('/reference/good.ftl'),
                 None,
                 set()),
                (self.path('/l10n/de/ref.ftl'),
                 self.path('/reference/ref.ftl'),
                 None,
                 set()),
            ])
        self.assertTupleEqual(
            files.match(self.path('/l10n/de/good.ftl')),
            (self.path('/l10n/de/good.ftl'),
             self.path('/reference/good.ftl'),
             None,
             set()),
            )
        self.assertTupleEqual(
            files.match(self.path('/reference/good.ftl')),
            (self.path('/l10n/de/good.ftl'),
             self.path('/reference/good.ftl'),
             None,
             set()),
            )
        self.assertIsNone(files.match(self.path('/l10n/de/subdir/bad.ftl')))
        self.assertIsNone(files.match(self.path('/reference/subdir/bad.ftl')))
        files = MockProjectFiles(mocks, 'de', [cfg], mergebase='merging')
        self.assertListEqual(
            list(files),
            [
                (self.path('/l10n/de/good.ftl'),
                 self.path('/reference/good.ftl'),
                 'merging/de/good.ftl', set()),
                (self.path('/l10n/de/ref.ftl'),
                 self.path('/reference/ref.ftl'),
                 'merging/de/ref.ftl', set()),
            ])
        self.assertTupleEqual(
            files.match(self.path('/l10n/de/good.ftl')),
            (self.path('/l10n/de/good.ftl'),
             self.path('/reference/good.ftl'),
             'merging/de/good.ftl', set()),
            )
        self.assertTupleEqual(
            files.match(self.path('/reference/good.ftl')),
            (self.path('/l10n/de/good.ftl'),
             self.path('/reference/good.ftl'),
             'merging/de/good.ftl', set()),
            )
        # 'fr' is not in the locale list, should return no files
        files = MockProjectFiles(mocks, 'fr', [cfg])
        self.assertListEqual(list(files), [])

    def test_partial_l10n(self):
        cfg = ProjectConfig(None)
        cfg.set_locales(['de', 'fr'])
        cfg.add_paths({
            'l10n': self.path('/{locale}/major/*')
        }, {
            'l10n': self.path('/{locale}/minor/*'),
            'locales': ['de']
        })
        mocks = [
            self.path(leaf)
            for leaf in [
                '/de/major/good.ftl',
                '/de/major/not/subdir/bad.ftl',
                '/de/minor/good.ftl',
                '/fr/major/good.ftl',
                '/fr/major/not/subdir/bad.ftl',
                '/fr/minor/good.ftl',
            ]
        ]
        files = MockProjectFiles(mocks, 'de', [cfg])
        self.assertListEqual(
            list(files),
            [
                (self.path('/de/major/good.ftl'), None, None, set()),
                (self.path('/de/minor/good.ftl'), None, None, set()),
            ])
        self.assertTupleEqual(
            files.match(self.path('/de/major/some.ftl')),
            (self.path('/de/major/some.ftl'), None, None, set()))
        self.assertIsNone(files.match(self.path('/de/other/some.ftl')))
        # 'fr' is not in the locale list of minor, should only return major
        files = MockProjectFiles(mocks, 'fr', [cfg])
        self.assertListEqual(
            list(files),
            [
                (self.path('/fr/major/good.ftl'), None, None, set()),
            ])
        self.assertIsNone(files.match(self.path('/fr/minor/some.ftl')))

    def test_validation_mode(self):
        cfg = ProjectConfig(None)
        cfg.add_environment(l10n_base=self.path('/l10n'))
        cfg.set_locales(['de'])
        cfg.add_paths({
            'l10n': '{l10n_base}/{locale}/*',
            'reference': self.path('/reference/*')
        })
        mocks = [
            self.path(leaf)
            for leaf in [
                '/l10n/de/good.ftl',
                '/l10n/de/not/subdir/bad.ftl',
                '/l10n/fr/good.ftl',
                '/l10n/fr/not/subdir/bad.ftl',
                '/reference/ref.ftl',
                '/reference/not/subdir/bad.ftl',
            ]
        ]
        # `None` switches on validation mode
        files = MockProjectFiles(mocks, None, [cfg])
        self.assertListEqual(
            list(files),
            [
                (self.path('/reference/ref.ftl'),
                 self.path('/reference/ref.ftl'),
                 None,
                 set()),
            ])


class TestL10nMerge(Rooted, unittest.TestCase):
    # need to go through TOMLParser, as that's handling most of the
    # environment
    def test_merge_paths(self):
        parser = MockTOMLParser({
            "base.toml":
            '''\
basepath = "."
locales = [
    "de",
]
[env]
    l = "{l10n_base}/{locale}/"
[[paths]]
    reference = "reference/*"
    l10n = "{l}*"
'''})
        cfg = parser.parse(
            self.path('/base.toml'),
            env={'l10n_base': self.path('/l10n')}
        )
        mocks = [
            self.path(leaf)
            for leaf in [
                '/l10n/de/good.ftl',
                '/l10n/de/not/subdir/bad.ftl',
                '/l10n/fr/good.ftl',
                '/l10n/fr/not/subdir/bad.ftl',
                '/reference/ref.ftl',
                '/reference/not/subdir/bad.ftl',
            ]
        ]
        files = MockProjectFiles(mocks, 'de', [cfg], self.path('/mergers'))
        self.assertListEqual(
            list(files),
            [
                (self.path('/l10n/de/good.ftl'),
                 self.path('/reference/good.ftl'),
                 self.path('/mergers/de/good.ftl'),
                 set()),
                (self.path('/l10n/de/ref.ftl'),
                 self.path('/reference/ref.ftl'),
                 self.path('/mergers/de/ref.ftl'),
                 set()),
            ])
