# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import contextlib
import os
import urllib

from marionette_driver import By, errors, expected, Wait
from marionette_driver.keys import Keys
from marionette_harness import (
    MarionetteTestCase,
    run_if_e10s,
    run_if_manage_instance,
    skip,
    skip_if_mobile,
    WindowManagerMixin,
)

here = os.path.abspath(os.path.dirname(__file__))


def inline(doc):
    return "data:text/html;charset=utf-8,%s" % urllib.quote(doc)


class BaseNavigationTestCase(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(BaseNavigationTestCase, self).setUp()

        file_path = os.path.join(here, 'data', 'test.html').replace("\\", "/")

        self.test_page_file_url = "file:///{}".format(file_path)
        self.test_page_frameset = self.marionette.absolute_url("frameset.html")
        self.test_page_insecure = self.fixtures.where_is("test.html", on="https")
        self.test_page_not_remote = "about:robots"
        self.test_page_remote = self.marionette.absolute_url("test.html")
        self.test_page_slow_resource = self.marionette.absolute_url("slow_resource.html")

        if self.marionette.session_capabilities["platformName"] == "darwin":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        def open_with_link():
            link = self.marionette.find_element(By.ID, "new-blank-tab")
            link.click()

        # Always use a blank new tab for an empty history
        self.marionette.navigate(self.marionette.absolute_url("windowHandles.html"))
        self.new_tab = self.open_tab(open_with_link)
        self.marionette.switch_to_window(self.new_tab)
        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda _: self.history_length == 1,
            message="The newly opened tab doesn't have a browser history length of 1")

    def tearDown(self):
        self.marionette.timeout.reset()
        self.marionette.switch_to_parent_frame()

        self.close_all_tabs()

        super(BaseNavigationTestCase, self).tearDown()

    @property
    def history_length(self):
        return self.marionette.execute_script("return window.history.length;")

    @property
    def is_remote_tab(self):
        with self.marionette.using_context("chrome"):
            # TODO: DO NOT USE MOST RECENT WINDOW BUT CURRENT ONE
            return self.marionette.execute_script("""
              Components.utils.import("resource://gre/modules/AppConstants.jsm");

              let win = null;

              if (AppConstants.MOZ_APP_NAME == "fennec") {
                Components.utils.import("resource://gre/modules/Services.jsm");
                win = Services.wm.getMostRecentWindow("navigator:browser");
              } else {
                Components.utils.import("resource:///modules/RecentWindow.jsm");
                win = RecentWindow.getMostRecentBrowserWindow();
              }

              let tabBrowser = null;

              // Fennec
              if (win.BrowserApp) {
                tabBrowser = win.BrowserApp.selectedBrowser;

              // Firefox
              } else if (win.gBrowser) {
                tabBrowser = win.gBrowser.selectedBrowser;

              } else {
                return null;
              }

              return tabBrowser.isRemoteBrowser;
            """)

    @property
    def ready_state(self):
        return self.marionette.execute_script("return window.document.readyState;",
                                              sandbox=None)


