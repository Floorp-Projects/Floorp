# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_harness import MarionetteTestCase


def inline(doc, mime=None, charset=None):
    mime = "html" if mime is None else mime
    charset = "utf-8" if (charset is None) else charset
    return "data:text/{};charset={},{}".format(mime, charset, quote(doc))


class TestPageSource(MarionetteTestCase):
    def testShouldReturnTheSourceOfAPage(self):
        test_html = inline("<body><p> Check the PageSource</body>")
        self.marionette.navigate(test_html)
        source = self.marionette.page_source
        from_web_api = self.marionette.execute_script(
            "return document.documentElement.outerHTML"
        )
        self.assertTrue("<html" in source)
        self.assertTrue("PageSource" in source)
        self.assertEqual(source, from_web_api)

    def testShouldReturnTheSourceOfAPageWhenThereAreUnicodeChars(self):
        test_html = inline(
            '<head><meta http-equiv="pragma" content="no-cache"/></head><body><!-- the \u00ab section[id^="wifi-"] \u00bb selector.--></body>'
        )
        self.marionette.navigate(test_html)
        # if we don't throw on the next line we are good!
        source = self.marionette.page_source
        from_web_api = self.marionette.execute_script(
            "return document.documentElement.outerHTML"
        )
        self.assertEqual(source, from_web_api)

    def testShouldReturnAXMLDocumentSource(self):
        test_xml = inline("<xml><foo><bar>baz</bar></foo></xml>", "xml")
        self.marionette.navigate(test_xml)
        source = self.marionette.page_source
        from_web_api = self.marionette.execute_script(
            "return document.documentElement.outerHTML"
        )
        import re

        self.assertEqual(
            re.sub("\s", "", source), "<xml><foo><bar>baz</bar></foo></xml>"
        )
        self.assertEqual(source, from_web_api)
