# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import contextlib
import time
import urllib

from marionette_driver import By, errors, expected, Wait
from marionette_harness import (
    MarionetteTestCase,
    run_if_e10s,
    run_if_manage_instance,
    skip,
    skip_if_mobile,
    WindowManagerMixin,
)


def inline(doc):
    return "data:text/html;charset=utf-8,%s" % urllib.quote(doc)


class TestBackForwardNavigation(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestBackForwardNavigation, self).setUp()

        self.test_page = self.marionette.absolute_url('test.html')

        def open_with_link():
            link = self.marionette.find_element(By.ID, "new-blank-tab")
            link.click()

        # Always use a blank new tab for an empty history
        self.marionette.navigate(self.marionette.absolute_url("windowHandles.html"))
        self.new_tab = self.open_tab(open_with_link)
        self.marionette.switch_to_window(self.new_tab)
        self.assertEqual(self.history_length, 1)

    def tearDown(self):
        self.marionette.switch_to_parent_frame()
        self.close_all_tabs()

        super(TestBackForwardNavigation, self).tearDown()

    @property
    def history_length(self):
        return self.marionette.execute_script("return window.history.length;")

    def run_test(self, test_pages):
        # Helper method to run simple back and forward testcases.
        for index, page in enumerate(test_pages):
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.navigate(page["url"])
            else:
                self.marionette.navigate(page["url"])
            self.assertEqual(page["url"], self.marionette.get_url())
            self.assertEqual(self.history_length, index + 1)

        for page in test_pages[-2::-1]:
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.go_back()
            else:
                self.marionette.go_back()
            self.assertEqual(page["url"], self.marionette.get_url())

        for page in test_pages[1::]:
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.go_forward()
            else:
                self.marionette.go_forward()
            self.assertEqual(page["url"], self.marionette.get_url())

    def test_no_history_items(self):
        # Both methods should not raise a failure if no navigation is possible
        self.marionette.go_back()
        self.marionette.go_forward()

    def test_data_urls(self):
        test_pages = [
            {"url": inline("<p>foobar</p>")},
            {"url": self.test_page},
            {"url": inline("<p>foobar</p>")},
        ]
        self.run_test(test_pages)

    def test_same_document_hash_change(self):
        test_pages = [
            {"url": "{}#23".format(self.test_page)},
            {"url": self.test_page},
            {"url": "{}#42".format(self.test_page)},
        ]
        self.run_test(test_pages)

    @skip("Causes crashes for JS GC (bug 1344863) and a11y (bug 1344868)")
    def test_frameset(self):
        test_pages = [
            {"url": self.marionette.absolute_url("frameset.html")},
            {"url": self.test_page},
            {"url": self.marionette.absolute_url("frameset.html")},
        ]
        self.run_test(test_pages)

    def test_frameset_after_navigating_in_frame(self):
        test_element_locator = (By.ID, "email")

        self.marionette.navigate(self.test_page)
        self.assertEqual(self.marionette.get_url(), self.test_page)
        self.assertEqual(self.history_length, 1)
        page = self.marionette.absolute_url("frameset.html")
        self.marionette.navigate(page)
        self.assertEqual(self.marionette.get_url(), page)
        self.assertEqual(self.history_length, 2)
        frame = self.marionette.find_element(By.ID, "fifth")
        self.marionette.switch_to_frame(frame)
        link = self.marionette.find_element(By.ID, "linkId")
        link.click()

        # We cannot use get_url() to wait until the target page has been loaded,
        # because it will return the URL of the top browsing context and doesn't
        # wait for the page load to be complete.
        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            expected.element_present(*test_element_locator),
            message="Target element 'email' has not been found")
        self.assertEqual(self.history_length, 3)

        # Go back to the frame the click navigated away from
        self.marionette.go_back()
        self.assertEqual(self.marionette.get_url(), page)
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette.find_element(*test_element_locator)

        # Go back to the non-frameset page
        self.marionette.switch_to_parent_frame()
        self.marionette.go_back()
        self.assertEqual(self.marionette.get_url(), self.test_page)

        # Go forward to the frameset page
        self.marionette.go_forward()
        self.assertEqual(self.marionette.get_url(), page)

        # Go forward to the frame the click navigated to
        # TODO: See above for automatic browser context switches. Hard to do here
        frame = self.marionette.find_element(By.ID, "fifth")
        self.marionette.switch_to_frame(frame)
        self.marionette.go_forward()
        self.marionette.find_element(*test_element_locator)
        self.assertEqual(self.marionette.get_url(), page)

    def test_image_document_to_html(self):
        test_pages = [
            {"url": self.marionette.absolute_url('black.png')},
            {"url": self.test_page},
            {"url": self.marionette.absolute_url('white.png')},
        ]
        self.run_test(test_pages)

    def test_image_document_to_image_document(self):
        test_pages = [
            {"url": self.marionette.absolute_url('black.png')},
            {"url": self.marionette.absolute_url('white.png')},
        ]
        self.run_test(test_pages)

    @run_if_e10s("Requires e10s mode enabled")
    def test_remoteness_change(self):
        # TODO: Verify that a remoteness change happened
        # like: self.assertNotEqual(self.marionette.current_window_handle, self.new_tab)

        # about:robots is always a non-remote page for now
        test_pages = [
            {"url": "about:robots"},
            {"url": self.test_page},
            {"url": "about:robots"},
        ]
        self.run_test(test_pages)

    def test_navigate_to_requested_about_page_after_error_page(self):
        test_pages = [
            {"url": "about:neterror"},
            {"url": self.marionette.absolute_url("test.html")},
            {"url": "about:blocked"},
        ]
        self.run_test(test_pages)

    def test_timeout_error(self):
        urls = [
            self.marionette.absolute_url('slow'),
            self.test_page,
            self.marionette.absolute_url('slow'),
        ]

        # First, load all pages completely to get them added to the cache
        for index, url in enumerate(urls):
            self.marionette.navigate(url)
            self.assertEqual(url, self.marionette.get_url())
            self.assertEqual(self.history_length, index + 1)

        self.marionette.go_back()
        self.assertEqual(urls[1], self.marionette.get_url())

        # Force triggering a timeout error
        self.marionette.timeout.page_load = 0.1
        with self.assertRaises(errors.TimeoutException):
            self.marionette.go_back()
        self.assertEqual(urls[0], self.marionette.get_url())
        self.marionette.timeout.page_load = 300000

        self.marionette.go_forward()
        self.assertEqual(urls[1], self.marionette.get_url())

        # Force triggering a timeout error
        self.marionette.timeout.page_load = 0.1
        with self.assertRaises(errors.TimeoutException):
            self.marionette.go_forward()
        self.assertEqual(urls[2], self.marionette.get_url())
        self.marionette.timeout.page_load = 300000

    def test_certificate_error(self):
        test_pages = [
            {"url": self.fixtures.where_is("/test.html", on="https"),
             "error": errors.InsecureCertificateException},
            {"url": self.test_page},
            {"url": self.fixtures.where_is("/test.html", on="https"),
             "error": errors.InsecureCertificateException},
        ]
        self.run_test(test_pages)


