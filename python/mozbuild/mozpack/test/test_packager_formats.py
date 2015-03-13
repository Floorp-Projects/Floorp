# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import unittest
from mozpack.packager.formats import (
    FlatFormatter,
    JarFormatter,
    OmniJarFormatter,
)
from mozpack.copier import FileRegistry
from mozpack.files import (
    GeneratedFile,
    ManifestFile,
)
from mozpack.chrome.manifest import (
    ManifestContent,
    ManifestComponent,
    ManifestResource,
    ManifestBinaryComponent,
)
from mozpack.test.test_files import (
    MockDest,
    foo_xpt,
    foo2_xpt,
    bar_xpt,
    read_interfaces,
)


CONTENTS = {
    'bases': {
        # base_path: is_addon?
        'app': False,
        'addon0': True,
    },
    'manifests': [
        ManifestContent('chrome/f', 'oo', 'oo/'),
        ManifestContent('chrome/f', 'bar', 'oo/bar/'),
        ManifestResource('chrome/f', 'foo', 'resource://bar/'),
        ManifestBinaryComponent('components', 'foo.so'),
        ManifestContent('app/chrome', 'content', 'foo/'),
        ManifestComponent('app/components', '{foo-id}', 'foo.js'),
        ManifestContent('addon0/chrome', 'content', 'foo/bar/'),
    ],
    'chrome/f/oo/bar/baz': GeneratedFile('foobarbaz'),
    'chrome/f/oo/baz': GeneratedFile('foobaz'),
    'chrome/f/oo/qux': GeneratedFile('fooqux'),
    'components/foo.so': GeneratedFile('foo.so'),
    'components/foo.xpt': foo_xpt,
    'components/bar.xpt': bar_xpt,
    'foo': GeneratedFile('foo'),
    'app/chrome/foo/foo': GeneratedFile('appfoo'),
    'app/components/foo.js': GeneratedFile('foo.js'),
    'addon0/chrome/foo/bar/baz': GeneratedFile('foobarbaz'),
    'addon0/components/foo.xpt': foo2_xpt,
    'addon0/components/bar.xpt': bar_xpt,
}


class MockDest(MockDest):
    def exists(self):
        return False


def fill_formatter(formatter, contents):
    for base, is_addon in contents['bases'].items():
        formatter.add_base(base, is_addon)

    for manifest in contents['manifests']:
        formatter.add_manifest(manifest)

    for k, v in contents.iteritems():
        if k in ('bases', 'manifests'):
            continue
        if k.endswith('.xpt'):
            formatter.add_interfaces(k, v)
        else:
            formatter.add(k, v)


def get_contents(registry, read_all=False):
    result = {}
    for k, v in registry:
        if k.endswith('.xpt'):
            tmpfile = MockDest()
            registry[k].copy(tmpfile)
            result[k] = read_interfaces(tmpfile)
        elif isinstance(v, FileRegistry):
            result[k] = get_contents(v)
        elif isinstance(v, ManifestFile) or read_all:
            result[k] = v.open().read().splitlines()
        else:
            result[k] = v
    return result


