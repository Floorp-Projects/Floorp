# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
from test_packager import MockFinder
from mozpack.packager import l10n
from mozpack.files import (
    GeneratedFile,
    ManifestFile,
)
from mozpack.chrome.manifest import (
    Manifest,
    ManifestLocale,
    ManifestContent,
)
from mozpack.copier import FileRegistry
from mozpack.packager.formats import FlatFormatter


class TestL10NRepack(unittest.TestCase):
    def test_l10n_repack(self):
        foo = GeneratedFile('foo')
        foobar = GeneratedFile('foobar')
        qux = GeneratedFile('qux')
        bar = GeneratedFile('bar')
        baz = GeneratedFile('baz')
        dict_aa = GeneratedFile('dict_aa')
        dict_bb = GeneratedFile('dict_bb')
        dict_cc = GeneratedFile('dict_cc')
        barbaz = GeneratedFile('barbaz')
        lst = GeneratedFile('foo\nbar')
        app_finder = MockFinder({
            'bar/foo': foo,
            'chrome/foo/foobar': foobar,
            'chrome/qux/qux.properties': qux,
            'chrome/qux/baz/baz.properties': baz,
            'chrome/chrome.manifest': ManifestFile('chrome', [
                ManifestContent('chrome', 'foo', 'foo/'),
                ManifestLocale('chrome', 'qux', 'en-US', 'qux/'),
            ]),
            'chrome.manifest':
            ManifestFile('', [Manifest('', 'chrome/chrome.manifest')]),
            'dict/aa': dict_aa,
            'app/chrome/bar/barbaz.dtd': barbaz,
            'app/chrome/chrome.manifest': ManifestFile('app/chrome', [
                ManifestLocale('app/chrome', 'bar', 'en-US', 'bar/')
            ]),
            'app/chrome.manifest':
            ManifestFile('app', [Manifest('app', 'chrome/chrome.manifest')]),
            'app/dict/bb': dict_bb,
            'app/dict/cc': dict_cc,
            'app/chrome/bar/search/foo.xml': foo,
            'app/chrome/bar/search/bar.xml': bar,
            'app/chrome/bar/search/lst.txt': lst,
        })
        app_finder.jarlogs = {}
        app_finder.base = 'app'
        foo_l10n = GeneratedFile('foo_l10n')
        qux_l10n = GeneratedFile('qux_l10n')
        baz_l10n = GeneratedFile('baz_l10n')
        barbaz_l10n = GeneratedFile('barbaz_l10n')
        lst_l10n = GeneratedFile('foo\nqux')
        l10n_finder = MockFinder({
            'chrome/qux-l10n/qux.properties': qux_l10n,
            'chrome/qux-l10n/baz/baz.properties': baz_l10n,
            'chrome/chrome.manifest': ManifestFile(' chrome', [
                ManifestLocale('chrome', 'qux', 'x-test', 'qux-l10n/'),
            ]),
            'chrome.manifest':
            ManifestFile('', [Manifest('', 'chrome/chrome.manifest')]),
            'dict/bb': dict_bb,
            'dict/cc': dict_cc,
            'app/chrome/bar-l10n/barbaz.dtd': barbaz_l10n,
            'app/chrome/chrome.manifest': ManifestFile('app/chrome', [
                ManifestLocale('app/chrome', 'bar', 'x-test', 'bar-l10n/')
            ]),
            'app/chrome.manifest':
            ManifestFile('app', [Manifest('app', 'chrome/chrome.manifest')]),
            'app/dict/aa': dict_aa,
            'app/chrome/bar-l10n/search/foo.xml': foo_l10n,
            'app/chrome/bar-l10n/search/qux.xml': qux_l10n,
            'app/chrome/bar-l10n/search/lst.txt': lst_l10n,
        })
        l10n_finder.base = 'l10n'
        copier = FileRegistry()
        formatter = FlatFormatter(copier)

        l10n._repack(app_finder, l10n_finder, copier, formatter,
                     ['dict', 'chrome/**/search/*.xml'])
        self.maxDiff = None

        repacked = {
            'bar/foo': foo,
            'chrome/foo/foobar': foobar,
            'chrome/qux-l10n/qux.properties': qux_l10n,
            'chrome/qux-l10n/baz/baz.properties': baz_l10n,
            'chrome/chrome.manifest': ManifestFile('chrome', [
                ManifestContent('chrome', 'foo', 'foo/'),
                ManifestLocale('chrome', 'qux', 'x-test', 'qux-l10n/'),
            ]),
            'chrome.manifest':
            ManifestFile('', [Manifest('', 'chrome/chrome.manifest')]),
            'dict/bb': dict_bb,
            'dict/cc': dict_cc,
            'app/chrome/bar-l10n/barbaz.dtd': barbaz_l10n,
            'app/chrome/chrome.manifest': ManifestFile('app/chrome', [
                ManifestLocale('app/chrome', 'bar', 'x-test', 'bar-l10n/')
            ]),
            'app/chrome.manifest':
            ManifestFile('app', [Manifest('app', 'chrome/chrome.manifest')]),
            'app/dict/aa': dict_aa,
            'app/chrome/bar-l10n/search/foo.xml': foo_l10n,
            'app/chrome/bar-l10n/search/qux.xml': qux_l10n,
            'app/chrome/bar-l10n/search/lst.txt': lst_l10n,
        }

        self.assertEqual(
            dict((p, f.open().read()) for p, f in copier),
            dict((p, f.open().read()) for p, f in repacked.iteritems())
        )


if __name__ == '__main__':
    mozunit.main()
