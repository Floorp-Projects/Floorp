# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait
from marionette_driver.addons import Addons

import os
import sys

# Add this directory to the import path.
sys.path.append(os.path.dirname(__file__))

from service_worker_testutils import (
    MarionetteServiceWorkerTestCase,
    EXT_ID,
    EXT_DIR_PATH,
    PREF_BG_SW_ENABLED,
    PREF_PERSIST_TEMP_ADDONS,
)


class PurgeExtensionServiceWorkersOnPrefDisabled(MarionetteServiceWorkerTestCase):
    def setUp(self):
        super(PurgeExtensionServiceWorkersOnPrefDisabled, self).setUp()
        self.test_extension_id = EXT_ID
        # Flip the "mirror: once" pref and restart Firefox to be able
        # to run the extension successfully.
        self.marionette.set_pref(PREF_BG_SW_ENABLED, True)
        self.marionette.set_pref(PREF_PERSIST_TEMP_ADDONS, True)
        self.marionette.restart(in_app=True)

    def tearDown(self):
        self.marionette.restart(clean=True)
        super(PurgeExtensionServiceWorkersOnPrefDisabled, self).tearDown()

    def test_unregistering_service_worker_when_clearing_data(self):
        self.install_extension_with_service_worker()

        # Flip the pref to false and restart again to verify that the
        # service worker registration has been removed as expected.
        self.marionette.set_pref(PREF_BG_SW_ENABLED, False)
        self.marionette.restart(in_app=True)
        self.assertFalse(self.is_extension_service_worker_registered)

    def install_extension_with_service_worker(self):
        addons = Addons(self.marionette)
        test_extension_path = os.path.join(
            os.path.dirname(self.filepath), "data", EXT_DIR_PATH
        )
        addons.install(test_extension_path, temp=True)
        self.test_extension_base_url = self.get_extension_url()
        Wait(self.marionette).until(
            lambda _: self.is_extension_service_worker_registered,
            message="Wait the extension service worker to be registered",
        )
