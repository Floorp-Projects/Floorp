# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest
import mock

from compare_locales.paths import (
    File,
    ProjectConfig,
    ProjectFiles,
)
from . import (
    MockOS,
    MockProjectFiles,
    MockTOMLParser,
    Rooted,
)


class TestMockOS(Rooted, unittest.TestCase):
    def setUp(self):
        self.node = MockOS('jazz', [
            'one/bit',
            'two/deep/in/directories/with/file1',
            'two/deep/in/directories/with/file2',
            'three/feet',
        ])

    def test_isfile(self):
        self.assertTrue(self.node.isfile('jazz/one/bit'))
        self.assertFalse(self.node.isfile('jazz/one'))
        self.assertFalse(self.node.isfile('foo'))

    def test_walk(self):
        self.assertListEqual(
            list(self.node.walk()),
            [
                ('jazz', ['one', 'three', 'two'], []),
                ('jazz/one', [], ['bit']),
                ('jazz/three', [], ['feet']),
                ('jazz/two', ['deep'], []),
                ('jazz/two/deep', ['in'], []),
                ('jazz/two/deep/in', ['directories'], []),
                ('jazz/two/deep/in/directories', ['with'], []),
                ('jazz/two/deep/in/directories/with', [], [
                    'file1',
                    'file2',
                ]),
            ]
        )

    def test_find(self):
        self.assertIsNone(self.node.find('foo'))
        self.assertIsNone(self.node.find('jazz/one/bit'))
        self.assertIsNone(self.node.find('jazz/one/bit/too/much'))
        self.assertIsNotNone(self.node.find('jazz/one'))
        self.assertListEqual(list(self.node.find('jazz/one').walk()), [
                ('jazz/one', [], ['bit']),
        ])
        self.assertEqual(self.node.find('jazz'), self.node)


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