class TestNavigate(BaseNavigationTestCase):

    def test_set_location_through_execute_script(self):
        self.marionette.execute_script(
            "window.location.href = arguments[0];",
            script_args=(self.test_page_remote,), sandbox=None)

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: self.test_page_remote == mn.get_url(),
            message="'{}' hasn't been loaded".format(self.test_page_remote))
        self.assertEqual("Marionette Test", self.marionette.title)

    def test_navigate_chrome_unsupported_error(self):
        with self.marionette.using_context("chrome"):
            self.assertRaises(errors.UnsupportedOperationException,
                              self.marionette.navigate, "about:blank")
            self.assertRaises(errors.UnsupportedOperationException, self.marionette.go_back)
            self.assertRaises(errors.UnsupportedOperationException, self.marionette.go_forward)
            self.assertRaises(errors.UnsupportedOperationException, self.marionette.refresh)

    def test_get_current_url_returns_top_level_browsing_context_url(self):
        page_iframe = self.marionette.absolute_url("test_iframe.html")

        self.marionette.navigate(page_iframe)
        self.assertEqual(page_iframe, self.marionette.get_url())
        frame = self.marionette.find_element(By.CSS_SELECTOR, "#test_iframe")
        self.marionette.switch_to_frame(frame)
        self.assertEqual(page_iframe, self.marionette.get_url())

    def test_get_current_url(self):
        self.marionette.navigate(self.test_page_remote)
        self.assertEqual(self.test_page_remote, self.marionette.get_url())
        self.marionette.navigate("about:blank")
        self.assertEqual("about:blank", self.marionette.get_url())

    def test_navigate_in_child_frame_changes_to_top(self):
        self.marionette.navigate(self.test_page_frameset)
        frame = self.marionette.find_element(By.NAME, "third")
        self.marionette.switch_to_frame(frame)
        self.assertRaises(errors.NoSuchElementException,
                          self.marionette.find_element, By.NAME, "third")

        self.marionette.navigate(self.test_page_frameset)
        self.marionette.find_element(By.NAME, "third")

    def test_invalid_url(self):
        with self.assertRaises(errors.MarionetteException):
            self.marionette.navigate("foo")
        with self.assertRaises(errors.MarionetteException):
            self.marionette.navigate("thisprotocoldoesnotexist://")

    def test_find_element_state_complete(self):
        self.marionette.navigate(self.test_page_remote)
        self.assertEqual("complete", self.ready_state)
        self.assertTrue(self.marionette.find_element(By.ID, "mozLink"))

    def test_navigate_timeout_error_no_remoteness_change(self):
        is_remote_before_timeout = self.is_remote_tab
        self.marionette.timeout.page_load = 0.5
        with self.assertRaises(errors.TimeoutException):
            self.marionette.navigate(self.marionette.absolute_url("slow"))
        self.assertEqual(self.is_remote_tab, is_remote_before_timeout)

    @run_if_e10s("Requires e10s mode enabled")
    def test_navigate_timeout_error_remoteness_change(self):
        self.assertTrue(self.is_remote_tab)
        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab)

        self.marionette.timeout.page_load = 0.5
        with self.assertRaises(errors.TimeoutException):
            self.marionette.navigate(self.marionette.absolute_url("slow"))

        # Even with the page not finished loading the browser is remote
        self.assertTrue(self.is_remote_tab)

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

    def test_navigate_hash_argument_identical(self):
        test_page = "{}#foo".format(inline("<p id=foo>"))

        self.marionette.navigate(test_page)
        self.marionette.find_element(By.ID, "foo")
        self.marionette.navigate(test_page)
        self.marionette.find_element(By.ID, "foo")

    def test_navigate_hash_argument_differnt(self):
        test_page = "{}#Foo".format(inline("<p id=foo>"))

        self.marionette.navigate(test_page)
        self.marionette.find_element(By.ID, "foo")
        self.marionette.navigate(test_page.lower())
        self.marionette.find_element(By.ID, "foo")

    @skip_if_mobile("Test file is only located on host machine")
    def test_navigate_file_url(self):
        self.marionette.navigate(self.test_page_file_url)
        self.marionette.find_element(By.ID, "file-url")
        self.marionette.navigate(self.test_page_remote)

    @run_if_e10s("Requires e10s mode enabled")
    @skip_if_mobile("Test file is only located on host machine")
    def test_navigate_file_url_remoteness_change(self):
        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab)

        self.marionette.navigate(self.test_page_file_url)
        self.assertTrue(self.is_remote_tab)
        self.marionette.find_element(By.ID, "file-url")

        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab)

    @skip_if_mobile("Bug 1334095 - Timeout: No new tab has been opened")
    def test_about_blank_for_new_docshell(self):
        self.assertEqual(self.marionette.get_url(), "about:blank")

        self.marionette.navigate("about:blank")

    @skip("Bug 1332064 - NoSuchElementException: Unable to locate element: :focus")
    @run_if_manage_instance("Only runnable if Marionette manages the instance")
    @skip_if_mobile("Bug 1322993 - Missing temporary folder")
    def test_focus_after_navigation(self):
        self.marionette.quit()
        self.marionette.start_session()

        self.marionette.navigate(inline("<input autofocus>"))
        focus_el = self.marionette.find_element(By.CSS_SELECTOR, ":focus")
        self.assertEqual(self.marionette.get_active_element(), focus_el)

    @skip_if_mobile("Needs application independent method to open a new tab")
    def test_no_hang_when_navigating_after_closing_original_tab(self):
        # Close the start tab
        self.marionette.switch_to_window(self.start_tab)
        self.marionette.close()

        self.marionette.switch_to_window(self.new_tab)
        self.marionette.navigate(self.test_page_remote)

    @skip_if_mobile("Interacting with chrome elements not available for Fennec")
    def test_type_to_non_remote_tab(self):
        self.marionette.navigate(self.test_page_not_remote)
        self.assertFalse(self.is_remote_tab)

        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element(By.ID, "urlbar")
            urlbar.send_keys(self.mod_key + "a")
            urlbar.send_keys(self.mod_key + "x")
            urlbar.send_keys("about:support" + Keys.ENTER)

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: mn.get_url() == "about:support",
            message="'about:support' hasn't been loaded")
        self.assertFalse(self.is_remote_tab)

    @skip_if_mobile("Interacting with chrome elements not available for Fennec")
    @run_if_e10s("Requires e10s mode enabled")
    def test_type_to_remote_tab(self):
        self.assertTrue(self.is_remote_tab)

        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element(By.ID, "urlbar")
            urlbar.send_keys(self.mod_key + "a")
            urlbar.send_keys(self.mod_key + "x")
            urlbar.send_keys(self.test_page_remote + Keys.ENTER)

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: mn.get_url() == self.test_page_remote,
            message="'{}' hasn't been loaded".format(self.test_page_remote))
        self.assertTrue(self.is_remote_tab)

    @skip_if_mobile("On Android no shortcuts are available")
    def test_navigate_shortcut_key(self):

        def open_with_shortcut():
            self.marionette.navigate(self.test_page_remote)
            with self.marionette.using_context("chrome"):
                main_win = self.marionette.find_element(By.ID, "main-window")
                main_win.send_keys(self.mod_key, Keys.SHIFT, "a")

        new_tab = self.open_tab(trigger=open_with_shortcut)
        self.marionette.switch_to_window(new_tab)

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: mn.get_url() == "about:addons",
            message="'about:addons' hasn't been loaded")


