# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import contextlib
import os
import re
import textwrap

from marionette_driver.addons import Addons
from marionette_driver.errors import MarionetteException
from marionette_driver.wait import Wait
from marionette_harness import MarionetteTestCase
from marionette_harness.runner.mixins.window_manager import WindowManagerMixin

from telemetry_harness.ping_server import PingServer

CANARY_CLIENT_ID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0"
UUID_PATTERN = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
)


class TelemetryTestCase(WindowManagerMixin, MarionetteTestCase):
    def __init__(self, *args, **kwargs):
        """Initialize the test case and create a ping server."""
        super(TelemetryTestCase, self).__init__(*args, **kwargs)

    def setUp(self, *args, **kwargs):
        """Set up the test case and start the ping server."""

        self.ping_server = PingServer(
            self.testvars["server_root"], self.testvars["server_url"]
        )
        self.ping_server.start()

        super(TelemetryTestCase, self).setUp(*args, **kwargs)

        # Store IDs of addons installed via self.install_addon()
        self.addon_ids = []

        with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
            self.marionette.navigate("about:about")

    def disable_telemetry(self):
        """Disable the Firefox Data Collection and Use in the current browser."""
        self.marionette.instance.profile.set_persistent_preferences(
            {"datareporting.healthreport.uploadEnabled": False}
        )
        self.marionette.set_pref("datareporting.healthreport.uploadEnabled", False)

    def enable_telemetry(self):
        """Enable the Firefox Data Collection and Use in the current browser."""
        self.marionette.instance.profile.set_persistent_preferences(
            {"datareporting.healthreport.uploadEnabled": True}
        )
        self.marionette.set_pref("datareporting.healthreport.uploadEnabled", True)

    @contextlib.contextmanager
    def new_tab(self):
        """Perform operations in a new tab and then close the new tab."""

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            start_tab = self.marionette.current_window_handle
            new_tab = self.open_tab(focus=True)
            self.marionette.switch_to_window(new_tab)

            yield

            self.marionette.close()
            self.marionette.switch_to_window(start_tab)

    def navigate_in_new_tab(self, url):
        """Open a new tab and navigate to the provided URL."""

        with self.new_tab():
            with self.marionette.using_context(self.marionette.CONTEXT_CONTENT):
                self.marionette.navigate(url)

    def assertIsValidUUID(self, value):
        """Check if the given UUID is valid."""

        self.assertIsNotNone(value)
        self.assertNotEqual(value, "")

        # Check for client ID that is used when Telemetry upload is disabled
        self.assertNotEqual(value, CANARY_CLIENT_ID, msg="UUID is CANARY CLIENT ID")

        self.assertIsNotNone(
            re.match(UUID_PATTERN, value),
            msg="UUID does not match regular expression",
        )

    def wait_for_pings(self, action_func, ping_filter, count, ping_server=None):
        """Call the given action and wait for pings to come in and return
        the `count` number of pings, that match the given filter.
        """

        if ping_server is None:
            ping_server = self.ping_server

        # Keep track of the current number of pings
        current_num_pings = len(ping_server.pings)

        # New list to store new pings that satisfy the filter
        filtered_pings = []

        def wait_func(*args, **kwargs):
            # Ignore existing pings in ping_server.pings
            new_pings = ping_server.pings[current_num_pings:]

            # Filter pings to make sure we wait for the correct ping type
            filtered_pings[:] = [p for p in new_pings if ping_filter(p)]

            return len(filtered_pings) >= count

        self.logger.info(
            "wait_for_pings running action '{action}'.".format(
                action=action_func.__name__
            )
        )

        # Call given action and wait for a ping
        action_func()

        try:
            Wait(self.marionette, 60).until(wait_func)
        except Exception as e:
            self.fail("Error waiting for ping: {}".format(e))

        return filtered_pings[:count]

    def wait_for_ping(self, action_func, ping_filter, ping_server=None):
        """Call wait_for_pings() with the given action_func and ping_filter and
        return the first result.
        """
        [ping] = self.wait_for_pings(
            action_func, ping_filter, 1, ping_server=ping_server
        )
        return ping

    def restart_browser(self):
        """Restarts browser while maintaining the same profile."""
        return self.marionette.restart(clean=False, in_app=True)

    def start_browser(self):
        """Start the browser."""
        return self.marionette.start_session()

    def quit_browser(self):
        """Quit the browser."""
        return self.marionette.quit()

    def install_addon(self):
        """Install a minimal addon."""
        addon_name = "helloworld"
        self._install_addon(addon_name)

    def install_dynamic_addon(self):
        """Install a dynamic probe addon.

        Source Code:
        https://github.com/mozilla-extensions/dynamic-probe-telemetry-extension
        """
        addon_name = "dynamic_addon/dynamic-probe-telemetry-extension-signed.xpi"
        self._install_addon(addon_name, temp=False)

    def _install_addon(self, addon_name, temp=True):
        """Logic to install addon and add its ID to self.addons.ids"""
        resources_dir = os.path.join(os.path.dirname(__file__), "resources")
        addon_path = os.path.abspath(os.path.join(resources_dir, addon_name))

        try:
            # Ensure the Environment has init'd so the installed addon
            # triggers an "environment-change" ping.
            script = """\
            let [resolve] = arguments;
            const { TelemetryEnvironment } = ChromeUtils.import(
              "resource://gre/modules/TelemetryEnvironment.jsm"
            );
            TelemetryEnvironment.onInitialized().then(resolve);
            """

            with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
                self.marionette.execute_async_script(textwrap.dedent(script))

            addons = Addons(self.marionette)
            addon_id = addons.install(addon_path, temp=temp)
        except MarionetteException as e:
            self.fail("{} - Error installing addon: {} - ".format(e.cause, e))
        else:
            self.addon_ids.append(addon_id)

    def set_persistent_profile_preferences(self, preferences):
        """Wrapper for setting persistent preferences on a user profile"""
        return self.marionette.instance.profile.set_persistent_preferences(preferences)

    def set_preferences(self, preferences):
        """Wrapper for setting persistent preferences on a user profile"""
        return self.marionette.set_prefs(preferences)

    @property
    def client_id(self):
        """Return the ID of the current client."""
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(
                """\
                const { ClientID } = ChromeUtils.import(
                  "resource://gre/modules/ClientID.jsm"
                );
                return ClientID.getCachedClientID();
                """
            )

    @property
    def subsession_id(self):
        """Return the ID of the current subsession."""
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            ping_data = self.marionette.execute_script(
                """\
                const { TelemetryController } = ChromeUtils.import(
                  "resource://gre/modules/TelemetryController.jsm"
                );
                return TelemetryController.getCurrentPingData(true);
                """
            )
            return ping_data["payload"]["info"]["subsessionId"]

    def tearDown(self, *args, **kwargs):
        """Stop the ping server and tear down the testcase."""
        super(TelemetryTestCase, self).tearDown()
        self.ping_server.stop()
        self.marionette.quit(in_app=False, clean=True)
