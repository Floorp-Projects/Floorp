# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

from marionette_driver import Wait
from marionette_driver.addons import Addons

# Add this directory to the import path.
sys.path.append(os.path.dirname(__file__))

from service_worker_testutils import (
    EXT_DIR_PATH,
    EXT_ID,
    PREF_BG_SW_ENABLED,
    MarionetteServiceWorkerTestCase,
)


class TemporarilyInstalledAddonServiceWorkerNotPersisted(
    MarionetteServiceWorkerTestCase
):
    def setUp(self):
        super(TemporarilyInstalledAddonServiceWorkerNotPersisted, self).setUp()
        self.test_extension_id = EXT_ID
        # Flip the "mirror: once" pref and restart Firefox to be able
        # to run the extension successfully.
        self.marionette.set_pref(PREF_BG_SW_ENABLED, True)
        self.marionette.restart(in_app=True)

    def tearDown(self):
        self.marionette.restart(in_app=False, clean=True)
        super(TemporarilyInstalledAddonServiceWorkerNotPersisted, self).tearDown()

    def test_temporarily_installed_addon_serviceWorkers_not_persisted(self):
        self.install_temporary_extension_with_service_worker()
        # Make sure the extension worker registration is persisted
        # across restarts when the pref stays set to true.
        self.marionette.restart(in_app=True)
        self.assertFalse(self.is_extension_service_worker_registered)

    def install_temporary_extension_with_service_worker(self):
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