class TestBackForwardNavigation(BaseNavigationTestCase):

    def run_bfcache_test(self, test_pages):
        # Helper method to run simple back and forward testcases.
        for index, page in enumerate(test_pages):
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.navigate(page["url"])
            else:
                self.marionette.navigate(page["url"])
            self.assertEqual(page["url"], self.marionette.get_url())
            self.assertEqual(self.history_length, index + 1)

            if "is_remote" in page:
                self.assertEqual(page["is_remote"], self.is_remote_tab,
                                 "'{}' doesn't match expected remoteness state: {}".format(
                                     page["url"], page["is_remote"]))

        # Now going back in history for all test pages by backward iterating
        # through the list (-1) and skipping the first entry at the end (-2).
        for page in test_pages[-2::-1]:
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.go_back()
            else:
                self.marionette.go_back()
            self.assertEqual(page["url"], self.marionette.get_url())

        if "is_remote" in page:
            self.assertEqual(page["is_remote"], self.is_remote_tab,
                             "'{}' doesn't match expected remoteness state: {}".format(
                                 page["url"], page["is_remote"]))

        # Now going forward in history by skipping the first entry.
        for page in test_pages[1::]:
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.go_forward()
            else:
                self.marionette.go_forward()
            self.assertEqual(page["url"], self.marionette.get_url())

        if "is_remote" in page:
            self.assertEqual(page["is_remote"], self.is_remote_tab,
                             "'{}' doesn't match expected remoteness state: {}".format(
                                 page["url"], page["is_remote"]))

    def test_no_history_items(self):
        # Both methods should not raise a failure if no navigation is possible
        self.marionette.go_back()
        self.marionette.go_forward()

    def test_data_urls(self):
        test_pages = [
            {"url": inline("<p>foobar</p>")},
            {"url": self.test_page_remote},
            {"url": inline("<p>foobar</p>")},
        ]
        self.run_bfcache_test(test_pages)

    def test_same_document_hash_change(self):
        test_pages = [
            {"url": "{}#23".format(self.test_page_remote)},
            {"url": self.test_page_remote},
            {"url": "{}#42".format(self.test_page_remote)},
        ]
        self.run_bfcache_test(test_pages)

    @skip_if_mobile("Test file is only located on host machine")
    def test_file_url(self):
        test_pages = [
            {"url": self.test_page_remote},
            {"url": self.test_page_file_url},
            {"url": self.test_page_remote},
        ]
        self.run_bfcache_test(test_pages)

    @skip("Causes crashes for JS GC (bug 1344863) and a11y (bug 1344868)")
    def test_frameset(self):
        test_pages = [
            {"url": self.marionette.absolute_url("frameset.html")},
            {"url": self.test_page_remote},
            {"url": self.marionette.absolute_url("frameset.html")},
        ]
        self.run_bfcache_test(test_pages)

    def test_frameset_after_navigating_in_frame(self):
        test_element_locator = (By.ID, "email")

        self.marionette.navigate(self.test_page_remote)
        self.assertEqual(self.marionette.get_url(), self.test_page_remote)
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
        self.assertEqual(self.marionette.get_url(), self.test_page_remote)

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

    def test_image_to_html_to_image(self):
        test_pages = [
            {"url": self.marionette.absolute_url("black.png")},
            {"url": self.test_page_remote},
            {"url": self.marionette.absolute_url("white.png")},
        ]
        self.run_bfcache_test(test_pages)

    def test_image_to_image(self):
        test_pages = [
            {"url": self.marionette.absolute_url("black.png")},
            {"url": self.marionette.absolute_url("white.png")},
        ]
        self.run_bfcache_test(test_pages)

    @run_if_e10s("Requires e10s mode enabled")
    def test_remoteness_change(self):
        test_pages = [
            {"url": "about:robots", "is_remote": False},
            {"url": self.test_page_remote, "is_remote": True},
            {"url": "about:robots", "is_remote": False},
        ]
        self.run_bfcache_test(test_pages)

    @skip_if_mobile("Bug 1333209 - Process killed because of connection loss")
    def test_non_remote_about_pages(self):
        test_pages = [
            {"url": "about:preferences", "is_remote": False},
            {"url": "about:robots", "is_remote": False},
            {"url": "about:support", "is_remote": False},
        ]
        self.run_bfcache_test(test_pages)

    def test_navigate_to_requested_about_page_after_error_page(self):
        test_pages = [
            {"url": "about:neterror"},
            {"url": self.test_page_remote},
            {"url": "about:blocked"},
        ]
        self.run_bfcache_test(test_pages)

    def test_timeout_error(self):
        urls = [
            self.marionette.absolute_url("slow?delay=3"),
            self.test_page_remote,
            self.marionette.absolute_url("slow?delay=4"),
        ]

        # First, load all pages completely to get them added to the cache
        for index, url in enumerate(urls):
            self.marionette.navigate(url)
            self.assertEqual(url, self.marionette.get_url())
            self.assertEqual(self.history_length, index + 1)

        self.marionette.go_back()
        self.assertEqual(urls[1], self.marionette.get_url())

        # Force triggering a timeout error
        self.marionette.timeout.page_load = 0.5
        with self.assertRaises(errors.TimeoutException):
            self.marionette.go_back()
        self.marionette.timeout.reset()

        delay = Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            expected.element_present(By.ID, "delay"),
            message="Target element 'delay' has not been found after timeout in 'back'")
        self.assertEqual(delay.text, "3")

        self.marionette.go_forward()
        self.assertEqual(urls[1], self.marionette.get_url())

        # Force triggering a timeout error
        self.marionette.timeout.page_load = 0.5
        with self.assertRaises(errors.TimeoutException):
            self.marionette.go_forward()
        self.marionette.timeout.reset()

        delay = Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            expected.element_present(By.ID, "delay"),
            message="Target element 'delay' has not been found after timeout in 'forward'")
        self.assertEqual(delay.text, "4")

    def test_certificate_error(self):
        test_pages = [
            {"url": self.test_page_insecure,
             "error": errors.InsecureCertificateException},
            {"url": self.test_page_remote},
            {"url": self.test_page_insecure,
             "error": errors.InsecureCertificateException},
        ]
        self.run_bfcache_test(test_pages)


