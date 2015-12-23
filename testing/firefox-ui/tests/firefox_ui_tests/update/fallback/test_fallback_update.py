# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer.testcases import UpdateTestCase


class TestFallbackUpdate(UpdateTestCase):

    def setUp(self):
        UpdateTestCase.setUp(self, is_fallback=True)

    def tearDown(self):
        try:
            self.windows.close_all([self.browser])
        finally:
            UpdateTestCase.tearDown(self)

    def _test_update(self):
        self.download_and_apply_available_update(force_fallback=True)

        self.download_and_apply_forced_update()

        self.check_update_applied()

    def test_update(self):
        try:
            self._test_update()
        except:
            # Switch context to the main browser window before embarking
            # down the failure code path to work around bug 1141519.
            self.browser.switch_to()
            raise
