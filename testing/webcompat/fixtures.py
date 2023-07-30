# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import asyncio
import json
import re
import subprocess

import pytest
import webdriver

from client import Client

APS_PREF = "privacy.partition.always_partition_third_party_non_cookie_storage"
CB_PBM_PREF = "network.cookie.cookieBehavior.pbmode"
CB_PREF = "network.cookie.cookieBehavior"
INJECTIONS_PREF = "extensions.webcompat.perform_injections"
PBM_PREF = "browser.privatebrowsing.autostart"
PIP_OVERRIDES_PREF = "extensions.webcompat.enable_picture_in_picture_overrides"
SHIMS_PREF = "extensions.webcompat.enable_shims"
STRICT_ETP_PREF = "privacy.trackingprotection.enabled"
UA_OVERRIDES_PREF = "extensions.webcompat.perform_ua_overrides"


class WebDriver:
    def __init__(self, config):
        self.browser_binary = config.getoption("browser_binary")
        self.device_serial = config.getoption("device_serial")
        self.package_name = config.getoption("package_name")
        self.addon = config.getoption("addon")
        self.webdriver_binary = config.getoption("webdriver_binary")
        self.port = config.getoption("webdriver_port")
        self.wsPort = config.getoption("webdriver_ws_port")
        self.headless = config.getoption("headless")
        self.proc = None

    def command_line_driver(self):
        raise NotImplementedError

    def capabilities(self, test_config):
        raise NotImplementedError

    def __enter__(self):
        assert self.proc is None
        self.proc = subprocess.Popen(self.command_line_driver())
        return self

    def __exit__(self, *args, **kwargs):
        self.proc.kill()


class FirefoxWebDriver(WebDriver):
    def command_line_driver(self):
        return [
            self.webdriver_binary,
            "--port",
            str(self.port),
            "--websocket-port",
            str(self.wsPort),
            "-vv",
        ]

    def capabilities(self, test_config):
        prefs = {}

        if "aps" in test_config:
            prefs[APS_PREF] = test_config["aps"]

        if "use_interventions" in test_config:
            value = test_config["use_interventions"]
            prefs[INJECTIONS_PREF] = value
            prefs[UA_OVERRIDES_PREF] = value
            prefs[PIP_OVERRIDES_PREF] = value

        if "use_pbm" in test_config:
            prefs[PBM_PREF] = test_config["use_pbm"]

        if "use_shims" in test_config:
            prefs[SHIMS_PREF] = test_config["use_shims"]

        if "use_strict_etp" in test_config:
            prefs[STRICT_ETP_PREF] = test_config["use_strict_etp"]

        # remote/cdp/CDP.sys.mjs sets cookieBehavior to 0,
        # which we definitely do not want, so set it back to 5.
        cookieBehavior = 4 if test_config.get("without_tcp") else 5
        prefs[CB_PREF] = cookieBehavior
        prefs[CB_PBM_PREF] = cookieBehavior

        fx_options = {"prefs": prefs}

        if self.browser_binary:
            fx_options["binary"] = self.browser_binary
            if self.headless:
                fx_options["args"] = ["--headless"]

        if self.device_serial:
            fx_options["androidDeviceSerial"] = self.device_serial
            fx_options["androidPackage"] = self.package_name

        if self.addon:
            prefs["xpinstall.signatures.required"] = False
            prefs["extensions.experiments.enabled"] = True

        return {
            "pageLoadStrategy": "normal",
            "moz:firefoxOptions": fx_options,
        }


@pytest.fixture(scope="session")
def should_do_2fa(request):
    return request.config.getoption("do2fa", False)


@pytest.fixture(scope="session")
def config_file(request):
    path = request.config.getoption("config")
    if not path:
        return None
    with open(path) as f:
        return json.load(f)


@pytest.fixture
def bug_number(request):
    return re.findall("\d+", str(request.fspath.basename))[0]


@pytest.fixture
def credentials(bug_number, config_file):
    if not config_file:
        pytest.skip(f"login info required for bug #{bug_number}")
        return None

    try:
        credentials = config_file[bug_number]
    except KeyError:
        pytest.skip(f"no login for bug #{bug_number} found")
        return

    return {"username": credentials["username"], "password": credentials["password"]}


@pytest.fixture(scope="session")
def driver(pytestconfig):
    if pytestconfig.getoption("browser") == "firefox":
        cls = FirefoxWebDriver
    else:
        assert False

    with cls(pytestconfig) as driver_instance:
        yield driver_instance


@pytest.fixture(scope="session")
def event_loop():
    return asyncio.get_event_loop_policy().new_event_loop()


@pytest.fixture(scope="function")
async def client(session, event_loop):
    return Client(session, event_loop)


def install_addon(session, addon_file_path):
    context = session.send_session_command("GET", "moz/context")
    session.send_session_command("POST", "moz/context", {"context": "chrome"})
    session.execute_async_script(
        """
        const addon_file_path = arguments[0];
        const cb = arguments[1];
        const { AddonManager } = ChromeUtils.import(
            "resource://gre/modules/AddonManager.jsm"
        );
        const { ExtensionPermissions } = ChromeUtils.import(
            "resource://gre/modules/ExtensionPermissions.jsm"
        );
        const { FileUtils } = ChromeUtils.importESModule(
            "resource://gre/modules/FileUtils.sys.mjs"
        );
        const file = new FileUtils.File(arguments[0]);
        AddonManager.installTemporaryAddon(file).then(addon => {
            // also make sure the addon works in private browsing mode
            const incognitoPermission = {
                permissions: ["internal:privateBrowsingAllowed"],
                origins: [],
            };
            ExtensionPermissions.add(addon.id, incognitoPermission).then(() => {
                addon.reload().then(cb);
            });
        });
        """,
        [addon_file_path],
    )
    session.send_session_command("POST", "moz/context", {"context": context})


@pytest.fixture(scope="function")
async def session(driver, test_config):
    caps = driver.capabilities(test_config)
    caps.update(
        {
            "acceptInsecureCerts": True,
            "webSocketUrl": True,
        }
    )
    caps = {"alwaysMatch": caps}
    print(caps)

    session = None
    for i in range(0, 15):
        try:
            if not session:
                session = webdriver.Session(
                    "localhost", driver.port, capabilities=caps, enable_bidi=True
                )
                session.test_config = test_config
            session.start()
            break
        except (ConnectionRefusedError, webdriver.error.TimeoutException):
            await asyncio.sleep(0.5)

    await session.bidi_session.start()

    if driver.addon:
        install_addon(session, driver.addon)

    yield session

    await session.bidi_session.end()
    session.end()


@pytest.fixture(autouse=True)
def platform(session):
    return session.capabilities["platformName"]


@pytest.fixture(autouse=True)
def only_platforms(bug_number, platform, request, session):
    if request.node.get_closest_marker("only_platforms"):
        for only in request.node.get_closest_marker("only_platforms").args:
            if only == platform:
                return
        pytest.skip(f"Bug #{bug_number} skipped on platform ({platform})")


@pytest.fixture(autouse=True)
def skip_platforms(bug_number, platform, request, session):
    if request.node.get_closest_marker("skip_platforms"):
        for skipped in request.node.get_closest_marker("skip_platforms").args:
            if skipped == platform:
                pytest.skip(f"Bug #{bug_number} skipped on platform ({platform})")