class TestRefresh(BaseNavigationTestCase):

    def test_basic(self):
        self.marionette.navigate(self.test_page_remote)
        self.assertEqual(self.test_page_remote, self.marionette.get_url())

        self.marionette.execute_script("""
          let elem = window.document.createElement('div');
          elem.id = 'someDiv';
          window.document.body.appendChild(elem);
        """)
        self.marionette.find_element(By.ID, "someDiv")

        self.marionette.refresh()
        self.assertEqual(self.test_page_remote, self.marionette.get_url())
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette.find_element(By.ID, "someDiv")

    def test_refresh_in_child_frame_navigates_to_top(self):
        self.marionette.navigate(self.test_page_frameset)
        self.assertEqual(self.test_page_frameset, self.marionette.get_url())

        frame = self.marionette.find_element(By.NAME, "third")
        self.marionette.switch_to_frame(frame)
        self.assertRaises(errors.NoSuchElementException,
                          self.marionette.find_element, By.NAME, "third")

        self.marionette.refresh()
        self.marionette.find_element(By.NAME, "third")

    @skip_if_mobile("Test file is only located on host machine")
    def test_file_url(self):
        self.marionette.navigate(self.test_page_file_url)
        self.assertEqual(self.test_page_file_url, self.marionette.get_url())

        self.marionette.refresh()
        self.assertEqual(self.test_page_file_url, self.marionette.get_url())

    def test_image(self):
        image = self.marionette.absolute_url('black.png')

        self.marionette.navigate(image)
        self.assertEqual(image, self.marionette.get_url())

        self.marionette.refresh()
        self.assertEqual(image, self.marionette.get_url())

    def test_timeout_error(self):
        slow_page = self.marionette.absolute_url("slow?delay=3")

        self.marionette.navigate(slow_page)
        self.assertEqual(slow_page, self.marionette.get_url())

        self.marionette.timeout.page_load = 0.5
        with self.assertRaises(errors.TimeoutException):
            self.marionette.refresh()
        self.assertEqual(slow_page, self.marionette.get_url())

    def test_insecure_error(self):
        with self.assertRaises(errors.InsecureCertificateException):
            self.marionette.navigate(self.test_page_insecure)
        self.assertEqual(self.test_page_insecure, self.marionette.get_url())

        with self.assertRaises(errors.InsecureCertificateException):
            self.marionette.refresh()


