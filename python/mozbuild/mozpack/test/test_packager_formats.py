# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
from mozpack.packager.formats import (
    FlatFormatter,
    JarFormatter,
    OmniJarFormatter,
)
from mozpack.copier import FileRegistry
from mozpack.files import GeneratedFile
from mozpack.chrome.manifest import (
    ManifestContent,
    ManifestResource,
    ManifestBinaryComponent,
)
from mozpack.test.test_files import (
    TestWithTmpDir,
    foo_xpt,
    bar_xpt,
    read_interfaces,
)


class TestFlatFormatter(TestWithTmpDir):
    def test_flat_formatter(self):
        registry = FileRegistry()
        formatter = FlatFormatter(registry)
        formatter.add_base('app')
        formatter.add('f/oo/bar', GeneratedFile('foobar'))
        formatter.add('f/oo/baz', GeneratedFile('foobaz'))
        formatter.add('f/oo/qux', GeneratedFile('fooqux'))
        formatter.add_manifest(ManifestContent('f/oo', 'bar', 'bar'))
        formatter.add_manifest(ManifestContent('f/oo', 'qux', 'qux'))
        self.assertEqual(registry.paths(),
                         ['f/oo/bar', 'f/oo/baz', 'f/oo/qux',
                          'chrome.manifest', 'f/f.manifest',
                          'f/oo/oo.manifest'])
        self.assertEqual(registry['chrome.manifest'].open().read(),
                         'manifest f/f.manifest\n')
        self.assertEqual(registry['f/f.manifest'].open().read(),
                         'manifest oo/oo.manifest\n')
        self.assertEqual(registry['f/oo/oo.manifest'].open().read(), ''.join([
            'content bar bar\n',
            'content qux qux\n',
        ]))

        formatter.add_interfaces('components/foo.xpt', foo_xpt)
        formatter.add_interfaces('components/bar.xpt', bar_xpt)
        self.assertEqual(registry.paths(),
                         ['f/oo/bar', 'f/oo/baz', 'f/oo/qux',
                          'chrome.manifest', 'f/f.manifest',
                          'f/oo/oo.manifest', 'components/components.manifest',
                          'components/interfaces.xpt'])
        self.assertEqual(registry['chrome.manifest'].open().read(), ''.join([
            'manifest f/f.manifest\n',
            'manifest components/components.manifest\n',
        ]))
        self.assertEqual(
            registry['components/components.manifest'].open().read(),
            'interfaces interfaces.xpt\n'
        )

        registry['components/interfaces.xpt'] \
            .copy(self.tmppath('interfaces.xpt'))
        linked = read_interfaces(self.tmppath('interfaces.xpt'))
        foo = read_interfaces(foo_xpt.open())
        bar = read_interfaces(bar_xpt.open())
        self.assertEqual(foo['foo'], linked['foo'])
        self.assertEqual(bar['bar'], linked['bar'])

        formatter.add_manifest(ManifestContent('app/chrome', 'content',
                                               'foo/'))
        self.assertEqual(registry['chrome.manifest'].open().read(), ''.join([
            'manifest f/f.manifest\n',
            'manifest components/components.manifest\n',
        ]))
        self.assertEqual(registry['app/chrome.manifest'].open().read(),
                         'manifest chrome/chrome.manifest\n')
        self.assertEqual(registry['app/chrome/chrome.manifest'].open().read(),
                         'content content foo/\n')

    def test_bases(self):
        formatter = FlatFormatter(FileRegistry())
        formatter.add_base('')
        formatter.add_base('browser')
        formatter.add_base('webapprt')
        self.assertEqual(formatter._get_base('platform.ini'), '')
        self.assertEqual(formatter._get_base('browser/application.ini'),
                         'browser')
        self.assertEqual(formatter._get_base('webapprt/webapprt.ini'),
                         'webapprt')


class TestJarFormatter(TestWithTmpDir):
    def test_jar_formatter(self):
        registry = FileRegistry()
        formatter = JarFormatter(registry)
        formatter.add_manifest(ManifestContent('f', 'oo', 'oo/'))
        formatter.add_manifest(ManifestContent('f', 'bar', 'oo/bar/'))
        formatter.add('f/oo/bar/baz', GeneratedFile('foobarbaz'))
        formatter.add('f/oo/qux', GeneratedFile('fooqux'))

        self.assertEqual(registry.paths(),
                         ['chrome.manifest', 'f/f.manifest', 'f/oo.jar'])
        self.assertEqual(registry['chrome.manifest'].open().read(),
                         'manifest f/f.manifest\n')
        self.assertEqual(registry['f/f.manifest'].open().read(), ''.join([
            'content oo jar:oo.jar!/\n',
            'content bar jar:oo.jar!/bar/\n',
        ]))
        self.assertTrue(formatter.contains('f/oo/bar/baz'))
        self.assertFalse(formatter.contains('foo/bar/baz'))
        self.assertEqual(registry['f/oo.jar'].paths(), ['bar/baz', 'qux'])

        formatter.add_manifest(ManifestResource('f', 'foo', 'resource://bar/'))
        self.assertEqual(registry['f/f.manifest'].open().read(), ''.join([
            'content oo jar:oo.jar!/\n',
            'content bar jar:oo.jar!/bar/\n',
            'resource foo resource://bar/\n',
        ]))


