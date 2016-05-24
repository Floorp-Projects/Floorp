# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By

from firefox_ui_harness.testcases import FirefoxTestCase


class TestEnablePrivilege(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)

        self.url = self.marionette.absolute_url('security/enable_privilege.html')

    def test_enable_privilege(self):
        with self.marionette.using_context('content'):
            self.marionette.navigate(self.url)

            result = self.marionette.find_element(By.ID, 'result')
            self.assertEqual(result.get_property('textContent'), 'PASS')
