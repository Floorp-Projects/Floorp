# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from firefox_puppeteer import Puppeteer
from firefox_puppeteer.ui.browser.window import BrowserWindow


class BaseFirefoxTestCase(unittest.TestCase, Puppeteer):
    """Base TestCase class for Firefox Desktop tests.

    This is designed to enhance MarionetteTestCase by inserting the Puppeteer
    mixin class (so Firefox specific API modules are exposed to test scope) and
    providing common set-up and tear-down code for Firefox tests.

    Child classes are expected to also subclass MarionetteTestCase such that
    MarionetteTestCase is inserted into the MRO after FirefoxTestCase but before
    unittest.TestCase.

    example:
    `class AwesomeTestCase(FirefoxTestCase, MarionetteTestCase)`

    The key role of MarionetteTestCase is to set self.marionette appropriately
    in `__init__`. Any TestCase class that satisfies this requirement is
    compatible with this class.

    If you're extending the inheritance tree further to make specialized
    TestCases, favour the use of super() as opposed to explicit calls to a
    parent class.

    """
    def __init__(self, *args, **kwargs):
        super(BaseFirefoxTestCase, self).__init__(*args, **kwargs)

    def _check_and_fix_leaked_handles(self):
        handle_count = len(self.marionette.window_handles)

        try:
            self.assertEqual(handle_count, self._start_handle_count,
                             'A test must not leak window handles. This test started with '
                             '%s open top level browsing contexts, but ended with %s.' %
                             (self._start_handle_count, handle_count))
        finally:
            # For clean-up make sure we work on a proper browser window
            if not self.browser or self.browser.closed:
                # Find a proper replacement browser window
                # TODO: We have to make this less error prone in case no browser is open.
                self.browser = self.windows.switch_to(lambda win: type(win) is BrowserWindow)

            # Ensure to close all the remaining chrome windows to give following
            # tests a proper start condition and make them not fail.
            self.windows.close_all([self.browser])
            self.browser.focus()

            # Also close all remaining tabs
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.tabs[0]])
            self.browser.tabbar.tabs[0].switch_to()

    def restart(self, flags=None):
        """Restart Firefox and re-initialize data.

        :param flags: Specific restart flags for Firefox
        """
        # TODO: Bug 1148220 Marionette's in_app restart has to send 'quit-application-requested'
        # observer notification before an in_app restart
        self.marionette.execute_script("""
          Components.utils.import("resource://gre/modules/Services.jsm");
          let cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
                                     .createInstance(Components.interfaces.nsISupportsPRBool);
          Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);
        """)
        self.marionette.restart(in_app=True)

        # Marionette doesn't keep the former context, so restore to chrome
        self.marionette.set_context('chrome')

        # Ensure that we always have a valid browser instance available
        self.browser = self.windows.switch_to(lambda win: type(win) is BrowserWindow)

    def setUp(self, *args, **kwargs):
        super(BaseFirefoxTestCase, self).setUp(*args, **kwargs)

        self._start_handle_count = len(self.marionette.window_handles)
        self.marionette.set_context('chrome')

        self.browser = self.windows.current
        self.browser.focus()
        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            # Ensure that we have a default page opened
            self.marionette.navigate(self.prefs.get_pref('browser.newtab.url'))

    def tearDown(self, *args, **kwargs):
        self.marionette.set_context('chrome')

        try:
            self.prefs.restore_all_prefs()

            # This code should be run after all other tearDown code
            # so that in case of a failure, further tests will not run
            # in a state that is more inconsistent than necessary.
            self._check_and_fix_leaked_handles()
        finally:
            super(BaseFirefoxTestCase, self).tearDown(*args, **kwargs)
