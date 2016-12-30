# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import contextlib
import time
import urllib

from marionette_driver import errors, By, Wait
from marionette_harness import (
    MarionetteTestCase,
    skip,
    skip_if_mobile,
    WindowManagerMixin,
)


def inline(doc):
    return "data:text/html;charset=utf-8,%s" % urllib.quote(doc)


class TestNavigate(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestNavigate, self).setUp()

        self.marionette.navigate("about:")
        self.test_doc = self.marionette.absolute_url("test.html")
        self.iframe_doc = self.marionette.absolute_url("test_iframe.html")

    def tearDown(self):
        self.close_all_windows()

        super(TestNavigate, self).tearDown()

    @property
    def location_href(self):
        return self.marionette.execute_script("return window.location.href")

    def test_set_location_through_execute_script(self):
        self.marionette.execute_script(
            "window.location.href = '%s'" % self.test_doc)
        Wait(self.marionette).until(
            lambda _: self.test_doc == self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_navigate(self):
        self.marionette.navigate(self.test_doc)
        self.assertNotEqual("about:", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_navigate_chrome_error(self):
        with self.marionette.using_context("chrome"):
            self.assertRaises(errors.UnsupportedOperationException,
                              self.marionette.navigate, "about:blank")
            self.assertRaises(errors.UnsupportedOperationException, self.marionette.go_back)
            self.assertRaises(errors.UnsupportedOperationException, self.marionette.go_forward)
            self.assertRaises(errors.UnsupportedOperationException, self.marionette.refresh)

    def test_get_current_url_returns_top_level_browsing_context_url(self):
        self.marionette.navigate(self.iframe_doc)
        self.assertEqual(self.iframe_doc, self.location_href)
        frame = self.marionette.find_element(By.CSS_SELECTOR, "#test_iframe")
        self.marionette.switch_to_frame(frame)
        self.assertEqual(self.iframe_doc, self.marionette.get_url())

    def test_get_current_url(self):
        self.marionette.navigate(self.test_doc)
        self.assertEqual(self.test_doc, self.marionette.get_url())
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.get_url())

    def test_go_back(self):
        self.marionette.navigate(self.test_doc)
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.location_href)
        self.marionette.go_back()
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_go_forward(self):
        self.marionette.navigate(self.test_doc)
        self.assertNotEqual("about:blank", self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.location_href)
        self.marionette.go_back()
        self.assertEqual(self.test_doc, self.location_href)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.marionette.go_forward()
        self.assertEqual("about:blank", self.location_href)

    def test_refresh(self):
        self.marionette.navigate(self.test_doc)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.assertTrue(self.marionette.execute_script(
            """var elem = window.document.createElement('div'); elem.id = 'someDiv';
            window.document.body.appendChild(elem); return true;"""))
        self.assertFalse(self.marionette.execute_script(
            "return window.document.getElementById('someDiv') == undefined"))
        self.marionette.refresh()
        # TODO(ato): Bug 1291320
        time.sleep(0.2)
        self.assertEqual("Marionette Test", self.marionette.title)
        self.assertTrue(self.marionette.execute_script(
            "return window.document.getElementById('someDiv') == undefined"))

    @skip("Disabled due to Bug 977899")
    def test_navigate_frame(self):
        self.marionette.navigate(self.marionette.absolute_url("test_iframe.html"))
        self.marionette.switch_to_frame(0)
        self.marionette.navigate(self.marionette.absolute_url("empty.html"))
        self.assertTrue('empty.html' in self.marionette.get_url())
        self.marionette.switch_to_frame()
        self.assertTrue('test_iframe.html' in self.marionette.get_url())

    @skip_if_mobile("Bug 1323755 - Socket timeout")
    def test_invalid_protocol(self):
        with self.assertRaises(errors.MarionetteException):
            self.marionette.navigate("thisprotocoldoesnotexist://")

    def test_should_navigate_to_requested_about_page(self):
        self.marionette.navigate("about:neterror")
        self.assertEqual(self.marionette.get_url(), "about:neterror")
        self.marionette.navigate(self.marionette.absolute_url("test.html"))
        self.marionette.navigate("about:blocked")
        self.assertEqual(self.marionette.get_url(), "about:blocked")

    def test_find_element_state_complete(self):
        self.marionette.navigate(self.test_doc)
        state = self.marionette.execute_script(
            "return window.document.readyState")
        self.assertEqual("complete", state)
        self.assertTrue(self.marionette.find_element(By.ID, "mozLink"))

    def test_error_when_exceeding_page_load_timeout(self):
        with self.assertRaises(errors.TimeoutException):
            self.marionette.timeout.page_load = 0.1
            self.marionette.navigate(self.marionette.absolute_url("slow"))
            self.marionette.find_element(By.TAG_NAME, "p")

    def test_navigate_iframe(self):
        self.marionette.navigate(self.iframe_doc)
        self.assertTrue('test_iframe.html' in self.marionette.get_url())
        self.assertTrue(self.marionette.find_element(By.ID, "test_iframe"))

    def test_fragment(self):
        doc = inline("<p id=foo>")
        self.marionette.navigate(doc)
        self.marionette.execute_script("window.visited = true", sandbox=None)
        self.marionette.navigate("%s#foo" % doc)
        self.assertTrue(self.marionette.execute_script(
            "return window.visited", sandbox=None))

    @skip_if_mobile("Fennec doesn't support other chrome windows")
    def test_about_blank_for_new_docshell(self):
        """ Bug 1312674 - Hang when loading about:blank for a new docshell."""
        # Open a window to get a new docshell created for the first tab
        with self.marionette.using_context("chrome"):
            tab = self.open_tab(lambda: self.marionette.execute_script(" window.open() "))
            self.marionette.switch_to_window(tab)

        self.marionette.navigate('about:blank')
        self.marionette.close()
        self.marionette.switch_to_window(self.start_window)

    def test_error_on_tls_navigation(self):
        self.assertRaises(errors.InsecureCertificateException,
                          self.marionette.navigate, self.fixtures.where_is("/test.html", on="https"))

    def test_html_document_to_image_document(self):
        self.marionette.navigate(self.fixtures.where_is("test.html"))
        self.marionette.navigate(self.fixtures.where_is("white.png"))
        self.assertIn("white.png", self.marionette.title)

    def test_image_document_to_image_document(self):
        self.marionette.navigate(self.fixtures.where_is("test.html"))

        self.marionette.navigate(self.fixtures.where_is("white.png"))
        self.assertIn("white.png", self.marionette.title)
        self.marionette.navigate(self.fixtures.where_is("black.png"))
        self.assertIn("black.png", self.marionette.title)

    def test_focus_after_navigation(self):
        self.marionette.quit()
        self.marionette.start_session()

        self.marionette.navigate(inline("<input autofocus>"))
        active_el = self.marionette.execute_script("return document.activeElement")
        focus_el = self.marionette.find_element(By.CSS_SELECTOR, ":focus")
        self.assertEqual(active_el, focus_el)