class TestOmniJarFormatter(TestWithTmpDir):
    def test_omnijar_formatter(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, 'omni.foo')
        formatter.add_base('app')
        formatter.add('chrome/f/oo/bar', GeneratedFile('foobar'))
        formatter.add('chrome/f/oo/baz', GeneratedFile('foobaz'))
        formatter.add('chrome/f/oo/qux', GeneratedFile('fooqux'))
        formatter.add_manifest(ManifestContent('chrome/f/oo', 'bar', 'bar'))
        formatter.add_manifest(ManifestContent('chrome/f/oo', 'qux', 'qux'))
        self.assertEqual(registry.paths(), ['omni.foo'])
        self.assertEqual(registry['omni.foo'].paths(), [
            'chrome/f/oo/bar',
            'chrome/f/oo/baz',
            'chrome/f/oo/qux',
            'chrome.manifest',
            'chrome/chrome.manifest',
            'chrome/f/f.manifest',
            'chrome/f/oo/oo.manifest',
        ])
        self.assertEqual(registry['omni.foo']['chrome.manifest']
                         .open().read(), 'manifest chrome/chrome.manifest\n')
        self.assertEqual(registry['omni.foo']['chrome/chrome.manifest']
                         .open().read(), 'manifest f/f.manifest\n')
        self.assertEqual(registry['omni.foo']['chrome/f/f.manifest']
                         .open().read(), 'manifest oo/oo.manifest\n')
        self.assertEqual(registry['omni.foo']['chrome/f/oo/oo.manifest']
                         .open().read(), ''.join([
                             'content bar bar\n',
                             'content qux qux\n',
                         ]))
        self.assertTrue(formatter.contains('chrome/f/oo/bar'))
        self.assertFalse(formatter.contains('chrome/foo/bar'))

        formatter.add_interfaces('components/foo.xpt', foo_xpt)
        formatter.add_interfaces('components/bar.xpt', bar_xpt)
        self.assertEqual(registry['omni.foo'].paths(), [
            'chrome/f/oo/bar',
            'chrome/f/oo/baz',
            'chrome/f/oo/qux',
            'chrome.manifest',
            'chrome/chrome.manifest',
            'chrome/f/f.manifest',
            'chrome/f/oo/oo.manifest',
            'components/components.manifest',
            'components/interfaces.xpt',
        ])
        self.assertEqual(registry['omni.foo']['chrome.manifest']
                         .open().read(), ''.join([
                             'manifest chrome/chrome.manifest\n',
                             'manifest components/components.manifest\n'
                         ]))
        self.assertEqual(registry['omni.foo']
                         ['components/components.manifest'].open().read(),
                         'interfaces interfaces.xpt\n')

        registry['omni.foo'][
            'components/interfaces.xpt'].copy(self.tmppath('interfaces.xpt'))
        linked = read_interfaces(self.tmppath('interfaces.xpt'))
        foo = read_interfaces(foo_xpt.open())
        bar = read_interfaces(bar_xpt.open())
        self.assertEqual(foo['foo'], linked['foo'])
        self.assertEqual(bar['bar'], linked['bar'])

        formatter.add('app/chrome/foo/baz', GeneratedFile('foobaz'))
        formatter.add_manifest(ManifestContent('app/chrome', 'content',
                                               'foo/'))
        self.assertEqual(registry.paths(), ['omni.foo', 'app/omni.foo'])
        self.assertEqual(registry['app/omni.foo'].paths(), [
            'chrome/foo/baz',
            'chrome.manifest',
            'chrome/chrome.manifest',
        ])
        self.assertEqual(registry['app/omni.foo']['chrome.manifest']
                         .open().read(), 'manifest chrome/chrome.manifest\n')
        self.assertEqual(registry['app/omni.foo']['chrome/chrome.manifest']
                         .open().read(), 'content content foo/\n')

        formatter.add_manifest(ManifestBinaryComponent('components', 'foo.so'))
        formatter.add('components/foo.so', GeneratedFile('foo'))
        self.assertEqual(registry.paths(), [
            'omni.foo', 'app/omni.foo', 'chrome.manifest',
            'components/components.manifest', 'components/foo.so',
        ])
        self.assertEqual(registry['chrome.manifest'].open().read(),
                         'manifest components/components.manifest\n')
        self.assertEqual(registry['components/components.manifest']
                         .open().read(), 'binary-component foo.so\n')

        formatter.add_manifest(ManifestBinaryComponent('app/components',
                                                       'foo.so'))
        formatter.add('app/components/foo.so', GeneratedFile('foo'))
        self.assertEqual(registry.paths(), [
            'omni.foo', 'app/omni.foo', 'chrome.manifest',
            'components/components.manifest', 'components/foo.so',
            'app/chrome.manifest', 'app/components/components.manifest',
            'app/components/foo.so',
        ])
        self.assertEqual(registry['app/chrome.manifest'].open().read(),
                         'manifest components/components.manifest\n')
        self.assertEqual(registry['app/components/components.manifest']
                         .open().read(), 'binary-component foo.so\n')

        formatter.add('app/foo', GeneratedFile('foo'))
        self.assertEqual(registry.paths(), [
            'omni.foo', 'app/omni.foo', 'chrome.manifest',
            'components/components.manifest', 'components/foo.so',
            'app/chrome.manifest', 'app/components/components.manifest',
            'app/components/foo.so', 'app/foo'
        ])

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
