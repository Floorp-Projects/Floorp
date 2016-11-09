# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer.puppeteer import Puppeteer
from firefox_puppeteer.ui.browser.window import BrowserWindow


class PuppeteerMixin(object):
    """Mix-in class for Firefox specific API modules exposed to test scope.

    It also provides common set-up and tear-down code for Firefox tests.

    Child test case classes are expected to also subclass MarionetteTestCase such
    that PuppeteerMixin is followed by MarionetteTestCase. This will insert the
    Puppeteer mixin before the MarionetteTestCase into the MRO.

    example:
    `class MyTestCase(PuppeteerMixin, MarionetteTestCase)`

    The key role of MarionetteTestCase is to set self.marionette appropriately
    in `setUp()`. Any TestCase class that satisfies this requirement is
    compatible with this class.

    If you're extending the inheritance tree further to make specialized
    TestCases, favour the use of super() as opposed to explicit calls to a
    parent class.

    """
    def _check_and_fix_leaked_handles(self):
        handle_count = len(self.marionette.window_handles)
        url = []

        try:
            # Verify the existence of leaked tabs and print their URLs.
            if self._start_handle_count < handle_count:
                message = ('A test must not leak window handles. This test started with '
                           '%s open top level browsing contexts, but ended with %s.'
                           ' Remaining Tabs URLs:') % (self._start_handle_count, handle_count)
                with self.marionette.using_context('content'):
                    for tab in self.marionette.window_handles:
                        if tab not in self._init_tab_handles:
                            url.append(' %s' % self.marionette.get_url())
                self.assertListEqual(self._init_tab_handles, self.marionette.window_handles,
                                     message + ','.join(url))
        finally:
            # For clean-up make sure we work on a proper browser window
            if not self.browser or self.browser.closed:
                # Find a proper replacement browser window
                # TODO: We have to make this less error prone in case no browser is open.
                self.browser = self.puppeteer.windows.switch_to(
                    lambda win: type(win) is BrowserWindow)

            # Ensure to close all the remaining chrome windows to give following
            # tests a proper start condition and make them not fail.
            self.puppeteer.windows.close_all([self.browser])
            self.browser.focus()

            # Also close all remaining tabs
            self.browser.tabbar.close_all_tabs([self.browser.tabbar.tabs[0]])
            self.browser.tabbar.tabs[0].switch_to()

    def restart(self, **kwargs):
        """Restart Firefox and re-initialize data.

        :param flags: Specific restart flags for Firefox
        """
        self.marionette.restart(in_app=not kwargs.get('clean'), **kwargs)

        # Ensure that we always have a valid browser instance available
        self.browser = self.puppeteer.windows.switch_to(lambda win: type(win) is BrowserWindow)

    def setUp(self, *args, **kwargs):
        super(PuppeteerMixin, self).setUp(*args, **kwargs)

        self._start_handle_count = len(self.marionette.window_handles)
        self._init_tab_handles = self.marionette.window_handles
        self.marionette.set_context('chrome')

        self.puppeteer = Puppeteer(self.marionette)
        self.browser = self.puppeteer.windows.current
        self.browser.focus()
        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            # Ensure that we have a default page opened
            self.marionette.navigate(self.puppeteer.prefs.get_pref('browser.newtab.url'))

    def tearDown(self, *args, **kwargs):
        self.marionette.set_context('chrome')

        try:
            # This code should be run after all other tearDown code
            # so that in case of a failure, further tests will not run
            # in a state that is more inconsistent than necessary.
            self._check_and_fix_leaked_handles()
        finally:
            super(PuppeteerMixin, self).tearDown(*args, **kwargs)
