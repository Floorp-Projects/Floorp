# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import urllib

from marionette_driver.by import By
from marionette_harness import MarionetteTestCase, skip


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class TestTitle(MarionetteTestCase):
    @property
    def document_title(self):
        return self.marionette.execute_script("return document.title", sandbox=None)

    def test_title_from_top(self):
        self.marionette.navigate(inline("<title>foo</title>"))
        self.assertEqual("foo", self.marionette.title)

    def test_title_from_frame(self):
        self.marionette.navigate(inline("""<title>foo</title>
<iframe src="{}">""".format(inline("<title>bar</title>"))))

        self.assertEqual("foo", self.document_title)
        self.assertEqual("foo", self.marionette.title)

        bar = self.marionette.find_element(By.TAG_NAME, "iframe")
        self.marionette.switch_to_frame(bar)
        self.assertEqual("bar", self.document_title)
        self.assertEqual("foo", self.marionette.title)

    def test_title_from_nested_frame(self):
        self.marionette.navigate(inline("""<title>foo</title>
<iframe id=bar src="{}">""".format(inline("""<title>bar</title>
<iframe id=baz src="{}">""".format(inline("<title>baz</title>"))))))

        self.assertEqual("foo", self.document_title)
        self.assertEqual("foo", self.marionette.title)

        bar = self.marionette.find_element(By.ID, "bar")
        self.marionette.switch_to_frame(bar)
        self.assertEqual("bar", self.document_title)
        self.assertEqual("foo", self.marionette.title)

        baz = self.marionette.find_element(By.ID, "baz")
        self.marionette.switch_to_frame(baz)
        self.assertEqual("baz", self.document_title)
        self.assertEqual("foo", self.marionette.title)

    @skip("https://bugzilla.mozilla.org/show_bug.cgi?id=1255946")
    def test_navigate_top_frame_from_nested_context(self):
        self.marionette.navigate(inline("""<title>foo</title>
<iframe src="{}">""".format(inline("""<title>bar</title>
<a href="{}" target=_top>consume top frame</a>""".format(inline("<title>baz</title>"))))))

        self.assertEqual("foo", self.document_title)
        self.assertEqual("foo", self.marionette.title)

        bar = self.marionette.find_element(By.TAG_NAME, "iframe")
        self.marionette.switch_to_frame(bar)
        self.assertEqual("bar", self.document_title)
        self.assertEqual("foo", self.marionette.title)

        consume = self.marionette.find_element(By.TAG_NAME, "a")
        consume.click()

        self.assertEqual("baz", self.document_title)
        self.assertEqual("baz", self.marionette.title)
