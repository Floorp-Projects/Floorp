# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import six
import unittest

from compare_locales.paths.matcher import Matcher, ANDROID_STANDARD_MAP
from . import Rooted


class TestMatcher(unittest.TestCase):

    def test_matcher(self):
        one = Matcher('foo/*')
        self.assertTrue(one.match('foo/baz'))
        self.assertFalse(one.match('foo/baz/qux'))
        other = Matcher('bar/*')
        self.assertTrue(other.match('bar/baz'))
        self.assertFalse(other.match('bar/baz/qux'))
        self.assertEqual(one.sub(other, 'foo/baz'), 'bar/baz')
        self.assertIsNone(one.sub(other, 'bar/baz'))
        one = Matcher('foo/**')
        self.assertTrue(one.match('foo/baz'))
        self.assertTrue(one.match('foo/baz/qux'))
        other = Matcher('bar/**')
        self.assertTrue(other.match('bar/baz'))
        self.assertTrue(other.match('bar/baz/qux'))
        self.assertEqual(one.sub(other, 'foo/baz'), 'bar/baz')
        self.assertEqual(one.sub(other, 'foo/baz/qux'), 'bar/baz/qux')
        one = Matcher('foo/*/one/**')
        self.assertTrue(one.match('foo/baz/one/qux'))
        self.assertFalse(one.match('foo/baz/bez/one/qux'))
        other = Matcher('bar/*/other/**')
        self.assertTrue(other.match('bar/baz/other/qux'))
        self.assertFalse(other.match('bar/baz/bez/other/qux'))
        self.assertEqual(one.sub(other, 'foo/baz/one/qux'),
                         'bar/baz/other/qux')
        self.assertEqual(one.sub(other, 'foo/baz/one/qux/zzz'),
                         'bar/baz/other/qux/zzz')
        self.assertIsNone(one.sub(other, 'foo/baz/bez/one/qux'))
        one = Matcher('foo/**/bar/**')
        self.assertTrue(one.match('foo/bar/baz.qux'))
        self.assertTrue(one.match('foo/tender/bar/baz.qux'))
        self.assertFalse(one.match('foo/nobar/baz.qux'))
        self.assertFalse(one.match('foo/tender/bar'))

    def test_prefix(self):
        self.assertEqual(
            Matcher('foo/bar.file').prefix, 'foo/bar.file'
        )
        self.assertEqual(
            Matcher('foo/*').prefix, 'foo/'
        )
        self.assertEqual(
            Matcher('foo/**').prefix, 'foo/'
        )
        self.assertEqual(
            Matcher('foo/*/bar').prefix, 'foo/'
        )
        self.assertEqual(
            Matcher('foo/**/bar').prefix, 'foo/'
        )
        self.assertEqual(
            Matcher('foo/**/bar/*').prefix, 'foo/'
        )
        self.assertEqual(
            Matcher('foo/{v}/bar').prefix,
            'foo/'
        )
        self.assertEqual(
            Matcher('foo/{v}/bar', {'v': 'expanded'}).prefix,
            'foo/expanded/bar'
        )
        self.assertEqual(
            Matcher('foo/{v}/*/bar').prefix,
            'foo/'
        )
        self.assertEqual(
            Matcher('foo/{v}/*/bar', {'v': 'expanded'}).prefix,
            'foo/expanded/'
        )
        self.assertEqual(
            Matcher('foo/{v}/*/bar', {'v': '{missing}'}).prefix,
            'foo/'
        )

    def test_variables(self):
        self.assertDictEqual(
            Matcher('foo/bar.file').match('foo/bar.file'),
            {}
        )
        self.assertDictEqual(
            Matcher('{path}/bar.file').match('foo/bar.file'),
            {
                'path': 'foo'
            }
        )
        self.assertDictEqual(
            Matcher('{ path }/bar.file').match('foo/bar.file'),
            {
                'path': 'foo'
            }
        )
        self.assertIsNone(
            Matcher('{ var }/foopy/{ var }/bears')
            .match('one/foopy/other/bears')
        )
        self.assertDictEqual(
            Matcher('{ var }/foopy/{ var }/bears')
            .match('same_value/foopy/same_value/bears'),
            {
                'var': 'same_value'
            }
        )
        self.assertIsNone(
            Matcher('{ var }/foopy/bears', {'var': 'other'})
            .match('one/foopy/bears')
        )
        self.assertDictEqual(
            Matcher('{ var }/foopy/bears', {'var': 'one'})
            .match('one/foopy/bears'),
            {
                'var': 'one'
            }
        )
        self.assertDictEqual(
            Matcher('{one}/{two}/something', {
                'one': 'some/segment',
                'two': 'with/a/lot/of'
            }).match('some/segment/with/a/lot/of/something'),
            {
                'one': 'some/segment',
                'two': 'with/a/lot/of'
            }
        )
        self.assertDictEqual(
            Matcher('{l}**', {
                'l': 'foo/{locale}/'
            }).match('foo/it/path'),
            {
                'l': 'foo/it/',
                'locale': 'it',
                's1': 'path',
            }
        )
        self.assertDictEqual(
            Matcher('{l}*', {
                'l': 'foo/{locale}/'
            }).match('foo/it/path'),
            {
                'l': 'foo/it/',
                'locale': 'it',
                's1': 'path',
            }
        )

    def test_variables_sub(self):
        one = Matcher('{base}/{loc}/*', {'base': 'ONE_BASE'})
        other = Matcher('{base}/somewhere/*', {'base': 'OTHER_BASE'})
        self.assertEqual(
            one.sub(other, 'ONE_BASE/ab-CD/special'),
            'OTHER_BASE/somewhere/special'
        )

    def test_copy(self):
        one = Matcher('{base}/{loc}/*', {
            'base': 'ONE_BASE',
            'generic': 'keep'
        })
        other = Matcher(one, {'base': 'OTHER_BASE'})
        self.assertEqual(
            one.sub(other, 'ONE_BASE/ab-CD/special'),
            'OTHER_BASE/ab-CD/special'
        )
        self.assertDictEqual(
            one.env,
            {
                'base': ['ONE_BASE'],
                'generic': ['keep']
            }
        )
        self.assertDictEqual(
            other.env,
            {
                'base': ['OTHER_BASE'],
                'generic': ['keep']
            }
        )

    def test_eq(self):
        self.assertEqual(
            Matcher('foo'),
            Matcher('foo')
        )
        self.assertNotEqual(
            Matcher('foo'),
            Matcher('bar')
        )
        self.assertEqual(
            Matcher('foo', root='/bar/'),
            Matcher('foo', root='/bar/')
        )
        self.assertNotEqual(
            Matcher('foo', root='/bar/'),
            Matcher('foo', root='/baz/')
        )
        self.assertNotEqual(
            Matcher('foo'),
            Matcher('foo', root='/bar/')
        )
        self.assertEqual(
            Matcher('foo', env={'one': 'two'}),
            Matcher('foo', env={'one': 'two'})
        )
        self.assertEqual(
            Matcher('foo'),
            Matcher('foo', env={})
        )
        self.assertNotEqual(
            Matcher('foo', env={'one': 'two'}),
            Matcher('foo', env={'one': 'three'})
        )
        self.assertEqual(
            Matcher('foo', env={'other': 'val'}),
            Matcher('foo', env={'one': 'two'})
        )


