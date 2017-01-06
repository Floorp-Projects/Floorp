# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import hashlib
import imghdr
import struct
import urllib

from marionette_driver import By
from marionette_driver.errors import JavascriptException, NoSuchWindowException
from marionette_harness import (
    MarionetteTestCase,
    skip,
    skip_if_mobile,
    WindowManagerMixin,
)


def inline(doc, mime="text/html;charset=utf-8"):
    return "data:{0},{1}".format(mime, urllib.quote(doc))


box = inline("<body><div id='box'><p id='green' style='width: 50px; height: 50px; "
             "background: silver;'></p></div></body>")
input = inline("<body><input id='text-input'></input></body>")
long = inline("<body style='height: 300vh'><p style='margin-top: 100vh'>foo</p></body>")
short = inline("<body style='height: 10vh'></body>")
svg = inline("""
    <svg xmlns="http://www.w3.org/2000/svg" height="20" width="20">
      <rect height="20" width="20"/>
    </svg>""", mime="image/svg+xml")


class ScreenCaptureTestCase(MarionetteTestCase):

    def setUp(self):
        super(ScreenCaptureTestCase, self).setUp()

        self._device_pixel_ratio = None

    @property
    def device_pixel_ratio(self):
        if self._device_pixel_ratio is None:
            self._device_pixel_ratio = self.marionette.execute_script("""
                return window.devicePixelRatio
                """)
        return self._device_pixel_ratio

    @property
    def document_element(self):
        return self.marionette.find_element(By.CSS_SELECTOR, ":root")

    @property
    def page_y_offset(self):
        return self.marionette.execute_script("return window.pageYOffset")

    @property
    def viewport_dimensions(self):
        return self.marionette.execute_script("""
            return [arguments[0].clientWidth,
                    arguments[0].clientHeight];
            """, script_args=[self.document_element])

    def assert_png(self, screenshot):
        """Test that screenshot is a Base64 encoded PNG file."""
        image = base64.decodestring(screenshot)
        self.assertEqual(imghdr.what("", image), "png")

    def assert_formats(self, element=None):
        if element is None:
            element = self.document_element

        screenshot_default = self.marionette.screenshot(element=element)
        screenshot_image = self.marionette.screenshot(element=element, format="base64")
        binary1 = self.marionette.screenshot(element=element, format="binary")
        binary2 = self.marionette.screenshot(element=element, format="binary")
        hash1 = self.marionette.screenshot(element=element, format="hash")
        hash2 = self.marionette.screenshot(element=element, format="hash")

        # Valid data should have been returned
        self.assert_png(screenshot_image)
        self.assertEqual(imghdr.what("", binary1), "png")
        self.assertEqual(screenshot_image, base64.b64encode(binary1))
        self.assertEqual(hash1, hashlib.sha256(screenshot_image).hexdigest())

        # Different formats produce different data
        self.assertNotEqual(screenshot_image, binary1)
        self.assertNotEqual(screenshot_image, hash1)
        self.assertNotEqual(binary1, hash1)

        # A second capture should be identical
        self.assertEqual(screenshot_image, screenshot_default)
        self.assertEqual(binary1, binary2)
        self.assertEqual(hash1, hash2)

    def get_element_dimensions(self, element):
        rect = element.rect
        return rect["width"], rect["height"]

    def get_image_dimensions(self, screenshot):
        self.assert_png(screenshot)
        image = base64.decodestring(screenshot)
        width, height = struct.unpack(">LL", image[16:24])
        return int(width), int(height)

    def scale(self, rect):
        return (int(rect[0] * self.device_pixel_ratio),
                int(rect[1] * self.device_pixel_ratio))