class TestNavigate(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestNavigate, self).setUp()

        self.marionette.navigate("about:")
        self.test_doc = self.marionette.absolute_url("test.html")
        self.iframe_doc = self.marionette.absolute_url("test_iframe.html")

    def tearDown(self):
        self.marionette.timeout.reset()
        self.close_all_tabs()

        super(TestNavigate, self).tearDown()

    @property
    def location_href(self):
        # Windows 8 has recently seen a proliferation of intermittent
        # test failures to do with failing to compare "about:blank" ==
        # u"about:blank". For the sake of consistenty, we encode the
        # returned URL as Unicode here to ensure that the values are
        # absolutely of the same type.
        #
        # (https://bugzilla.mozilla.org/show_bug.cgi?id=1322862)
        return self.marionette.execute_script("return window.location.href").encode("utf-8")

    def test_set_location_through_execute_script(self):
        self.marionette.execute_script(
            "window.location.href = '%s'" % self.test_doc)
        Wait(self.marionette).until(
            lambda _: self.test_doc == self.location_href)
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

    def test_navigate_in_child_frame_changes_to_top(self):
        frame_html = self.marionette.absolute_url("frameset.html")

        self.marionette.navigate(frame_html)
        frame = self.marionette.find_element(By.NAME, "third")
        self.marionette.switch_to_frame(frame)
        self.assertRaises(errors.NoSuchElementException,
                          self.marionette.find_element, By.NAME, "third")

        self.marionette.navigate(frame_html)
        self.marionette.find_element(By.NAME, "third")

    @skip_if_mobile("Bug 1323755 - Socket timeout")
    def test_invalid_protocol(self):
        with self.assertRaises(errors.MarionetteException):
            self.marionette.navigate("thisprotocoldoesnotexist://")

    def test_find_element_state_complete(self):
        self.marionette.navigate(self.test_doc)
        state = self.marionette.execute_script(
            "return window.document.readyState")
        self.assertEqual("complete", state)
        self.assertTrue(self.marionette.find_element(By.ID, "mozLink"))

    def test_error_when_exceeding_page_load_timeout(self):
        self.marionette.timeout.page_load = 0.1
        with self.assertRaises(errors.TimeoutException):
            self.marionette.navigate(self.marionette.absolute_url("slow"))

    def test_navigate_to_same_image_document_twice(self):
        self.marionette.navigate(self.fixtures.where_is("black.png"))
        self.assertIn("black.png", self.marionette.title)
        self.marionette.navigate(self.fixtures.where_is("black.png"))
        self.assertIn("black.png", self.marionette.title)

    def test_navigate_hash_change(self):
        doc = inline("<p id=foo>")
        self.marionette.navigate(doc)
        self.marionette.execute_script("window.visited = true", sandbox=None)
        self.marionette.navigate("{}#foo".format(doc))
        self.assertTrue(self.marionette.execute_script(
            "return window.visited", sandbox=None))

    @skip_if_mobile("Bug 1334095 - Timeout: No new tab has been opened")
    def test_about_blank_for_new_docshell(self):
        """ Bug 1312674 - Hang when loading about:blank for a new docshell."""
        def open_with_link():
            link = self.marionette.find_element(By.ID, "new-blank-tab")
            link.click()

        # Open a new tab to get a new docshell created
        self.marionette.navigate(self.marionette.absolute_url("windowHandles.html"))
        new_tab = self.open_tab(trigger=open_with_link)
        self.marionette.switch_to_window(new_tab)
        self.assertEqual(self.marionette.get_url(), "about:blank")

        self.marionette.navigate('about:blank')
        self.marionette.close()
        self.marionette.switch_to_window(self.start_window)

    @run_if_manage_instance("Only runnable if Marionette manages the instance")
    @skip_if_mobile("Bug 1322993 - Missing temporary folder")
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
