# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import textwrap
import unittest

from compare_locales.tests import BaseHelper
from compare_locales.paths import File


def dedent_ftl(text):
    return textwrap.dedent(text.rstrip() + "\n").encode("utf-8")


REFERENCE = b'''\
simple = value
term_ref = some { -term }
  .attr = is simple
msg-attr-ref = some {button.label}
mixed-attr = value
  .and = attribute
only-attr =
  .one = exists
-term = value need
  .attrs = can differ
'''


class TestFluent(BaseHelper):
    file = File('foo.ftl', 'foo.ftl')
    refContent = REFERENCE

    def test_simple(self):
        self._test(b'''simple = localized''',
                   tuple())


class TestMessage(BaseHelper):
    file = File('foo.ftl', 'foo.ftl')
    refContent = REFERENCE

    def test_excess_attribute(self):
        self._test(
            dedent_ftl('''\
            simple = value with
                .obsolete = attribute
            '''),
            (
                (
                    'error', 24,
                    'Obsolete attribute: obsolete', 'fluent'
                ),
            )
        )

    def test_duplicate_attribute(self):
        self._test(
            dedent_ftl('''\
            only-attr =
                .one = attribute
                .one = again
                .one = three times
            '''),
            (
                (
                    'warning', 16,
                    'Attribute "one" is duplicated', 'fluent'
                ),
                (
                    'warning', 37,
                    'Attribute "one" is duplicated', 'fluent'
                ),
                (
                    'warning', 54,
                    'Attribute "one" is duplicated', 'fluent'
                ),
            )
        )

    def test_only_attributes(self):
        self._test(
            dedent_ftl('''\
            only-attr = obsolete value
            '''),
            (
                (
                    'error', 0,
                    'Missing attribute: one', 'fluent'
                ),
                (
                    'error', 12,
                    'Obsolete value', 'fluent'
                ),
            )
        )

    def test_missing_value(self):
        self._test(
            dedent_ftl('''\
            mixed-attr =
                .and = attribute exists
            '''),
            (
                (
                    'error', 0,
                    'Missing value', 'fluent'
                ),
            )
        )


class TestTerm(BaseHelper):
    file = File('foo.ftl', 'foo.ftl')
    refContent = REFERENCE

    def test_mismatching_attribute(self):
        self._test(
            dedent_ftl('''\
            -term = value with
                .different = attribute
            '''),
            tuple()
        )

    def test_duplicate_attribute(self):
        self._test(
            dedent_ftl('''\
            -term = need value
                .one = attribute
                .one = again
                .one = three times
            '''),
            (
                (
                    'warning', 23,
                    'Attribute "one" is duplicated', 'fluent'
                ),
                (
                    'warning', 44,
                    'Attribute "one" is duplicated', 'fluent'
                ),
                (
                    'warning', 61,
                    'Attribute "one" is duplicated', 'fluent'
                ),
            )
        )


class TestMessageReference(BaseHelper):
    file = File('foo.ftl', 'foo.ftl')
    refContent = REFERENCE

    def test_msg_attr(self):
        self._test(
            b'''msg-attr-ref = Nice {button.label}''',
            tuple()
        )
        self._test(
            b'''msg-attr-ref = not at all''',
            (
                (
                    'warning', 0,
                    'Missing message reference: button.label', 'fluent'
                ),
            )
        )
        self._test(
            b'''msg-attr-ref = {button} is not a label''',
            (
                (
                    'warning', 0,
                    'Missing message reference: button.label', 'fluent'
                ),
                (
                    'warning', 16,
                    'Obsolete message reference: button', 'fluent'
                ),
            )
        )
        self._test(
            b'''msg-attr-ref = {button.tooltip} is not a label''',
            (
                (
                    'warning', 0,
                    'Missing message reference: button.label', 'fluent'
                ),
                (
                    'warning', 16,
                    'Obsolete message reference: button.tooltip', 'fluent'
                ),
            )
        )