class TestScreenCaptureChrome(WindowManagerMixin, ScreenCaptureTestCase):

    def setUp(self):
        super(TestScreenCaptureChrome, self).setUp()
        self.marionette.set_context("chrome")

    def tearDown(self):
        self.close_all_windows()
        super(TestScreenCaptureChrome, self).tearDown()

    @property
    def window_dimensions(self):
        return tuple(self.marionette.execute_script("""
            let el = document.documentElement;
            let rect = el.getBoundingClientRect();
            return [rect.width, rect.height];
            """))

    def open_dialog(self, url=None, width=None, height=None):
        if url is None:
            url = "chrome://marionette/content/test_dialog.xul"

        def opener():
            features = "chrome"
            if height is not None:
                features += ",height={}".format(height)
            if width is not None:
                features += ",width={}".format(width)

            self.marionette.execute_script("""
                window.open(arguments[0], "", arguments[1]);
                """, script_args=[url, features])

        return self.open_window(opener)

    def test_capture_different_context(self):
        """Check that screenshots in content and chrome are different."""
        with self.marionette.using_context("content"):
            screenshot_content = self.marionette.screenshot()
        screenshot_chrome = self.marionette.screenshot()
        self.assertNotEqual(screenshot_content, screenshot_chrome)

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def test_capture_element(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)

        # Ensure we only capture the element
        el = self.marionette.find_element(By.ID, "test-list")
        screenshot_element = self.marionette.screenshot(element=el)
        self.assertEqual(self.scale(self.get_element_dimensions(el)),
                         self.get_image_dimensions(screenshot_element))

        # Ensure we do not capture the full window
        screenshot_dialog = self.marionette.screenshot()
        self.assertNotEqual(screenshot_dialog, screenshot_element)

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

    def test_capture_flags(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)

        textbox = self.marionette.find_element(By.ID, "text-box")
        textbox.send_keys("")
        screenshot_focus = self.marionette.screenshot()

        self.marionette.execute_script("arguments[0].blur();", script_args=[textbox])
        screenshot_no_focus = self.marionette.screenshot()

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

        self.assertNotEqual(screenshot_focus, screenshot_no_focus)

    def test_capture_full_area(self):
        # A full capture is not the outer dimensions of the window,
        # but instead the bounding box of the window's root node (documentElement).
        screenshot_full = self.marionette.screenshot()
        screenshot_root = self.marionette.screenshot(element=self.document_element)

        self.assert_png(screenshot_full)
        self.assert_png(screenshot_root)
        self.assertEqual(screenshot_root, screenshot_full)
        self.assertEqual(self.scale(self.get_element_dimensions(self.document_element)),
                         self.get_image_dimensions(screenshot_full))

    def test_capture_viewport(self):
        # Load a HTML test page into the chrome window to get scrollbars
        test_page = self.marionette.absolute_url("test.html")
        dialog = self.open_dialog(url=test_page, width=50, height=50)
        self.marionette.switch_to_window(dialog)

        # Size of screenshot has to match viewport size
        screenshot = self.marionette.screenshot(full=False)
        self.assert_png(screenshot)
        self.assertEqual(self.scale(self.viewport_dimensions),
                         self.get_image_dimensions(screenshot))
        self.assertNotEqual(self.scale(self.window_dimensions),
                            self.get_image_dimensions(screenshot))

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

    def test_capture_window_already_closed(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)
        self.marionette.close_chrome_window()

        self.assertRaises(NoSuchWindowException, self.marionette.screenshot)
        self.marionette.switch_to_window(self.start_window)

    def test_formats(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)

        self.assert_formats()

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

    def test_format_unknown(self):
        with self.assertRaises(ValueError):
            self.marionette.screenshot(format="cheese")

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def test_highlight_elements(self):
        dialog = self.open_dialog()
        self.marionette.switch_to_window(dialog)

        # Highlighting the element itself shouldn't make the image larger
        element = self.marionette.find_element(By.ID, "test-list")
        screenshot_element = self.marionette.screenshot(element=element)
        screenshot_highlight = self.marionette.screenshot(element=element,
                                                          highlights=[element])
        self.assertEqual(self.scale(self.get_element_dimensions(element)),
                         self.get_image_dimensions(screenshot_element))
        self.assertNotEqual(screenshot_element, screenshot_highlight)

        # Highlighting a sub element
        button = self.marionette.find_element(By.ID, "choose-button")
        screenshot_highlight_button = self.marionette.screenshot(element=element,
                                                                 highlights=[button])
        self.assertNotEqual(screenshot_element, screenshot_highlight_button)
        self.assertNotEqual(screenshot_highlight, screenshot_highlight_button)

        self.marionette.close_chrome_window()
        self.marionette.switch_to_window(self.start_window)

    def test_highlight_element_not_seen(self):
        """Check that for not found elements an exception is raised."""
        with self.marionette.using_context('content'):
            self.marionette.navigate(box)
            content_element = self.marionette.find_element(By.ID, "green")

        self.assertRaisesRegexp(JavascriptException, "Element reference not seen before",
                                self.marionette.screenshot, highlights=[content_element])

        chrome_document_element = self.document_element
        with self.marionette.using_context('content'):
            self.assertRaisesRegexp(JavascriptException, "Element reference not seen before",
                                    self.marionette.screenshot,
                                    highlights=[chrome_document_element])


