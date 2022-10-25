# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import contextlib
import os

from six.moves.urllib.parse import quote

from marionette_driver import By, errors, expected, Wait
from marionette_driver.keys import Keys
from marionette_driver.marionette import Alert
from marionette_harness import (
    MarionetteTestCase,
    run_if_manage_instance,
    WindowManagerMixin,
)

here = os.path.abspath(os.path.dirname(__file__))


BLACK_PIXEL = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg=="  # noqa
RED_PIXEL = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEX/TQBcNTh/AAAAAXRSTlPM0jRW/QAAAApJREFUeJxjYgAAAAYAAzY3fKgAAAAASUVORK5CYII="  # noqa


def inline(doc):
    return "data:text/html;charset=utf-8,%s" % quote(doc)


def inline_image(data):
    return "data:image/png;base64,%s" % data


class BaseNavigationTestCase(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(BaseNavigationTestCase, self).setUp()

        file_path = os.path.join(here, "data", "test.html").replace("\\", "/")

        self.test_page_file_url = "file:///{}".format(file_path)
        self.test_page_frameset = self.marionette.absolute_url("frameset.html")
        self.test_page_insecure = self.fixtures.where_is("test.html", on="https")
        self.test_page_not_remote = "about:robots"
        self.test_page_push_state = self.marionette.absolute_url(
            "navigation_pushstate.html"
        )
        self.test_page_remote = self.marionette.absolute_url("test.html")

        if self.marionette.session_capabilities["platformName"] == "mac":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        # Always use a blank new tab for an empty history
        self.new_tab = self.open_tab()
        self.marionette.switch_to_window(self.new_tab)
        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda _: self.history_length == 1,
            message="The newly opened tab doesn't have a browser history length of 1",
        )

    def tearDown(self):
        self.marionette.timeout.reset()

        self.close_all_tabs()

        super(BaseNavigationTestCase, self).tearDown()

    @property
    def history_length(self):
        return self.marionette.execute_script("return window.history.length;")

    @property
    def is_remote_tab(self):
        with self.marionette.using_context("chrome"):
            # TODO: DO NOT USE MOST RECENT WINDOW BUT CURRENT ONE
            return self.marionette.execute_script(
                """
              const { AppConstants } = ChromeUtils.import(
                "resource://gre/modules/AppConstants.jsm"
              );

              let win = null;

              if (AppConstants.MOZ_APP_NAME == "fennec") {
                win = Services.wm.getMostRecentWindow("navigator:browser");
              } else {
                const { BrowserWindowTracker } = ChromeUtils.import(
                  "resource:///modules/BrowserWindowTracker.jsm"
                );
                win = BrowserWindowTracker.getTopWindow();
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
            """
            )

    @property
    def ready_state(self):
        return self.marionette.execute_script(
            "return window.document.readyState;", sandbox=None
        )