class ConcatTest(unittest.TestCase):
    def test_plain(self):
        left = Matcher('some/path/')
        right = Matcher('with/file')
        concatenated = left.concat(right)
        self.assertEqual(str(concatenated), 'some/path/with/file')
        self.assertEqual(concatenated.prefix, 'some/path/with/file')
        pattern_concatenated = left.concat('with/file')
        self.assertEqual(concatenated, pattern_concatenated)

    def test_stars(self):
        left = Matcher('some/*/path/')
        right = Matcher('with/file')
        concatenated = left.concat(right)
        self.assertEqual(concatenated.prefix, 'some/')
        concatenated = right.concat(left)
        self.assertEqual(concatenated.prefix, 'with/filesome/')


class TestAndroid(unittest.TestCase):
    '''special case handling for `android_locale` to handle the funky
    locale codes in Android apps
    '''
    def test_match(self):
        # test matches as well as groupdict aliasing.
        one = Matcher('values-{android_locale}/strings.xml')
        self.assertEqual(
            one.match('values-de/strings.xml'),
            {
                'android_locale': 'de',
                'locale': 'de'
            }
        )
        self.assertEqual(
            one.match('values-de-rDE/strings.xml'),
            {
                'android_locale': 'de-rDE',
                'locale': 'de-DE'
            }
        )
        self.assertEqual(
            one.match('values-b+sr+Latn/strings.xml'),
            {
                'android_locale': 'b+sr+Latn',
                'locale': 'sr-Latn'
            }
        )
        self.assertEqual(
            one.with_env(
                {'locale': 'de'}
            ).match('values-de/strings.xml'),
            {
                'android_locale': 'de',
                'locale': 'de'
            }
        )
        self.assertEqual(
            one.with_env(
                {'locale': 'de-DE'}
            ).match('values-de-rDE/strings.xml'),
            {
                'android_locale': 'de-rDE',
                'locale': 'de-DE'
            }
        )
        self.assertEqual(
            one.with_env(
                {'locale': 'sr-Latn'}
            ).match('values-b+sr+Latn/strings.xml'),
            {
                'android_locale': 'b+sr+Latn',
                'locale': 'sr-Latn'
            }
        )

    def test_repeat(self):
        self.assertEqual(
            Matcher('{android_locale}/{android_locale}').match(
                'b+sr+Latn/b+sr+Latn'
            ),
            {
                'android_locale': 'b+sr+Latn',
                'locale': 'sr-Latn'
            }
        )
        self.assertEqual(
            Matcher(
                '{android_locale}/{android_locale}',
                env={'locale': 'sr-Latn'}
            ).match(
                'b+sr+Latn/b+sr+Latn'
            ),
            {
                'android_locale': 'b+sr+Latn',
                'locale': 'sr-Latn'
            }
        )

    def test_mismatch(self):
        # test failed matches
        one = Matcher('values-{android_locale}/strings.xml')
        self.assertIsNone(
            one.with_env({'locale': 'de'}).match(
                'values-fr.xml'
            )
        )
        self.assertIsNone(
            one.with_env({'locale': 'de-DE'}).match(
                'values-de-DE.xml'
            )
        )
        self.assertIsNone(
            one.with_env({'locale': 'sr-Latn'}).match(
                'values-sr-Latn.xml'
            )
        )
        self.assertIsNone(
            Matcher('{android_locale}/{android_locale}').match(
                'b+sr+Latn/de-rDE'
            )
        )

    def test_prefix(self):
        one = Matcher('values-{android_locale}/strings.xml')
        self.assertEqual(
            one.with_env({'locale': 'de'}).prefix,
            'values-de/strings.xml'
        )
        self.assertEqual(
            one.with_env({'locale': 'de-DE'}).prefix,
            'values-de-rDE/strings.xml'
        )
        self.assertEqual(
            one.with_env({'locale': 'sr-Latn'}).prefix,
            'values-b+sr+Latn/strings.xml'
        )
        self.assertEqual(
            one.prefix,
            'values-'
        )

    def test_aliases(self):
        # test legacy locale code mapping
        # he <-> iw, id <-> in, yi <-> ji
        one = Matcher('values-{android_locale}/strings.xml')
        for legacy, standard in six.iteritems(ANDROID_STANDARD_MAP):
            self.assertDictEqual(
                one.match('values-{}/strings.xml'.format(legacy)),
                {
                    'android_locale': legacy,
                    'locale': standard
                }
            )
            self.assertEqual(
                one.with_env({'locale': standard}).prefix,
                'values-{}/strings.xml'.format(legacy)
            )


