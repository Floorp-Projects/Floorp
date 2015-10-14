# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import imghdr
import struct
import urllib

from unittest import skip

from marionette import MarionetteTestCase


def inline(doc, mime="text/html;charset=utf-8"):
    return "data:%s,%s" % (mime, urllib.quote(doc))


ELEMENT = "iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAYAAAAeP4ixAAAAVklEQVRoge3PMQ0AMAzAsPJHVWYbjEWTj/zx7O75oXk9AAISD6QWSC2QWiC1QGqB1AKpBVILpBZILZBaILVAaoHUAqkFUgukFkgtkFogtUBqgdT6BnIBMKa1DtYxhPkAAAAASUVORK5CYII="
HIGHLIGHTED_ELEMENT = "iVBORw0KGgoAAAANSUhEUgAAADIAAAAyCAYAAAAeP4ixAAAAVklEQVRoge3PQRHAQAgAMfyrwhm1sb3JIwIyN3MvmJu53f01kRqRGpEakRqRGpEakRqRGpEakRqRGpEakRqRGpEakRqRGpEakRqRGpEakRqRmvciL/gAQgW/OxTpMPwAAAAASUVORK5CYII="


box = inline(
    "<div id=green style='width: 50px; height: 50px; background: silver;'></div>")
long = inline("<body style='height: 300vh'><p style='margin-top: 100vh'>foo")
short = inline("<body style='height: 10vh'>")
svg = inline("""<svg xmlns="http://www.w3.org/2000/svg" height="20" width="20">
 <rect height="20" width="20"/>
</svg>""", mime="image/svg+xml")


class ScreenCaptureTestCase(MarionetteTestCase):
    def assert_png(self, string):
        """Test that string is a Base64 encoded PNG file."""
        image = base64.decodestring(string)
        self.assertEqual(imghdr.what("", image), "png")

    def get_image_dimensions(self, string):
        self.assert_png(string)
        image = base64.decodestring(string)
        width, height = struct.unpack(">LL", image[16:24])
        return int(width), int(height)


class Chrome(ScreenCaptureTestCase):
    @property
    def primary_window_dimensions(self):
        return tuple(self.marionette.execute_script("""
            let win = document.getElementsByTagName("window")[0];
            let rect = win.getBoundingClientRect();
            return [rect.width, rect.height];"""))

    def setUp(self):
        ScreenCaptureTestCase.setUp(self)
        self.marionette.set_context("chrome")

    # A full chrome window screenshot is not the outer dimensions of
    # the window, but instead the bounding box of the <window> inside
    # <browser>.
    def test_window(self):
        string = self.marionette.screenshot()
        self.assert_png(string)
        self.assertEqual(self.primary_window_dimensions,
                         self.get_image_dimensions(string))


    def test_chrome_delegation(self):
        with self.marionette.using_context("content"):
            content = self.marionette.screenshot()
        chrome = self.marionette.screenshot()
        self.assertNotEqual(content, chrome)


class Content(ScreenCaptureTestCase):
    @property
    def body_scroll_dimensions(self):
        return tuple(self.marionette.execute_script(
            "return [document.body.scrollWidth, document.body.scrollHeight]"))

    @property
    def viewport_dimensions(self):
        return tuple(self.marionette.execute_script("""
            let docEl = document.documentElement;
            return [docEl.clientWidth, docEl.clientHeight];"""))

    @property
    def document_element(self):
        return self.marionette.execute_script("return document.documentElement")

    @property
    def page_y_offset(self):
        return self.marionette.execute_script("return window.pageYOffset")

    def setUp(self):
        ScreenCaptureTestCase.setUp(self)
        self.marionette.set_context("content")

    def test_html_document_element(self):
        self.marionette.navigate(long)
        string = self.marionette.screenshot()
        self.assert_png(string)
        self.assertEqual(
            self.body_scroll_dimensions, self.get_image_dimensions(string))

    def test_svg_document_element(self):
        self.marionette.navigate(svg)
        doc_el = self.document_element
        string = self.marionette.screenshot()
        self.assert_png(string)
        self.assertEqual((doc_el.rect["width"], doc_el.rect["height"]),
            self.get_image_dimensions(string))

    def test_viewport(self):
        self.marionette.navigate(short)
        string = self.marionette.screenshot(full=False)
        self.assert_png(string)
        self.assertEqual(
            self.viewport_dimensions, self.get_image_dimensions(string))

    def test_viewport_after_scroll(self):
        self.marionette.navigate(long)
        before = self.marionette.screenshot()
        el = self.marionette.find_element("tag name", "p")
        self.marionette.execute_script(
            "arguments[0].scrollIntoView()", script_args=[el])
        after = self.marionette.screenshot(full=False)
        self.assertNotEqual(before, after)
        self.assertGreater(self.page_y_offset, 0)

    def test_element(self):
        self.marionette.navigate(box)
        el = self.marionette.find_element("tag name", "div")
        string = self.marionette.screenshot(element=el)
        self.assert_png(string)
        self.assertEqual(
            (el.rect["width"], el.rect["height"]), self.get_image_dimensions(string))
        self.assertEqual(ELEMENT, string)

    @skip("https://bugzilla.mozilla.org/show_bug.cgi?id=1213875")
    def test_element_scrolled_into_view(self):
        self.marionette.navigate(long)
        el = self.marionette.find_element("tag name", "p")
        string = self.marionette.screenshot(element=el)
        self.assert_png(string)
        self.assertEqual(
            (el.rect["width"], el.rect["height"]), self.get_image_dimensions(string))
        self.assertGreater(self.page_y_offset, 0)

    def test_element_with_highlight(self):
        self.marionette.navigate(box)
        el = self.marionette.find_element("tag name", "div")
        string = self.marionette.screenshot(element=el, highlights=[el])
        self.assert_png(string)
        self.assertEqual(
            (el.rect["width"], el.rect["height"]), self.get_image_dimensions(string))
        self.assertEqual(HIGHLIGHTED_ELEMENT, string)

    def test_binary_element(self):
        self.marionette.navigate(box)
        el = self.marionette.find_element("tag name", "div")
        bin = self.marionette.screenshot(element=el, format="binary")
        enc = base64.b64encode(bin)
        self.assertEqual(ELEMENT, enc)

    def test_unknown_format(self):
        with self.assertRaises(ValueError):
            self.marionette.screenshot(format="cheese")