class TestNavigate(BaseNavigationTestCase):
    def test_set_location_through_execute_script(self):
        # To avoid unexpected remoteness changes and a hang in any non-navigation
        # command (bug 1519354) when navigating via the location bar, already
        # pre-load a page which causes a remoteness change.
        self.marionette.navigate(self.test_page_push_state)

        self.marionette.execute_script(
            "window.location.href = arguments[0];",
            script_args=(self.test_page_remote,),
            sandbox=None,
        )

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            expected.element_present(*(By.ID, "testh1")),
            message="Target element 'testh1' has not been found",
        )

        self.assertEqual(self.test_page_remote, self.marionette.get_url())

    def test_navigate_chrome_unsupported_error(self):
        with self.marionette.using_context("chrome"):
            self.assertRaises(
                errors.UnsupportedOperationException,
                self.marionette.navigate,
                "about:blank",
            )
            self.assertRaises(
                errors.UnsupportedOperationException, self.marionette.go_back
            )
            self.assertRaises(
                errors.UnsupportedOperationException, self.marionette.go_forward
            )
            self.assertRaises(
                errors.UnsupportedOperationException, self.marionette.refresh
            )

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
        self.assertRaises(
            errors.NoSuchElementException,
            self.marionette.find_element,
            By.NAME,
            "third",
        )

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

    def test_navigate_timeout_error_remoteness_change(self):
        self.assertTrue(self.is_remote_tab)
        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab)

        self.marionette.timeout.page_load = 0.5
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
        self.assertTrue(
            self.marionette.execute_script("return window.visited", sandbox=None)
        )

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

    def test_navigate_history_pushstate(self):
        target_page = self.marionette.absolute_url("navigation_pushstate_target.html")

        self.marionette.navigate(self.test_page_push_state)
        self.marionette.find_element(By.ID, "forward").click()

        # By using pushState() the URL is updated but the target page is not loaded
        # and as such the element is not displayed
        self.assertEqual(self.marionette.get_url(), target_page)
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette.find_element(By.ID, "target")

        self.marionette.go_back()
        self.assertEqual(self.marionette.get_url(), self.test_page_push_state)

        # The target page still gets not loaded
        self.marionette.go_forward()
        self.assertEqual(self.marionette.get_url(), target_page)
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette.find_element(By.ID, "target")

        # Navigating to a different page, and returning to the injected
        # page, it will be loaded.
        self.marionette.navigate(self.test_page_remote)
        self.assertEqual(self.marionette.get_url(), self.test_page_remote)

        self.marionette.go_back()
        self.assertEqual(self.marionette.get_url(), target_page)
        self.marionette.find_element(By.ID, "target")

        self.marionette.go_back()
        self.assertEqual(self.marionette.get_url(), self.test_page_push_state)

    def test_navigate_file_url(self):
        self.marionette.navigate(self.test_page_file_url)
        self.marionette.find_element(By.ID, "file-url")
        self.marionette.navigate(self.test_page_remote)

    def test_navigate_file_url_remoteness_change(self):
        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab)

        self.marionette.navigate(self.test_page_file_url)
        self.assertTrue(self.is_remote_tab)
        self.marionette.find_element(By.ID, "file-url")

        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab)

    def test_stale_element_after_remoteness_change(self):
        self.marionette.navigate(self.test_page_file_url)
        self.assertTrue(self.is_remote_tab)
        elem = self.marionette.find_element(By.ID, "file-url")

        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab)

        # Force a GC to get rid of the replaced browsing context.
        with self.marionette.using_context("chrome"):
            self.marionette.execute_async_script(
                """
                const resolve = arguments[0];

                var memSrv = Cc["@mozilla.org/memory-reporter-manager;1"]
                  .getService(Ci.nsIMemoryReporterManager);

                Services.obs.notifyObservers(null, "child-mmu-request", null);
                memSrv.minimizeMemoryUsage(resolve);
            """
            )

        with self.assertRaises(errors.StaleElementException):
            elem.click()

    def test_about_blank_for_new_docshell(self):
        self.assertEqual(self.marionette.get_url(), "about:blank")

        self.marionette.navigate("about:blank")

    def test_about_newtab(self):
        with self.marionette.using_prefs({"browser.newtabpage.enabled": True}):
            self.marionette.navigate("about:newtab")

            self.marionette.navigate(self.test_page_remote)
            self.marionette.find_element(By.ID, "testDiv")

    @run_if_manage_instance("Only runnable if Marionette manages the instance")
    def test_focus_after_navigation(self):
        self.marionette.restart()

        self.marionette.navigate(inline("<input autofocus>"))
        focus_el = self.marionette.find_element(By.CSS_SELECTOR, ":focus")
        self.assertEqual(self.marionette.get_active_element(), focus_el)

    def test_no_hang_when_navigating_after_closing_original_tab(self):
        # Close the start tab
        self.marionette.switch_to_window(self.start_tab)
        self.marionette.close()

        self.marionette.switch_to_window(self.new_tab)
        self.marionette.navigate(self.test_page_remote)

    def test_type_to_non_remote_tab(self):
        self.marionette.navigate(self.test_page_not_remote)
        self.assertFalse(self.is_remote_tab)

        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element(By.ID, "urlbar-input")
            urlbar.send_keys(self.mod_key + "a")
            urlbar.send_keys(self.mod_key + "x")
            urlbar.send_keys("about:support" + Keys.ENTER)

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: mn.get_url() == "about:support",
            message="'about:support' hasn't been loaded",
        )
        self.assertFalse(self.is_remote_tab)

    def test_type_to_remote_tab(self):
        self.assertTrue(self.is_remote_tab)

        with self.marionette.using_context("chrome"):
            urlbar = self.marionette.find_element(By.ID, "urlbar-input")
            urlbar.send_keys(self.mod_key + "a")
            urlbar.send_keys(self.mod_key + "x")
            urlbar.send_keys(self.test_page_remote + Keys.ENTER)

        Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            lambda mn: mn.get_url() == self.test_page_remote,
            message="'{}' hasn't been loaded".format(self.test_page_remote),
        )
        self.assertTrue(self.is_remote_tab)

    def test_navigate_after_deleting_session(self):
        self.marionette.delete_session()
        self.marionette.start_session()

        self.marionette.navigate(self.test_page_remote)
        self.assertEqual(self.test_page_remote, self.marionette.get_url())