class TestTermReference(BaseHelper):
    file = File('foo.ftl', 'foo.ftl')
    refContent = REFERENCE

    def test_good_term_ref(self):
        self._test(
            dedent_ftl('''\
            term_ref = localized to {-term}
                .attr = is plain
            '''),
            tuple()
        )

    def test_missing_term_ref(self):
        self._test(
            dedent_ftl('''\
            term_ref = localized
                .attr = should not refer to {-term}
            '''),
            (
                (
                    'warning', 0,
                    'Missing term reference: -term', 'fluent'
                ),
                (
                    'warning', 54,
                    'Obsolete term reference: -term', 'fluent'
                ),
            )
        )

    def test_l10n_only_term_ref(self):
        self._test(
            b'''simple = localized with { -term }''',
            (
               (
                    u'warning', 26,
                    u'Obsolete term reference: -term', u'fluent'
                ),
            )
        )

    def test_term_attr(self):
        self._test(
            dedent_ftl('''\
            term_ref = Depends on { -term.prop ->
                *[some] Term prop, doesn't reference the term value, though.
              }
              .attr = still simple
            '''),
            (
                (
                    u'warning', 0,
                    u'Missing term reference: -term', u'fluent'
                ),
            )
        )


class SelectExpressionTest(BaseHelper):
    file = File('foo.ftl', 'foo.ftl')
    refContent = b'''\
msg = { $val ->
 *[other] Show something
  }
-term = Foopy
'''

    def test_no_select(self):
        self._test(
            b'''msg = Something''',
            tuple()
        )

    def test_good(self):
        self._test(
            dedent_ftl('''\
            msg = { $val ->
             *[one] one
              [other] other
              }
            '''),
            tuple()
        )

    def test_duplicate_variant(self):
        self._test(
            dedent_ftl('''\
            msg = { $val ->
             *[one] one
              [one] other
              }
            '''),
            (
                (
                    'warning', 19,
                    'Variant key "one" is duplicated', 'fluent'
                ),
                (
                    'warning', 31,
                    'Variant key "one" is duplicated', 'fluent'
                ),
            )
        )

    def test_term_value(self):
        self._test(
            dedent_ftl('''\
            -term = { PLATFORM() ->
             *[one] one
              [two] two
              [two] duplicate
              }
            '''),
            (
                (
                    'warning', 39,
                    'Variant key "two" is duplicated', 'fluent'
                ),
                (
                    'warning', 51,
                    'Variant key "two" is duplicated', 'fluent'
                ),
            )
        )

    def test_term_attribute(self):
        self._test(
            dedent_ftl('''\
            -term = boring value
              .attr = { PLATFORM() ->
               *[one] one
                [two] two
                [two] duplicate
                [two] three
                }
            '''),
            (
                (
                    'warning', 66,
                    'Variant key "two" is duplicated', 'fluent'
                ),
                (
                    'warning', 80,
                    'Variant key "two" is duplicated', 'fluent'
                ),
                (
                    'warning', 100,
                    'Variant key "two" is duplicated', 'fluent'
                ),
            )
        )


class PluralTest(BaseHelper):
    file = File('foo.ftl', 'foo.ftl')
    refContent = b'''\
msg = { $val ->
 *[other] Show something
  }
'''

    def test_missing_plural(self):
        self.file.locale = 'ru'
        self._test(
            dedent_ftl('''\
            msg = { $val ->
              [one] thing
              [3] is ok
             *[many] stuff
             }
            '''),
            (
                (
                    'warning', 19,
                    'Plural categories missing: few', 'fluent'
                ),
            )
        )

    def test_ignoring_other(self):
        self.file.locale = 'de'
        self._test(
            dedent_ftl('''\
            msg = { $val ->
              [1] thing
             *[other] stuff
             }
            '''),
            tuple()
        )


if __name__ == '__main__':
    unittest.main()
