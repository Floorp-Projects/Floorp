# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase


class TestPageSource(MarionetteTestCase):
    def testShouldReturnTheSourceOfAPage(self):
        test_html = self.marionette.absolute_url("testPageSource.html")
        self.marionette.navigate(test_html)
        source = self.marionette.page_source
        from_web_api = self.marionette.execute_script("return document.documentElement.outerHTML")
        self.assertTrue("<html" in source)
        self.assertTrue("PageSource" in source)
        self.assertEqual(source, from_web_api)

    def testShouldReturnTheSourceOfAPageWhenThereAreUnicodeChars(self):
        test_html = self.marionette.absolute_url("testPageSourceWithUnicodeChars.html")
        self.marionette.navigate(test_html)
        # if we don't throw on the next line we are good!
        source = self.marionette.page_source
        from_web_api = self.marionette.execute_script("return document.documentElement.outerHTML")
        self.assertEqual(source, from_web_api)

    def testShouldReturnAXMLDocumentSource(self):
        test_xml = self.marionette.absolute_url("testPageSource.xml")
        self.marionette.navigate(test_xml)
        source = self.marionette.page_source
        from_web_api = self.marionette.execute_script("return document.documentElement.outerHTML")
        import re
        self.assertEqual(re.sub("\s", "", source), "<xml><foo><bar>baz</bar></foo></xml>")
        self.assertEqual(source, from_web_api)