class TestScreenCaptureContent(WindowManagerMixin, ScreenCaptureTestCase):

    def setUp(self):
        super(TestScreenCaptureContent, self).setUp()
        self.marionette.set_context("content")

    def tearDown(self):
        self.close_all_tabs()
        super(TestScreenCaptureContent, self).tearDown()

    @property
    def scroll_dimensions(self):
        return tuple(self.marionette.execute_script("""
            return [document.body.scrollWidth, document.body.scrollHeight]
            """))

    @skip_if_mobile("Needs application independent method to open a new tab")
    def test_capture_tab_already_closed(self):
        tab = self.open_tab()
        self.marionette.switch_to_window(tab)
        self.marionette.close()

        self.assertRaises(NoSuchWindowException, self.marionette.screenshot)
        self.marionette.switch_to_window(self.start_tab)

    def test_capture_element(self):
        self.marionette.navigate(box)
        el = self.marionette.find_element(By.TAG_NAME, "div")
        screenshot = self.marionette.screenshot(element=el)
        self.assert_png(screenshot)
        self.assertEqual(self.scale(self.get_element_dimensions(el)),
                         self.get_image_dimensions(screenshot))

    @skip("Bug 1213875")
    def test_capture_element_scrolled_into_view(self):
        self.marionette.navigate(long)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        screenshot = self.marionette.screenshot(element=el)
        self.assert_png(screenshot)
        self.assertEqual(self.scale(self.get_element_dimensions(el)),
                         self.get_image_dimensions(screenshot))
        self.assertGreater(self.page_y_offset, 0)

    def test_capture_flags(self):
        self.marionette.navigate(input)

        textbox = self.marionette.find_element(By.ID, "text-input")
        textbox.send_keys("")
        screenshot_focus = self.marionette.screenshot()

        self.marionette.execute_script("arguments[0].blur();", script_args=[textbox])
        screenshot_no_focus = self.marionette.screenshot()

        self.assertNotEqual(screenshot_focus, screenshot_no_focus)

    def test_capture_html_document_element(self):
        self.marionette.navigate(long)
        screenshot = self.marionette.screenshot()
        self.assert_png(screenshot)
        self.assertEqual(self.scale(self.scroll_dimensions),
                         self.get_image_dimensions(screenshot))

    def test_capture_svg_document_element(self):
        self.marionette.navigate(svg)
        screenshot = self.marionette.screenshot()
        self.assert_png(screenshot)
        self.assertEqual(self.scale(self.get_element_dimensions(self.document_element)),
                         self.get_image_dimensions(screenshot))

    def test_capture_viewport(self):
        url = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(short)
        self.marionette.navigate(url)
        screenshot = self.marionette.screenshot(full=False)
        self.assert_png(screenshot)
        self.assertEqual(self.scale(self.viewport_dimensions),
                         self.get_image_dimensions(screenshot))

    def test_capture_viewport_after_scroll(self):
        self.marionette.navigate(long)
        before = self.marionette.screenshot()
        el = self.marionette.find_element(By.TAG_NAME, "p")
        self.marionette.execute_script(
            "arguments[0].scrollIntoView()", script_args=[el])
        after = self.marionette.screenshot(full=False)
        self.assertNotEqual(before, after)
        self.assertGreater(self.page_y_offset, 0)

    def test_formats(self):
        self.marionette.navigate(box)

        # Use a smaller region to speed up the test
        element = self.marionette.find_element(By.TAG_NAME, "div")
        self.assert_formats(element=element)

    def test_format_unknown(self):
        with self.assertRaises(ValueError):
            self.marionette.screenshot(format="cheese")

    def test_highlight_elements(self):
        self.marionette.navigate(box)
        element = self.marionette.find_element(By.TAG_NAME, "div")

        # Highlighting the element itself shouldn't make the image larger
        screenshot_element = self.marionette.screenshot(element=element)
        screenshot_highlight = self.marionette.screenshot(element=element,
                                                          highlights=[element])
        self.assertEqual(self.scale(self.get_element_dimensions(element)),
                         self.get_image_dimensions(screenshot_highlight))
        self.assertNotEqual(screenshot_element, screenshot_highlight)

        # Highlighting a sub element
        paragraph = self.marionette.find_element(By.ID, "green")
        screenshot_highlight_paragraph = self.marionette.screenshot(element=element,
                                                                    highlights=[paragraph])
        self.assertNotEqual(screenshot_element, screenshot_highlight_paragraph)
        self.assertNotEqual(screenshot_highlight, screenshot_highlight_paragraph)

    def test_scroll_default(self):
        self.marionette.navigate(long)
        before = self.page_y_offset
        el = self.marionette.find_element(By.TAG_NAME, "p")
        self.marionette.screenshot(element=el, format="hash")
        self.assertNotEqual(before, self.page_y_offset)

    def test_scroll(self):
        self.marionette.navigate(long)
        before = self.page_y_offset
        el = self.marionette.find_element(By.TAG_NAME, "p")
        self.marionette.screenshot(element=el, format="hash", scroll=True)
        self.assertNotEqual(before, self.page_y_offset)

    def test_scroll_off(self):
        self.marionette.navigate(long)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        before = self.page_y_offset
        self.marionette.screenshot(element=el, format="hash", scroll=False)
        self.assertEqual(before, self.page_y_offset)

    def test_scroll_no_element(self):
        self.marionette.navigate(long)
        before = self.page_y_offset
        self.marionette.screenshot(format="hash", scroll=True)
        self.assertEqual(before, self.page_y_offset)
