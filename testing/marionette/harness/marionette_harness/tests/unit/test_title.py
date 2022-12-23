# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestTitle(MarionetteTestCase):
    def test_basic(self):
        self.marionette.navigate(inline("<title>foo</title>"))
        self.assertEqual(self.marionette.title, "foo")
