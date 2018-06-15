# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# add this directory to the path
import sys
import os
sys.path.append(os.path.dirname(__file__))

from session_store_test_case import SessionStoreTestCase


class TestSessionStoreWindowsShutdown(SessionStoreTestCase):

    def setUp(self):
        super(TestSessionStoreWindowsShutdown, self).setUp(startup_page=3, no_auto_updates=False)

    def test_with_variety(self):
        """ Test restoring a set of windows across Windows shutdown.

        Opens a set of windows, both standard and private, with
        some number of tabs in them. Once the tabs have loaded, shuts down
        the browser with the Windows Restart Manager, restarts the browser,
        and then ensures that the standard tabs have been restored,
        and that the private ones have not.

        This specifically exercises the Windows synchronous shutdown
        mechanism, which terminates the process in response to the
        Restart Manager's WM_ENDSESSION message.
        """

        current_windows_set = self.convert_open_windows_to_set()
        self.assertEqual(current_windows_set, self.all_windows,
                         msg='Not all requested windows have been opened. Expected {}, got {}.'
                         .format(self.all_windows, current_windows_set))

        self.marionette.quit(in_app=True, callback=lambda: self.simulate_os_shutdown())
        self.marionette.start_session()
        self.marionette.set_context('chrome')

        current_windows_set = self.convert_open_windows_to_set()
        self.assertEqual(current_windows_set, self.test_windows,
                         msg="""Non private browsing windows should have
                         been restored. Expected {}, got {}.
                         """.format(self.test_windows, current_windows_set))
