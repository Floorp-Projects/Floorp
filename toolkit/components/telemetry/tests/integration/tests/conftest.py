# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import contextlib
import mozinstall
import os
import pytest
import re
import sys
import textwrap
import time

from marionette_driver import By, keys
from marionette_driver.addons import Addons
from marionette_driver.errors import MarionetteException
from marionette_driver.marionette import Marionette
from marionette_driver.wait import Wait
from six import reraise
from telemetry_harness.ping_server import PingServer

CANARY_CLIENT_ID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0"
SERVER_ROOT = "toolkit/components/telemetry/tests/marionette/harness/www"
UUID_PATTERN = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
)

here = os.path.abspath(os.path.dirname(__file__))

"""Get a build object we need to find a Firefox binary"""
try:
    from mozbuild.base import MozbuildObject

    build = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build = None


@pytest.fixture(name="binary")
def fixture_binary():
    """Return a Firefox binary"""
    try:
        return build.get_binary_path()
    except Exception:
        print(str(Exception))

    app = "firefox"
    bindir = os.path.join(os.environ["PYTHON_TEST_TMP"], app)
    if os.path.isdir(bindir):
        try:
            return mozinstall.get_binary(bindir, app_name=app)
        except Exception:
            print(str(Exception))

    if "GECKO_BINARY_PATH" in os.environ:
        return os.environ["GECKO_BINARY_PATH"]


@pytest.fixture(name="marionette")
def fixture_marionette(binary, ping_server):
    """Start a marionette session with specific browser prefs"""
    server_url = "{url}pings".format(url=ping_server.get_url("/"))
    prefs = {
        # Clear the region detection url to
        #   * avoid net access in tests
        #   * stabilize browser.search.region to avoid an extra subsession (bug 1579840#c40)
        "browser.region.network.url": '',
        # Disable smart sizing because it changes prefs at startup. (bug 1547750)
        "browser.cache.disk.smart_size.enabled": False,
        "toolkit.telemetry.server": server_url,
        "toolkit.telemetry.initDelay": 1,
        "toolkit.telemetry.minSubsessionLength": 0,
        "datareporting.healthreport.uploadEnabled": True,
        "datareporting.policy.dataSubmissionEnabled": True,
        "datareporting.policy.dataSubmissionPolicyBypassNotification": True,
        "toolkit.telemetry.log.level": "Trace",
        "toolkit.telemetry.log.dump": True,
        "toolkit.telemetry.send.overrideOfficialCheck": True,
        "toolkit.telemetry.testing.disableFuzzingDelay": True,
    }
    yield Marionette(host="localhost", port=0, bin=binary, prefs=prefs)


@pytest.fixture(name="ping_server")
def fixture_ping_server():
    """Run a ping server on localhost on a free port assigned by the OS"""
    server = PingServer(SERVER_ROOT, "http://localhost:0")
    server.start()
    yield server
    server.stop()


