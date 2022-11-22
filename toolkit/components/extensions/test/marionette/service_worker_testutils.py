# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_harness import MarionetteTestCase

EXT_ID = "extension-with-bg-sw@test"
EXT_DIR_PATH = "extension-with-bg-sw"
PREF_BG_SW_ENABLED = "extensions.backgroundServiceWorker.enabled"
PREF_PERSIST_TEMP_ADDONS = (
    "dom.serviceWorkers.testing.persistTemporarilyInstalledAddons"
)


class MarionetteServiceWorkerTestCase(MarionetteTestCase):
    def get_extension_url(self, path="/"):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script(
                """
              let policy = WebExtensionPolicy.getByID(arguments[0]);
              return policy.getURL(arguments[1])
            """,
                script_args=(self.test_extension_id, path),
            )

    @property
    def is_extension_service_worker_registered(self):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script(
                """
                let serviceWorkerManager = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
                    Ci.nsIServiceWorkerManager
                );

                let serviceWorkers = serviceWorkerManager.getAllRegistrations();
                for (let i = 0; i < serviceWorkers.length; i++) {
                    let sw = serviceWorkers.queryElementAt(
                        i,
                        Ci.nsIServiceWorkerRegistrationInfo
                    );
                    if (sw.scope == arguments[0]) {
                        return true;
                    }
                }
                return false;
            """,
                script_args=(self.test_extension_base_url,),
            )
