# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import Wait
from marionette_harness import MarionetteTestCase


class ServiceWorkerAtShutdownTestCase(MarionetteTestCase):
    def setUp(self):
        super(ServiceWorkerAtShutdownTestCase, self).setUp()
        self.install_service_worker()
        self.set_pref_to_delete_site_data_on_shutdown()

    def tearDown(self):
        self.marionette.restart(clean=True)
        super(ServiceWorkerAtShutdownTestCase, self).tearDown()

    def install_service_worker(self):
        install_url = self.marionette.absolute_url("serviceworker/install_serviceworker.html")
        self.marionette.navigate(install_url)
        Wait(self.marionette).until(lambda _: self.is_service_worker_registered)

    def set_pref_to_delete_site_data_on_shutdown(self):
        self.marionette.set_pref("network.cookie.lifetimePolicy", 2)

    def test_unregistering_service_worker_when_clearing_data(self):
        self.marionette.restart(clean=False, in_app=True)
        self.assertFalse(self.is_service_worker_registered)

    @property
    def is_service_worker_registered(self):
        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script("""
                let serviceWorkerManager = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
                    Ci.nsIServiceWorkerManager
                );

                let principal =
                    Services.scriptSecurityManager.createContentPrincipalFromOrigin(arguments[0]);

                let serviceWorkers = serviceWorkerManager.getAllRegistrations();
                for (let i = 0; i < serviceWorkers.length; i++) {
                    let sw = serviceWorkers.queryElementAt(
                        i,
                        Ci.nsIServiceWorkerRegistrationInfo
                    );
                    if (sw.principal.origin == principal.origin) {
                        return true;
                    }
                }
                return false;
            """, script_args=(self.marionette.absolute_url(""),))