class Browser(object):
    def __init__(self, marionette, ping_server):
        self.marionette = marionette
        self.ping_server = ping_server
        self.addon_ids = []

    def disable_telemetry(self):
        self.marionette.instance.profile.set_persistent_preferences(
            {"datareporting.healthreport.uploadEnabled": False}
        )
        self.marionette.set_pref("datareporting.healthreport.uploadEnabled", False)

    def enable_search_events(self):
        """
        Event Telemetry categories are disabled by default.
        Search events are in the "navigation" category and are not enabled by
        default in builds of Firefox, so we enable them here.
        """

        script = """\
        let {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
        Services.telemetry.setEventRecordingEnabled("navigation", true);
        """

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            self.marionette.execute_script(textwrap.dedent(script))

    def enable_telemetry(self):
        self.marionette.instance.profile.set_persistent_preferences(
            {"datareporting.healthreport.uploadEnabled": True}
        )
        self.marionette.set_pref("datareporting.healthreport.uploadEnabled", True)

    def get_client_id(self):
        """Return the ID of the current client."""
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(
                'Cu.import("resource://gre/modules/ClientID.jsm");'
                "return ClientID.getCachedClientID();"
            )

    def get_default_search_engine(self):
        """Retrieve the identifier of the default search engine.

        We found that it's required to initialize the search service before
        attempting to retrieve the default search engine. Not calling init
        would result in a JavaScript error (see bug 1543960 for more
        information).
        """

        script = """\
        let [resolve] = arguments;
        let searchService = Components.classes[
                "@mozilla.org/browser/search-service;1"]
            .getService(Components.interfaces.nsISearchService);
        return searchService.init().then(function () {
          resolve(searchService.defaultEngine.identifier);
        });
        """

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_async_script(textwrap.dedent(script))

    def install_addon(self):
        resources_dir = os.path.join(os.path.dirname(__file__), "resources")
        addon_path = os.path.abspath(os.path.join(resources_dir, "helloworld"))

        try:
            # Ensure the Environment has init'd so the installed addon
            # triggers an "environment-change" ping.
            script = """\
                    let [resolve] = arguments;
            Cu.import("resource://gre/modules/TelemetryEnvironment.jsm");
            TelemetryEnvironment.onInitialized().then(resolve);
            """

            with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
                self.marionette.execute_async_script(textwrap.dedent(script))

            addons = Addons(self.marionette)
            addon_id = addons.install(addon_path, temp=True)
        except MarionetteException as e:
            pytest.fail("{} - Error installing addon: {} - ".format(e.cause, e.message))
        else:
            self.addon_ids.append(addon_id)

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

    def open_tab(self, focus=False):
        current_tabs = self.marionette.window_handles

        try:
            result = self.marionette.open(type="tab", focus=focus)
            if result["type"] != "tab":
                raise Exception(
                    "Newly opened browsing context is of type {} and not tab.".format(
                        result["type"]
                    )
                )
        except Exception:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            reraise(
                exc_type,
                exc_type("Failed to trigger opening a new tab: {}".format(exc_value)),
                exc_traceback,
            )
        else:
            Wait(self.marionette).until(
                lambda mn: len(mn.window_handles) == len(current_tabs) + 1,
                message="No new tab has been opened",
            )

            [new_tab] = list(set(self.marionette.window_handles) - set(current_tabs))

            return new_tab

    def quit(self, in_app=False):
        self.marionette.quit(in_app=in_app)

    def restart(self):
        self.marionette.restart(clean=False, in_app=True)

    def search(self, text):
        """Perform a search via the browser's URL bar."""

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            self.marionette.execute_script("gURLBar.select();")
            urlbar = self.marionette.find_element(By.ID, "urlbar-input")
            urlbar.send_keys(keys.Keys.DELETE)
            urlbar.send_keys(text + keys.Keys.ENTER)

        # Wait for 0.1 seconds before proceeding to decrease the chance
        # of Firefox being shut down before Telemetry is recorded
        time.sleep(0.1)

    def search_in_new_tab(self, text):
        """Open a new tab and perform a search via the browser's URL bar,
        then close the new tab."""
        with self.new_tab():
            self.search(text)

    def start_session(self):
        self.marionette.start_session()

    def wait_for_search_service_init(self):
        script = """\
        let [resolve] = arguments;
        let searchService = Components.classes["@mozilla.org/browser/search-service;1"]
            .getService(Components.interfaces.nsISearchService);
        searchService.init().then(resolve);
        """

        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            self.marionette.execute_async_script(textwrap.dedent(script))


@pytest.fixture(name="browser")
def fixture_browser(marionette, ping_server):
    """Return an instance of our Browser object"""
    browser = Browser(marionette, ping_server)
    browser.start_session()
    yield browser
    browser.quit()


class Helpers(object):
    def __init__(self, ping_server, marionette):
        self.ping_server = ping_server
        self.marionette = marionette

    def assert_is_valid_uuid(self, value):
        """Custom assertion for UUID's"""
        assert value is not None
        assert value != ""
        assert value != CANARY_CLIENT_ID
        assert re.match(UUID_PATTERN, value) is not None

    def wait_for_ping(self, action_func, ping_filter):
        [ping] = self.wait_for_pings(action_func, ping_filter, 1)
        return ping

    def wait_for_pings(self, action_func, ping_filter, count):
        """Call the given action and wait for pings to come in and return
        the `count` number of pings, that match the given filter."""
        # Keep track of the current number of pings
        current_num_pings = len(self.ping_server.pings)

        # New list to store new pings that satisfy the filter
        filtered_pings = []

        def wait_func(*args, **kwargs):
            # Ignore existing pings in self.ping_server.pings
            new_pings = self.ping_server.pings[current_num_pings:]

            # Filter pings to make sure we wait for the correct ping type
            filtered_pings[:] = [p for p in new_pings if ping_filter(p)]

            return len(filtered_pings) >= count

        action_func()

        try:
            Wait(self.marionette, 60).until(wait_func)
        except Exception as e:
            pytest.fail("Error waiting for ping: {}".format(e))

        return filtered_pings[:count]


@pytest.fixture(name="helpers")
def fixture_helpers(ping_server, marionette):
    """Return an instace of our helpers object"""
    return Helpers(ping_server, marionette)
