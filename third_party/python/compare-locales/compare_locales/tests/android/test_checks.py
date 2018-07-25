# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals

from compare_locales.tests.test_checks import BaseHelper
from compare_locales.paths import File


ANDROID_WRAPPER = b'''<?xml version="1.0" encoding="utf-8"?>
<resources>
  <string name="foo">%s</string>
</resources>
'''


class QuotesTest(BaseHelper):
    file = File('values/strings.xml', 'values/strings.xml')
    refContent = ANDROID_WRAPPER % b'plain'

    def test_straightquotes(self):
        self._test(
            ANDROID_WRAPPER % b'""',
            (
                (
                    "error",
                    0,
                    "Double straight quotes not allowed",
                    "android"
                ),
            )
        )
        self._test(
            ANDROID_WRAPPER % b'"some"',
            tuple()
        )
        self._test(
            ANDROID_WRAPPER % b'some\\"',
            tuple()
        )
        self._test(
            ANDROID_WRAPPER % b'some"',
            tuple()
        )
        self._test(
            ANDROID_WRAPPER % b'some',
            tuple()
        )
        self._test(
            ANDROID_WRAPPER % b'some""',
            (
                (
                    "error",
                    4,
                    "Double straight quotes not allowed",
                    "android"
                ),
            )
        )

    def test_apostrophes(self):
        self._test(
            ANDROID_WRAPPER % b'''"some'apos"''',
            tuple()
        )
        self._test(
            ANDROID_WRAPPER % b'''some\\'apos''',
            tuple()
        )
        self._test(
            ANDROID_WRAPPER % b'''some'apos''',
            (
                (
                    "error",
                    4,
                    "Apostrophe must be escaped",
                    "android"
                ),
            )
        )


class TranslatableTest(BaseHelper):
    file = File('values/strings.xml', 'values/strings.xml')
    refContent = (ANDROID_WRAPPER % b'plain').replace(
        b'name="foo"',
        b'translatable="false" name="foo"')

    def test_translatable(self):
        self._test(
            ANDROID_WRAPPER % b'"some"',
            (
                (
                    "error",
                    0,
                    "strings must be translatable",
                    "android"
                ),
            )
        )