class TestBackForwardNavigation(BaseNavigationTestCase):
    def run_bfcache_test(self, test_pages):
        # Helper method to run simple back and forward testcases.

        def check_page_status(page, expected_history_length):
            if "alert_text" in page:
                if page["alert_text"] is None:
                    # navigation auto-dismisses beforeunload prompt
                    with self.assertRaises(errors.NoAlertPresentException):
                        Alert(self.marionette).text
                else:
                    self.assertEqual(Alert(self.marionette).text, page["alert_text"])

            self.assertEqual(self.marionette.get_url(), page["url"])
            self.assertEqual(self.history_length, expected_history_length)

            if "is_remote" in page:
                self.assertEqual(
                    page["is_remote"],
                    self.is_remote_tab,
                    "'{}' doesn't match expected remoteness state: {}".format(
                        page["url"], page["is_remote"]
                    ),
                )

            if "callback" in page and callable(page["callback"]):
                page["callback"]()

        for index, page in enumerate(test_pages):
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.navigate(page["url"])
            else:
                self.marionette.navigate(page["url"])

            check_page_status(page, index + 1)

        # Now going back in history for all test pages by backward iterating
        # through the list (-1) and skipping the first entry at the end (-2).
        for page in test_pages[-2::-1]:
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.go_back()
            else:
                self.marionette.go_back()

            check_page_status(page, len(test_pages))

        # Now going forward in history by skipping the first entry.
        for page in test_pages[1::]:
            if "error" in page:
                with self.assertRaises(page["error"]):
                    self.marionette.go_forward()
            else:
                self.marionette.go_forward()

            check_page_status(page, len(test_pages))

    def test_no_history_items(self):
        # Both methods should not raise a failure if no navigation is possible
        self.marionette.go_back()
        self.marionette.go_forward()

    def test_dismissed_beforeunload_prompt(self):
        url_beforeunload = inline(
            """
          <input type="text">
          <script>
            window.addEventListener("beforeunload", function (event) {
              event.preventDefault();
            });
          </script>
        """
        )

        def modify_page():
            self.marionette.find_element(By.TAG_NAME, "input").send_keys("foo")

        test_pages = [
            {"url": inline("<p>foobar</p>"), "alert_text": None},
            {"url": url_beforeunload, "callback": modify_page},
            {"url": inline("<p>foobar</p>"), "alert_text": None},
        ]

        self.run_bfcache_test(test_pages)

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

    def test_file_url(self):
        test_pages = [
            {"url": self.test_page_remote},
            {"url": self.test_page_file_url},
            {"url": self.test_page_remote},
        ]
        self.run_bfcache_test(test_pages)

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
            message="Target element 'email' has not been found",
        )
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
            {"url": inline_image(RED_PIXEL)},
            {"url": inline_image(BLACK_PIXEL)},
            {"url": self.marionette.absolute_url("black.png")},
        ]
        self.run_bfcache_test(test_pages)

    def test_remoteness_change(self):
        test_pages = [
            {"url": "about:robots", "is_remote": False},
            {"url": self.test_page_remote, "is_remote": True},
            {"url": "about:robots", "is_remote": False},
        ]
        self.run_bfcache_test(test_pages)

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
            message="Target element 'delay' has not been found after timeout in 'back'",
        )
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
            message="Target element 'delay' has not been found after timeout in 'forward'",
        )
        self.assertEqual(delay.text, "4")

    def test_certificate_error(self):
        test_pages = [
            {
                "url": self.test_page_insecure,
                "error": errors.InsecureCertificateException,
            },
            {"url": self.test_page_remote},
            {
                "url": self.test_page_insecure,
                "error": errors.InsecureCertificateException,
            },
        ]
        self.run_bfcache_test(test_pages)