@mock.patch('os.path.isfile')
@mock.patch('os.walk')
class TestExcludes(Rooted, unittest.TestCase):
    def _list(self, locale, _walk, _isfile):
        parser = MockTOMLParser({
            "pontoon.toml":
            '''\
basepath = "."

[[includes]]
    path = "configs-pontoon.toml"

[[excludes]]
    path = "configs-vendor.toml"
[[excludes]]
    path = "configs-special-templates.toml"
''',
            "vendor.toml":
            '''\
basepath = "."

[[includes]]
    path = "configs-vendor.toml"

[[excludes]]
    path = "configs-special-templates.toml"
''',
            "configs-pontoon.toml":
            '''\
basepath = "."

locales = [
    "de",
    "gd",
    "it",
]

[[paths]]
    reference = "en/**/*.ftl"
    l10n = "{locale}/**/*.ftl"
''',
            "configs-vendor.toml":
            '''\
basepath = "."

locales = [
    "de",
    "it",
]

[[paths]]
    reference = "en/firefox/*.ftl"
    l10n = "{locale}/firefox/*.ftl"
''',
            "configs-special-templates.toml":
            '''\
basepath = "."

[[paths]]
    reference = "en/firefox/home.ftl"
    l10n = "{locale}/firefox/home.ftl"
    locales = [
        "de",
        "fr",
    ]
[[paths]]
    reference = "en/firefox/pagina.ftl"
    l10n = "{locale}/firefox/pagina.ftl"
    locales = [
        "gd",
    ]
''',
            })
        pontoon = parser.parse(self.path('/pontoon.toml'))
        vendor = parser.parse(self.path('/vendor.toml'))
        pc = ProjectFiles(locale, [pontoon, vendor])
        mock_files = [
            '{}/{}/{}'.format(locale, dir, f)
            for locale in ('de', 'en', 'gd', 'it')
            for dir, files in (
                ('firefox', ('home.ftl', 'feature.ftl')),
                ('mozorg', ('mission.ftl',)),
            )
            for f in files
        ]
        os_ = MockOS(self.root, mock_files)
        _isfile.side_effect = os_.isfile
        _walk.side_effect = os_.walk
        local_files = [self.leaf(p).split('/', 1)[1] for p, _, _, _ in pc]
        return pontoon, vendor, local_files

    def test_reference(self, _walk, _isfile):
        pontoon_config, vendor_config, files = self._list(None, _walk, _isfile)
        pontoon_files = ProjectFiles(None, [pontoon_config])
        vendor_files = ProjectFiles(None, [vendor_config])
        self.assertListEqual(
            files,
            [
                'firefox/feature.ftl',
                'firefox/home.ftl',
                'mozorg/mission.ftl',
            ]
        )
        ref_path = self.path('/en/firefox/feature.ftl')
        self.assertIsNotNone(pontoon_files.match(ref_path))
        self.assertIsNotNone(vendor_files.match(ref_path))
        ref_path = self.path('/en/firefox/home.ftl')
        self.assertIsNotNone(pontoon_files.match(ref_path))
        self.assertIsNotNone(vendor_files.match(ref_path))
        ref_path = self.path('/en/mozorg/mission.ftl')
        self.assertIsNotNone(pontoon_files.match(ref_path))
        self.assertIsNone(vendor_files.match(ref_path))

    def test_de(self, _walk, _isfile):
        # home.ftl excluded completely by configs-special-templates.toml
        # firefox/* only in vendor
        pontoon_config, vendor_config, files = self._list('de', _walk, _isfile)
        pontoon_files = ProjectFiles('de', [pontoon_config])
        vendor_files = ProjectFiles('de', [vendor_config])
        self.assertListEqual(
            files,
            [
                'firefox/feature.ftl',
                # 'firefox/home.ftl',
                'mozorg/mission.ftl',
            ]
        )
        l10n_path = self.path('/de/firefox/feature.ftl')
        ref_path = self.path('/en/firefox/feature.ftl')
        self.assertEqual(
            pontoon_config.filter(
                File(
                    l10n_path,
                    'de/firefox/feature.ftl',
                    locale='de'
                )
            ),
            'ignore'
        )
        self.assertIsNone(pontoon_files.match(l10n_path))
        self.assertIsNone(pontoon_files.match(ref_path))
        self.assertIsNotNone(vendor_files.match(l10n_path))
        self.assertIsNotNone(vendor_files.match(ref_path))
        l10n_path = self.path('/de/firefox/home.ftl')
        ref_path = self.path('/en/firefox/home.ftl')
        self.assertEqual(
            pontoon_config.filter(
                File(
                    l10n_path,
                    'de/firefox/home.ftl',
                    locale='de'
                )
            ),
            'ignore'
        )
        self.assertIsNone(pontoon_files.match(l10n_path))
        self.assertIsNone(pontoon_files.match(ref_path))
        self.assertIsNone(vendor_files.match(l10n_path))
        self.assertIsNone(vendor_files.match(ref_path))
        l10n_path = self.path('/de/mozorg/mission.ftl')
        ref_path = self.path('/en/mozorg/mission.ftl')
        self.assertEqual(
            pontoon_config.filter(
                File(
                    l10n_path,
                    'de/mozorg/mission.ftl',
                    locale='de'
                )
            ),
            'error'
        )
        self.assertIsNotNone(pontoon_files.match(l10n_path))
        self.assertIsNotNone(pontoon_files.match(ref_path))
        self.assertIsNone(vendor_files.match(l10n_path))
        self.assertIsNone(vendor_files.match(ref_path))

    def test_gd(self, _walk, _isfile):
        # only community localization
        pontoon_config, vendor_config, files = self._list('gd', _walk, _isfile)
        pontoon_files = ProjectFiles('gd', [pontoon_config])
        vendor_files = ProjectFiles('gd', [vendor_config])
        self.assertListEqual(
            files,
            [
                'firefox/feature.ftl',
                'firefox/home.ftl',
                'mozorg/mission.ftl',
            ]
        )
        l10n_path = self.path('/gd/firefox/home.ftl')
        ref_path = self.path('/en/firefox/home.ftl')
        self.assertEqual(
            pontoon_config.filter(
                File(
                    l10n_path,
                    'gd/firefox/home.ftl',
                    locale='gd'
                )
            ),
            'error'
        )
        self.assertIsNotNone(pontoon_files.match(l10n_path))
        self.assertIsNotNone(pontoon_files.match(ref_path))
        self.assertIsNone(vendor_files.match(l10n_path))
        self.assertIsNone(vendor_files.match(ref_path))

    def test_it(self, _walk, _isfile):
        # all pages translated, but split between vendor and community
        pontoon_config, vendor_config, files = self._list('it', _walk, _isfile)
        pontoon_files = ProjectFiles('it', [pontoon_config])
        vendor_files = ProjectFiles('it', [vendor_config])
        self.assertListEqual(
            files,
            [
                'firefox/feature.ftl',
                'firefox/home.ftl',
                'mozorg/mission.ftl',
            ]
        )
        l10n_path = self.path('/it/firefox/home.ftl')
        ref_path = self.path('/en/firefox/home.ftl')
        file = File(
            l10n_path,
            'it/firefox/home.ftl',
            locale='it'
        )
        self.assertEqual(pontoon_config.filter(file), 'ignore')
        self.assertEqual(vendor_config.filter(file), 'error')
        self.assertIsNone(pontoon_files.match(l10n_path))
        self.assertIsNone(pontoon_files.match(ref_path))
        self.assertIsNotNone(vendor_files.match(l10n_path))
        self.assertIsNotNone(vendor_files.match(ref_path))


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