class TestRootedMatcher(Rooted, unittest.TestCase):
    def test_root_path(self):
        one = Matcher('some/path', root=self.root)
        self.assertIsNone(one.match('some/path'))
        self.assertIsNotNone(one.match(self.path('/some/path')))

    def test_copy(self):
        one = Matcher('some/path', root=self.path('/one-root'))
        other = Matcher(one, root=self.path('/different-root'))
        self.assertIsNone(other.match('some/path'))
        self.assertIsNone(
            other.match(self.path('/one-root/some/path'))
        )
        self.assertIsNotNone(
            other.match(self.path('/different-root/some/path'))
        )

    def test_rooted(self):
        r1 = self.path('/one-root')
        r2 = self.path('/other-root')
        one = Matcher(self.path('/one-root/full/path'), root=r2)
        self.assertIsNone(one.match(self.path('/other-root/full/path')))
        # concat r2 and r1. r1 is absolute, so we gotta trick that
        concat_root = r2
        if not r1.startswith('/'):
            # windows absolute paths don't start with '/', add one
            concat_root += '/'
        concat_root += r1
        self.assertIsNone(one.match(concat_root + '/full/path'))
        self.assertIsNotNone(one.match(self.path('/one-root/full/path')))

    def test_variable(self):
        r1 = self.path('/one-root')
        r2 = self.path('/other-root')
        one = Matcher(
            '{var}/path',
            env={'var': 'relative-dir'},
            root=r1
        )
        self.assertIsNone(one.match('relative-dir/path'))
        self.assertIsNotNone(
            one.match(self.path('/one-root/relative-dir/path'))
        )
        other = Matcher(one, env={'var': r2})
        self.assertIsNone(
            other.match(self.path('/one-root/relative-dir/path'))
        )
        self.assertIsNotNone(
            other.match(self.path('/other-root/path'))
        )
