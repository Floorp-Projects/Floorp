# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_ui_harness.testcases import UpdateTestCase


class TestDirectUpdate(UpdateTestCase):

    def setUp(self):
        UpdateTestCase.setUp(self, is_fallback=False)

    def tearDown(self):
        try:
            self.puppeteer.windows.close_all([self.browser])
        finally:
            UpdateTestCase.tearDown(self)

    def test_update(self):
        self.download_and_apply_available_update(force_fallback=False)
        self.check_update_applied()
