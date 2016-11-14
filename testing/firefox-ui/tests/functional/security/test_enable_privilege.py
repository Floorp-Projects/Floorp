# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver import By


class TestEnablePrivilege(MarionetteTestCase):

    def test_enable_privilege(self):
        with self.marionette.using_context('content'):
            url = self.marionette.absolute_url('security/enable_privilege.html')
            self.marionette.navigate(url)

            result = self.marionette.find_element(By.ID, 'result')
            self.assertEqual(result.get_property('textContent'), 'PASS')