class TestFormatters(unittest.TestCase):
    maxDiff = None

    def test_bases(self):
        formatter = FlatFormatter(FileRegistry())
        formatter.add_base('')
        formatter.add_base('browser')
        formatter.add_base('webapprt')
        formatter.add_base('addon0', addon=True)
        self.assertEqual(formatter._get_base('platform.ini'), '')
        self.assertEqual(formatter._get_base('browser/application.ini'),
                         'browser')
        self.assertEqual(formatter._get_base('webapprt/webapprt.ini'),
                         'webapprt')
        self.assertEqual(formatter._get_base('addon0/install.rdf'),
                         'addon0')

    def test_flat_formatter(self):
        registry = FileRegistry()
        formatter = FlatFormatter(registry)

        fill_formatter(formatter, CONTENTS)

        RESULT = {
            'chrome.manifest': [
                'manifest chrome/chrome.manifest',
                'manifest components/components.manifest',
            ],
            'chrome/chrome.manifest': [
                'manifest f/f.manifest',
            ],
            'chrome/f/f.manifest': [
                'content oo oo/',
                'content bar oo/bar/',
                'resource foo resource://bar/',
            ],
            'chrome/f/oo/bar/baz': CONTENTS['chrome/f/oo/bar/baz'],
            'chrome/f/oo/baz': CONTENTS['chrome/f/oo/baz'],
            'chrome/f/oo/qux': CONTENTS['chrome/f/oo/qux'],
            'components/components.manifest': [
                'binary-component foo.so',
                'interfaces interfaces.xpt',
            ],
            'components/foo.so': CONTENTS['components/foo.so'],
            'components/interfaces.xpt': {
                'foo': read_interfaces(foo_xpt.open())['foo'],
                'bar': read_interfaces(bar_xpt.open())['bar'],
            },
            'foo': CONTENTS['foo'],
            'app/chrome.manifest': [
                'manifest chrome/chrome.manifest',
                'manifest components/components.manifest',
            ],
            'app/chrome/chrome.manifest': [
                'content content foo/',
            ],
            'app/chrome/foo/foo': CONTENTS['app/chrome/foo/foo'],
            'app/components/components.manifest': [
                'component {foo-id} foo.js',
            ],
            'app/components/foo.js': CONTENTS['app/components/foo.js'],
            'addon0/chrome.manifest': [
                'manifest chrome/chrome.manifest',
                'manifest components/components.manifest',
            ],
            'addon0/chrome/chrome.manifest': [
                'content content foo/bar/',
            ],
            'addon0/chrome/foo/bar/baz': CONTENTS['addon0/chrome/foo/bar/baz'],
            'addon0/components/components.manifest': [
                'interfaces interfaces.xpt',
            ],
            'addon0/components/interfaces.xpt': {
                'foo': read_interfaces(foo2_xpt.open())['foo'],
                'bar': read_interfaces(bar_xpt.open())['bar'],
            },
        }

        self.assertEqual(get_contents(registry), RESULT)

    def test_jar_formatter(self):
        registry = FileRegistry()
        formatter = JarFormatter(registry)

        fill_formatter(formatter, CONTENTS)

        RESULT = {
            'chrome.manifest': [
                'manifest chrome/chrome.manifest',
                'manifest components/components.manifest',
            ],
            'chrome/chrome.manifest': [
                'manifest f/f.manifest',
            ],
            'chrome/f/f.manifest': [
                'content oo jar:oo.jar!/',
                'content bar jar:oo.jar!/bar/',
                'resource foo resource://bar/',
            ],
            'chrome/f/oo.jar': {
                'bar/baz': CONTENTS['chrome/f/oo/bar/baz'],
                'baz': CONTENTS['chrome/f/oo/baz'],
                'qux': CONTENTS['chrome/f/oo/qux'],
            },
            'components/components.manifest': [
                'binary-component foo.so',
                'interfaces interfaces.xpt',
            ],
            'components/foo.so': CONTENTS['components/foo.so'],
            'components/interfaces.xpt': {
                'foo': read_interfaces(foo_xpt.open())['foo'],
                'bar': read_interfaces(bar_xpt.open())['bar'],
            },
            'foo': CONTENTS['foo'],
            'app/chrome.manifest': [
                'manifest chrome/chrome.manifest',
                'manifest components/components.manifest',
            ],
            'app/chrome/chrome.manifest': [
                'content content jar:foo.jar!/',
            ],
            'app/chrome/foo.jar': {
                'foo': CONTENTS['app/chrome/foo/foo'],
            },
            'app/components/components.manifest': [
                'component {foo-id} foo.js',
            ],
            'app/components/foo.js': CONTENTS['app/components/foo.js'],
            'addon0/chrome.manifest': [
                'manifest chrome/chrome.manifest',
                'manifest components/components.manifest',
            ],
            'addon0/chrome/chrome.manifest': [
                'content content jar:foo.jar!/bar/',
            ],
            'addon0/chrome/foo.jar': {
                'bar/baz': CONTENTS['addon0/chrome/foo/bar/baz'],
            },
            'addon0/components/components.manifest': [
                'interfaces interfaces.xpt',
            ],
            'addon0/components/interfaces.xpt': {
                'foo': read_interfaces(foo2_xpt.open())['foo'],
                'bar': read_interfaces(bar_xpt.open())['bar'],
            },
        }

        self.assertEqual(get_contents(registry), RESULT)

    def test_omnijar_formatter(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, 'omni.foo')

        fill_formatter(formatter, CONTENTS)

        RESULT = {
            'omni.foo': {
                'chrome.manifest': [
                    'manifest chrome/chrome.manifest',
                    'manifest components/components.manifest',
                ],
                'chrome/chrome.manifest': [
                    'manifest f/f.manifest',
                ],
                'chrome/f/f.manifest': [
                    'content oo oo/',
                    'content bar oo/bar/',
                    'resource foo resource://bar/',
                ],
                'chrome/f/oo/bar/baz': CONTENTS['chrome/f/oo/bar/baz'],
                'chrome/f/oo/baz': CONTENTS['chrome/f/oo/baz'],
                'chrome/f/oo/qux': CONTENTS['chrome/f/oo/qux'],
                'components/components.manifest': [
                    'interfaces interfaces.xpt',
                ],
                'components/interfaces.xpt': {
                    'foo': read_interfaces(foo_xpt.open())['foo'],
                    'bar': read_interfaces(bar_xpt.open())['bar'],
                },
            },
            'chrome.manifest': [
                'manifest components/components.manifest',
            ],
            'components/components.manifest': [
                'binary-component foo.so',
            ],
            'components/foo.so': CONTENTS['components/foo.so'],
            'foo': CONTENTS['foo'],
            'app/omni.foo': {
                'chrome.manifest': [
                    'manifest chrome/chrome.manifest',
                    'manifest components/components.manifest',
                ],
                'chrome/chrome.manifest': [
                    'content content foo/',
                ],
                'chrome/foo/foo': CONTENTS['app/chrome/foo/foo'],
                'components/components.manifest': [
                    'component {foo-id} foo.js',
                ],
                'components/foo.js': CONTENTS['app/components/foo.js'],
            },
            'addon0/chrome.manifest': [
                'manifest chrome/chrome.manifest',
                'manifest components/components.manifest',
            ],
            'addon0/chrome/chrome.manifest': [
                'content content jar:foo.jar!/bar/',
            ],
            'addon0/chrome/foo.jar': {
                'bar/baz': CONTENTS['addon0/chrome/foo/bar/baz'],
            },
            'addon0/components/components.manifest': [
                'interfaces interfaces.xpt',
            ],
            'addon0/components/interfaces.xpt': {
                'foo': read_interfaces(foo2_xpt.open())['foo'],
                'bar': read_interfaces(bar_xpt.open())['bar'],
            },
        }
        self.assertEqual(get_contents(registry), RESULT)

    def test_omnijar_is_resource(self):
        registry = FileRegistry()
        f = OmniJarFormatter(registry, 'omni.foo', non_resources=[
            'defaults/messenger/mailViews.dat',
            'defaults/foo/*',
            '*/dummy',
        ])
        f.add_base('app')
        for base in ['', 'app/']:
            self.assertTrue(f.is_resource(base + 'chrome'))
            self.assertTrue(
                f.is_resource(base + 'chrome/foo/bar/baz.properties'))
            self.assertFalse(f.is_resource(base + 'chrome/icons/foo.png'))
            self.assertTrue(f.is_resource(base + 'components/foo.js'))
            self.assertFalse(f.is_resource(base + 'components/foo.so'))
            self.assertTrue(f.is_resource(base + 'res/foo.css'))
            self.assertFalse(f.is_resource(base + 'res/cursors/foo.png'))
            self.assertFalse(f.is_resource(base + 'res/MainMenu.nib/'))
            self.assertTrue(f.is_resource(base + 'defaults/pref/foo.js'))
            self.assertFalse(
                f.is_resource(base + 'defaults/pref/channel-prefs.js'))
            self.assertTrue(
                f.is_resource(base + 'defaults/preferences/foo.js'))
            self.assertFalse(
                f.is_resource(base + 'defaults/preferences/channel-prefs.js'))
            self.assertTrue(f.is_resource(base + 'modules/foo.jsm'))
            self.assertTrue(f.is_resource(base + 'greprefs.js'))
            self.assertTrue(f.is_resource(base + 'hyphenation/foo'))
            self.assertTrue(f.is_resource(base + 'update.locale'))
            self.assertTrue(
                f.is_resource(base + 'jsloader/resource/gre/modules/foo.jsm'))
            self.assertFalse(f.is_resource(base + 'foo'))
            self.assertFalse(f.is_resource('foo/bar/greprefs.js'))
            self.assertTrue(f.is_resource(base + 'defaults/messenger/foo.dat'))
            self.assertFalse(
                f.is_resource(base + 'defaults/messenger/mailViews.dat'))
            self.assertTrue(f.is_resource(base + 'defaults/pref/foo.js'))
            self.assertFalse(f.is_resource(base + 'defaults/foo/bar.dat'))
            self.assertFalse(f.is_resource(base + 'defaults/foo/bar/baz.dat'))
            self.assertTrue(f.is_resource(base + 'chrome/foo/bar/baz/dummy_'))
            self.assertFalse(f.is_resource(base + 'chrome/foo/bar/baz/dummy'))
            self.assertTrue(f.is_resource(base + 'chrome/foo/bar/dummy_'))
            self.assertFalse(f.is_resource(base + 'chrome/foo/bar/dummy'))


if __name__ == '__main__':
    mozunit.main()