class TestTLSNavigation(MarionetteTestCase):
    insecure_tls = {"acceptInsecureCerts": True}
    secure_tls = {"acceptInsecureCerts": False}

    def setUp(self):
        super(TestTLSNavigation, self).setUp()

        self.test_page_insecure = self.fixtures.where_is("test.html", on="https")

        self.marionette.delete_session()
        self.capabilities = self.marionette.start_session(
            {"requiredCapabilities": self.insecure_tls})

    def tearDown(self):
        try:
            self.marionette.delete_session()
        except:
            pass

        super(TestTLSNavigation, self).tearDown()

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
        self.marionette.navigate(self.test_page_insecure)
        self.assertIn("https", self.marionette.get_url())

    def test_navigate_by_click(self):
        link_url = self.test_page_insecure
        self.marionette.navigate(
            inline("<a href=%s>https is the future</a>" % link_url))
        self.marionette.find_element(By.TAG_NAME, "a").click()
        self.assertIn("https", self.marionette.get_url())

    def test_deactivation(self):
        invalid_cert_url = self.test_page_insecure

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


class TestPageLoadStrategy(BaseNavigationTestCase):

    def tearDown(self):
        self.marionette.delete_session()
        self.marionette.start_session()

        super(TestPageLoadStrategy, self).tearDown()

    def test_none(self):
        self.marionette.delete_session()
        self.marionette.start_session({"desiredCapabilities": {"pageLoadStrategy": "none"}})

        # With a strategy of "none" there should be no wait for the page load, and the
        # current load state is unknown. So only test that the command executes successfully.
        self.marionette.navigate(self.test_page_slow_resource)

    def test_eager(self):
        self.marionette.delete_session()
        self.marionette.start_session({"desiredCapabilities": {"pageLoadStrategy": "eager"}})

        self.marionette.navigate(self.test_page_slow_resource)
        self.assertEqual("interactive", self.ready_state)
        self.assertEqual(self.test_page_slow_resource, self.marionette.get_url())
        self.marionette.find_element(By.ID, "slow")

    def test_normal(self):
        self.marionette.delete_session()
        self.marionette.start_session({"desiredCapabilities": {"pageLoadStrategy": "normal"}})

        self.marionette.navigate(self.test_page_slow_resource)
        self.assertEqual(self.test_page_slow_resource, self.marionette.get_url())
        self.assertEqual("complete", self.ready_state)
        self.marionette.find_element(By.ID, "slow")

    @run_if_e10s("Requires e10s mode enabled")
    def test_strategy_after_remoteness_change(self):
        """Bug 1378191 - Reset of capabilities after listener reload"""
        self.marionette.delete_session()
        self.marionette.start_session({"desiredCapabilities": {"pageLoadStrategy": "eager"}})

        # Trigger a remoteness change which will reload the listener script
        self.assertTrue(self.is_remote_tab, "Initial tab doesn't have remoteness flag set")
        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab, "Tab has remoteness flag set")
        self.marionette.navigate(self.test_page_slow_resource)
        self.assertEqual("interactive", self.ready_state)