class TestTLSNavigation(MarionetteTestCase):
    insecure_tls = {"acceptInsecureCerts": True}
    secure_tls = {"acceptInsecureCerts": False}

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.delete_session()
        self.capabilities = self.marionette.start_session(
            {"requiredCapabilities": self.insecure_tls})

    def tearDown(self):
        try:
            self.marionette.delete_session()
        except:
            pass
        MarionetteTestCase.tearDown(self)

    @contextlib.contextmanager
    def safe_session(self):
        try:
            self.capabilities = self.marionette.start_session(
                {"requiredCapabilities": self.secure_tls})
            self.assertFalse(self.capabilities["acceptInsecureCerts"])
            yield self.marionette
        finally:
            self.marionette.delete_session()

    @contextlib.contextmanager
    def unsafe_session(self):
        try:
            self.capabilities = self.marionette.start_session(
                {"requiredCapabilities": self.insecure_tls})
            self.assertTrue(self.capabilities["acceptInsecureCerts"])
            yield self.marionette
        finally:
            self.marionette.delete_session()

    def test_navigate_by_command(self):
        self.marionette.navigate(
            self.fixtures.where_is("/test.html", on="https"))
        self.assertIn("https", self.marionette.get_url())

    def test_navigate_by_click(self):
        link_url = self.fixtures.where_is("/test.html", on="https")
        self.marionette.navigate(
            inline("<a href=%s>https is the future</a>" % link_url))
        self.marionette.find_element(By.TAG_NAME, "a").click()
        self.assertIn("https", self.marionette.get_url())

    def test_deactivation(self):
        invalid_cert_url = self.fixtures.where_is("/test.html", on="https")

        print "with safe session"
        with self.safe_session() as session:
            with self.assertRaises(errors.InsecureCertificateException):
                session.navigate(invalid_cert_url)

        print "with unsafe session"
        with self.unsafe_session() as session:
            session.navigate(invalid_cert_url)

        print "with safe session again"
        with self.safe_session() as session:
            with self.assertRaises(errors.InsecureCertificateException):
                session.navigate(invalid_cert_url)