class TestRefresh(BaseNavigationTestCase):
    def test_basic(self):
        self.marionette.navigate(self.test_page_remote)
        self.assertEqual(self.test_page_remote, self.marionette.get_url())

        self.marionette.execute_script(
            """
          let elem = window.document.createElement('div');
          elem.id = 'someDiv';
          window.document.body.appendChild(elem);
        """
        )
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
        self.assertRaises(
            errors.NoSuchElementException,
            self.marionette.find_element,
            By.NAME,
            "third",
        )

        self.marionette.refresh()
        self.marionette.find_element(By.NAME, "third")

    def test_file_url(self):
        self.marionette.navigate(self.test_page_file_url)
        self.assertEqual(self.test_page_file_url, self.marionette.get_url())

        self.marionette.refresh()
        self.assertEqual(self.test_page_file_url, self.marionette.get_url())

    def test_dismissed_beforeunload_prompt(self):
        self.marionette.navigate(
            inline(
                """
          <input type="text">
          <script>
            window.addEventListener("beforeunload", function (event) {
              event.preventDefault();
            });
          </script>
        """
            )
        )
        self.marionette.find_element(By.TAG_NAME, "input").send_keys("foo")
        self.marionette.refresh()

        # navigation auto-dismisses beforeunload prompt
        with self.assertRaises(errors.NoAlertPresentException):
            Alert(self.marionette).text

    def test_image(self):
        image = self.marionette.absolute_url("black.png")

        self.marionette.navigate(image)
        self.assertEqual(image, self.marionette.get_url())

        self.marionette.refresh()
        self.assertEqual(image, self.marionette.get_url())

    def test_history_pushstate(self):
        target_page = self.marionette.absolute_url("navigation_pushstate_target.html")

        self.marionette.navigate(self.test_page_push_state)
        self.marionette.find_element(By.ID, "forward").click()

        # By using pushState() the URL is updated but the target page is not loaded
        # and as such the element is not displayed
        self.assertEqual(self.marionette.get_url(), target_page)
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette.find_element(By.ID, "target")

        # Refreshing the target page will trigger a full page load.
        self.marionette.refresh()
        self.assertEqual(self.marionette.get_url(), target_page)
        self.marionette.find_element(By.ID, "target")

        self.marionette.go_back()
        self.assertEqual(self.marionette.get_url(), self.test_page_push_state)

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
        self.assertEqual(self.marionette.get_url(), self.test_page_insecure)

        with self.assertRaises(errors.InsecureCertificateException):
            self.marionette.refresh()


class TestTLSNavigation(BaseNavigationTestCase):
    insecure_tls = {"acceptInsecureCerts": True}
    secure_tls = {"acceptInsecureCerts": False}

    def setUp(self):
        super(TestTLSNavigation, self).setUp()

        self.test_page_insecure = self.fixtures.where_is("test.html", on="https")

        self.marionette.delete_session()
        self.capabilities = self.marionette.start_session(self.insecure_tls)

    def tearDown(self):
        try:
            self.marionette.delete_session()
            self.marionette.start_session()
        except:
            pass

        super(TestTLSNavigation, self).tearDown()

    @contextlib.contextmanager
    def safe_session(self):
        try:
            self.capabilities = self.marionette.start_session(self.secure_tls)
            self.assertFalse(self.capabilities["acceptInsecureCerts"])
            # Always use a blank new tab for an empty history
            self.new_tab = self.open_tab()
            self.marionette.switch_to_window(self.new_tab)
            Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
                lambda _: self.history_length == 1,
                message="The newly opened tab doesn't have a browser history length of 1",
            )
            yield self.marionette
        finally:
            self.close_all_tabs()
            self.marionette.delete_session()

    @contextlib.contextmanager
    def unsafe_session(self):
        try:
            self.capabilities = self.marionette.start_session(self.insecure_tls)
            self.assertTrue(self.capabilities["acceptInsecureCerts"])
            # Always use a blank new tab for an empty history
            self.new_tab = self.open_tab()
            self.marionette.switch_to_window(self.new_tab)
            Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
                lambda _: self.history_length == 1,
                message="The newly opened tab doesn't have a browser history length of 1",
            )
            yield self.marionette
        finally:
            self.close_all_tabs()
            self.marionette.delete_session()

    def test_navigate_by_command(self):
        self.marionette.navigate(self.test_page_insecure)
        self.assertIn("https", self.marionette.get_url())

    def test_navigate_by_click(self):
        link_url = self.test_page_insecure
        self.marionette.navigate(
            inline("<a href=%s>https is the future</a>" % link_url)
        )
        self.marionette.find_element(By.TAG_NAME, "a").click()
        self.assertIn("https", self.marionette.get_url())

    def test_deactivation(self):
        invalid_cert_url = self.test_page_insecure

        self.marionette.delete_session()

        print("with safe session")
        with self.safe_session() as session:
            with self.assertRaises(errors.InsecureCertificateException):
                session.navigate(invalid_cert_url)

        print("with unsafe session")
        with self.unsafe_session() as session:
            session.navigate(invalid_cert_url)

        print("with safe session again")
        with self.safe_session() as session:
            with self.assertRaises(errors.InsecureCertificateException):
                session.navigate(invalid_cert_url)


