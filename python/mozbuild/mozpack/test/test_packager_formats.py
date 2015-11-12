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
import mozpack.path as mozpath


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
    'files': {
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
    },
}

FILES = CONTENTS['files']

RESULT_FLAT = {
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
    'chrome/f/oo/bar/baz': FILES['chrome/f/oo/bar/baz'],
    'chrome/f/oo/baz': FILES['chrome/f/oo/baz'],
    'chrome/f/oo/qux': FILES['chrome/f/oo/qux'],
    'components/components.manifest': [
        'binary-component foo.so',
        'interfaces interfaces.xpt',
    ],
    'components/foo.so': FILES['components/foo.so'],
    'components/interfaces.xpt': {
        'foo': read_interfaces(foo_xpt.open())['foo'],
        'bar': read_interfaces(bar_xpt.open())['bar'],
    },
    'foo': FILES['foo'],
    'app/chrome.manifest': [
        'manifest chrome/chrome.manifest',
        'manifest components/components.manifest',
    ],
    'app/chrome/chrome.manifest': [
        'content content foo/',
    ],
    'app/chrome/foo/foo': FILES['app/chrome/foo/foo'],
    'app/components/components.manifest': [
        'component {foo-id} foo.js',
    ],
    'app/components/foo.js': FILES['app/components/foo.js'],
    'addon0/chrome.manifest': [
        'manifest chrome/chrome.manifest',
        'manifest components/components.manifest',
    ],
    'addon0/chrome/chrome.manifest': [
        'content content foo/bar/',
    ],
    'addon0/chrome/foo/bar/baz': FILES['addon0/chrome/foo/bar/baz'],
    'addon0/components/components.manifest': [
        'interfaces interfaces.xpt',
    ],
    'addon0/components/interfaces.xpt': {
        'foo': read_interfaces(foo2_xpt.open())['foo'],
        'bar': read_interfaces(bar_xpt.open())['bar'],
    },
}

RESULT_JAR = {
    p: RESULT_FLAT[p]
    for p in (
        'chrome.manifest',
        'chrome/chrome.manifest',
        'components/components.manifest',
        'components/foo.so',
        'components/interfaces.xpt',
        'foo',
        'app/chrome.manifest',
        'app/components/components.manifest',
        'app/components/foo.js',
        'addon0/chrome.manifest',
        'addon0/components/components.manifest',
        'addon0/components/interfaces.xpt',
    )
}

RESULT_JAR.update({
    'chrome/f/f.manifest': [
        'content oo jar:oo.jar!/',
        'content bar jar:oo.jar!/bar/',
        'resource foo resource://bar/',
    ],
    'chrome/f/oo.jar': {
        'bar/baz': FILES['chrome/f/oo/bar/baz'],
        'baz': FILES['chrome/f/oo/baz'],
        'qux': FILES['chrome/f/oo/qux'],
    },
    'app/chrome/chrome.manifest': [
        'content content jar:foo.jar!/',
    ],
    'app/chrome/foo.jar': {
        'foo': FILES['app/chrome/foo/foo'],
    },
    'addon0/chrome/chrome.manifest': [
        'content content jar:foo.jar!/bar/',
    ],
    'addon0/chrome/foo.jar': {
        'bar/baz': FILES['addon0/chrome/foo/bar/baz'],
    },
})

RESULT_OMNIJAR = {
    p: RESULT_FLAT[p]
    for p in (
        'components/foo.so',
        'foo',
    )
}

RESULT_OMNIJAR.update({
    p: RESULT_JAR[p]
    for p in RESULT_JAR
    if p.startswith('addon0/')
})

RESULT_OMNIJAR.update({
    'omni.foo': {
        'components/components.manifest': [
            'interfaces interfaces.xpt',
        ],
    },
    'chrome.manifest': [
        'manifest components/components.manifest',
    ],
    'components/components.manifest': [
        'binary-component foo.so',
    ],
    'app/omni.foo': {
        p: RESULT_FLAT['app/' + p]
        for p in (
            'chrome.manifest',
            'chrome/chrome.manifest',
            'chrome/foo/foo',
            'components/components.manifest',
            'components/foo.js',
        )
    },
})

RESULT_OMNIJAR['omni.foo'].update({
    p: RESULT_FLAT[p]
    for p in (
        'chrome.manifest',
        'chrome/chrome.manifest',
        'chrome/f/f.manifest',
        'chrome/f/oo/bar/baz',
        'chrome/f/oo/baz',
        'chrome/f/oo/qux',
        'components/interfaces.xpt',
    )
})

CONTENTS_WITH_BASE = {
    'bases': {
        mozpath.join('base/root', b): a
        for b, a in CONTENTS['bases'].iteritems()
    },
    'manifests': [
        m.move(mozpath.join('base/root', m.base))
        for m in CONTENTS['manifests']
    ],
    'files': {
        mozpath.join('base/root', p): f
        for p, f in CONTENTS['files'].iteritems()
    },
}

# There is no base for the top directory in CONTENTS,
# and we want a specific base here.
CONTENTS_WITH_BASE['bases']['base/root'] = False

EXTRA_CONTENTS = {
    'extra/file': GeneratedFile('extra file'),
}

CONTENTS_WITH_BASE['files'].update(EXTRA_CONTENTS)

def result_with_base(results):
    result = {
        mozpath.join('base/root', p): v
        for p, v in results.iteritems()
    }
    result.update(EXTRA_CONTENTS)
    return result

RESULT_FLAT_WITH_BASE = result_with_base(RESULT_FLAT)
RESULT_JAR_WITH_BASE = result_with_base(RESULT_JAR)
RESULT_OMNIJAR_WITH_BASE = result_with_base(RESULT_OMNIJAR)


class MockDest(MockDest):
    def exists(self):
        return False


def fill_formatter(formatter, contents):
    for base, is_addon in contents['bases'].items():
        formatter.add_base(base, is_addon)

    for manifest in contents['manifests']:
        formatter.add_manifest(manifest)

    for k, v in contents['files'].iteritems():
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
        self.assertEqual(get_contents(registry), RESULT_FLAT)

    def test_jar_formatter(self):
        registry = FileRegistry()
        formatter = JarFormatter(registry)

        fill_formatter(formatter, CONTENTS)
        self.assertEqual(get_contents(registry), RESULT_JAR)

    def test_omnijar_formatter(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, 'omni.foo')

        fill_formatter(formatter, CONTENTS)
        self.assertEqual(get_contents(registry), RESULT_OMNIJAR)

    def test_flat_formatter_with_base(self):
        registry = FileRegistry()
        formatter = FlatFormatter(registry)

        fill_formatter(formatter, CONTENTS_WITH_BASE)
        self.assertEqual(get_contents(registry), RESULT_FLAT_WITH_BASE)

    def test_jar_formatter_with_base(self):
        registry = FileRegistry()
        formatter = JarFormatter(registry)

        fill_formatter(formatter, CONTENTS_WITH_BASE)
        self.assertEqual(get_contents(registry), RESULT_JAR_WITH_BASE)

    def test_omnijar_formatter_with_base(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, 'omni.foo')

        fill_formatter(formatter, CONTENTS_WITH_BASE)
        result = {
            mozpath.join('base/root', p): v
            for p, v in RESULT_OMNIJAR.iteritems()
        }
        self.assertEqual(get_contents(registry), RESULT_OMNIJAR_WITH_BASE)

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
