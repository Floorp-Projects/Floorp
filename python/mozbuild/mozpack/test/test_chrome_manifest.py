# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import mozunit
import os
from mozpack.chrome.manifest import (
    ManifestContent,
    ManifestLocale,
    ManifestSkin,
    Manifest,
    ManifestResource,
    ManifestOverride,
    ManifestComponent,
    ManifestContract,
    ManifestInterfaces,
    ManifestBinaryComponent,
    ManifestCategory,
    ManifestStyle,
    ManifestOverlay,
    MANIFESTS_TYPES,
    parse_manifest,
    parse_manifest_line,
)
from mozpack.errors import errors, AccumulatedErrors
from test_errors import TestErrors


class TestManifest(unittest.TestCase):
    def test_parse_manifest(self):
        manifest = [
            'content global content/global/',
            'content global content/global/ application=foo application=bar' +
            ' platform',
            'locale global en-US content/en-US/',
            'locale global en-US content/en-US/ application=foo',
            'skin global classic/1.0 content/skin/classic/',
            'skin global classic/1.0 content/skin/classic/ application=foo' +
            ' os=WINNT',
            '',
            'manifest pdfjs/chrome.manifest',
            'resource gre-resources toolkit/res/',
            'override chrome://global/locale/netError.dtd' +
            ' chrome://browser/locale/netError.dtd',
            '# Comment',
            'component {b2bba4df-057d-41ea-b6b1-94a10a8ede68} foo.js',
            'contract @mozilla.org/foo;1' +
            ' {b2bba4df-057d-41ea-b6b1-94a10a8ede68}',
            'interfaces foo.xpt',
            'binary-component bar.so',
            'category command-line-handler m-browser' +
            ' @mozilla.org/browser/clh;1' +
            ' application={ec8030f7-c20a-464f-9b0e-13a3a9e97384}',
            'style chrome://global/content/customizeToolbar.xul' +
            ' chrome://browser/skin/',
            'overlay chrome://global/content/viewSource.xul' +
            ' chrome://browser/content/viewSourceOverlay.xul',
        ]
        other_manifest = [
            'content global content/global/'
        ]
        expected_result = [
            ManifestContent('', 'global', 'content/global/'),
            ManifestContent('', 'global', 'content/global/', 'application=foo',
                            'application=bar', 'platform'),
            ManifestLocale('', 'global', 'en-US', 'content/en-US/'),
            ManifestLocale('', 'global', 'en-US', 'content/en-US/',
                           'application=foo'),
            ManifestSkin('', 'global', 'classic/1.0', 'content/skin/classic/'),
            ManifestSkin('', 'global', 'classic/1.0', 'content/skin/classic/',
                         'application=foo', 'os=WINNT'),
            Manifest('', 'pdfjs/chrome.manifest'),
            ManifestResource('', 'gre-resources', 'toolkit/res/'),
            ManifestOverride('', 'chrome://global/locale/netError.dtd',
                             'chrome://browser/locale/netError.dtd'),
            ManifestComponent('', '{b2bba4df-057d-41ea-b6b1-94a10a8ede68}',
                              'foo.js'),
            ManifestContract('', '@mozilla.org/foo;1',
                             '{b2bba4df-057d-41ea-b6b1-94a10a8ede68}'),
            ManifestInterfaces('', 'foo.xpt'),
            ManifestBinaryComponent('', 'bar.so'),
            ManifestCategory('', 'command-line-handler', 'm-browser',
                             '@mozilla.org/browser/clh;1', 'application=' +
                             '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}'),
            ManifestStyle('', 'chrome://global/content/customizeToolbar.xul',
                          'chrome://browser/skin/'),
            ManifestOverlay('', 'chrome://global/content/viewSource.xul',
                            'chrome://browser/content/viewSourceOverlay.xul'),
        ]
        with mozunit.MockedOpen({'manifest': '\n'.join(manifest),
                                 'other/manifest': '\n'.join(other_manifest)}):
            # Ensure we have tests for all types of manifests.
            self.assertEqual(set(type(e) for e in expected_result),
                             set(MANIFESTS_TYPES.values()))
            self.assertEqual(list(parse_manifest(os.curdir, 'manifest')),
                             expected_result)
            self.assertEqual(list(parse_manifest(os.curdir, 'other/manifest')),
                             [ManifestContent('other', 'global',
                                              'content/global/')])

    def test_manifest_rebase(self):
        m = parse_manifest_line('chrome', 'content global content/global/')
        m = m.rebase('')
        self.assertEqual(str(m), 'content global chrome/content/global/')
        m = m.rebase('chrome')
        self.assertEqual(str(m), 'content global content/global/')

        m = parse_manifest_line('chrome/foo', 'content global content/global/')
        m = m.rebase('chrome')
        self.assertEqual(str(m), 'content global foo/content/global/')
        m = m.rebase('chrome/foo')
        self.assertEqual(str(m), 'content global content/global/')

        m = parse_manifest_line('modules/foo', 'resource foo ./')
        m = m.rebase('modules')
        self.assertEqual(str(m), 'resource foo foo/')
        m = m.rebase('modules/foo')
        self.assertEqual(str(m), 'resource foo ./')

        m = parse_manifest_line('chrome', 'content browser browser/content/')
        m = m.rebase('chrome/browser').move('jar:browser.jar!').rebase('')
        self.assertEqual(str(m), 'content browser jar:browser.jar!/content/')


class TestManifestErrors(TestErrors, unittest.TestCase):
    def test_parse_manifest_errors(self):
        manifest = [
            'skin global classic/1.0 content/skin/classic/ platform',
            '',
            'binary-component bar.so',
            'unsupported foo',
        ]
        with mozunit.MockedOpen({'manifest': '\n'.join(manifest)}):
            with self.assertRaises(AccumulatedErrors):
                with errors.accumulate():
                    list(parse_manifest(os.curdir, 'manifest'))
            out = self.get_output()
            # Expecting 2 errors
            self.assertEqual(len(out), 2)
            path = os.path.abspath('manifest')
            # First on line 1
            self.assertTrue(out[0].startswith('Error: %s:1: ' % path))
            # Second on line 4
            self.assertTrue(out[1].startswith('Error: %s:4: ' % path))


if __name__ == '__main__':
    mozunit.main()