class TestPageLoadStrategy(BaseNavigationTestCase):
    def setUp(self):
        super(TestPageLoadStrategy, self).setUp()

        # Test page that delays the response and as such the document to be
        # loaded. It is used for testing the page load strategy "none".
        self.test_page_slow = self.marionette.absolute_url("slow")

        # Similar to "slow" but additionally triggers a cross group navigation
        # which triggers a replacement of the top-level browsing context.
        self.test_page_slow_coop = self.marionette.absolute_url("slow-coop")

        # Test page that contains a slow loading <img> element which delays the
        # "load" but not the "DOMContentLoaded" event.
        self.test_page_slow_resource = self.marionette.absolute_url(
            "slow_resource.html"
        )

    def tearDown(self):
        self.marionette.delete_session()
        self.marionette.start_session()

        super(TestPageLoadStrategy, self).tearDown()

    def test_none(self):
        self.marionette.delete_session()
        self.marionette.start_session({"pageLoadStrategy": "none"})

        # Navigate will return immediately. As such wait for the target URL to
        # be the current location, and the element to exist.
        self.marionette.navigate(self.test_page_slow)
        with self.assertRaises(errors.NoSuchElementException):
            self.marionette.find_element(By.ID, "delay")

        Wait(
            self.marionette,
            ignored_exceptions=errors.NoSuchElementException,
            timeout=self.marionette.timeout.page_load,
        ).until(lambda _: self.marionette.find_element(By.ID, "delay"))

        self.assertEqual(self.marionette.get_url(), self.test_page_slow)

    def test_none_with_new_session_waits_for_page_loaded(self):
        self.marionette.delete_session()
        self.marionette.start_session({"pageLoadStrategy": "none"})

        # Navigate will return immediately.
        self.marionette.navigate(self.test_page_slow)

        # Make sure that when creating a new session right away it waits
        # until the page has been finished loading.
        self.marionette.delete_session()
        self.marionette.start_session()

        self.assertEqual(self.marionette.get_url(), self.test_page_slow)
        self.assertEqual(self.ready_state, "complete")
        self.marionette.find_element(By.ID, "delay")

    def test_none_with_new_session_waits_for_page_loaded_remoteness_change(self):
        self.marionette.delete_session()
        self.marionette.start_session({"pageLoadStrategy": "none"})

        # Navigate will return immediately.
        self.marionette.navigate(self.test_page_slow_coop)

        # Make sure that when creating a new session right away it waits
        # until the page has been finished loading.
        self.marionette.delete_session()
        self.marionette.start_session()

        self.assertEqual(self.marionette.get_url(), self.test_page_slow_coop)
        self.assertEqual(self.ready_state, "complete")
        self.marionette.find_element(By.ID, "delay")

    def test_eager(self):
        self.marionette.delete_session()
        self.marionette.start_session({"pageLoadStrategy": "eager"})

        self.marionette.navigate(self.test_page_slow_resource)
        self.assertEqual(self.ready_state, "interactive")
        self.assertEqual(self.marionette.get_url(), self.test_page_slow_resource)
        self.marionette.find_element(By.ID, "slow")

    def test_normal(self):
        self.marionette.delete_session()
        self.marionette.start_session({"pageLoadStrategy": "normal"})

        self.marionette.navigate(self.test_page_slow_resource)
        self.assertEqual(self.marionette.get_url(), self.test_page_slow_resource)
        self.assertEqual(self.ready_state, "complete")
        self.marionette.find_element(By.ID, "slow")

    def test_strategy_after_remoteness_change(self):
        """Bug 1378191 - Reset of capabilities after listener reload."""
        self.marionette.delete_session()
        self.marionette.start_session({"pageLoadStrategy": "eager"})

        # Trigger a remoteness change which will reload the listener script
        self.assertTrue(
            self.is_remote_tab, "Initial tab doesn't have remoteness flag set"
        )
        self.marionette.navigate("about:robots")
        self.assertFalse(self.is_remote_tab, "Tab has remoteness flag set")
        self.marionette.navigate(self.test_page_slow_resource)
        self.assertEqual(self.ready_state, "interactive")
