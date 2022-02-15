# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import re
import subprocess

import pytest
from selenium.webdriver import Remote


def pytest_generate_tests(metafunc):
    """Generate tests based on markers."""

    if "session" not in metafunc.fixturenames:
        return

    marks = [mark.name for mark in metafunc.function.pytestmark]

    argvalues = []
    ids = []

    skip_platforms = []
    if "skip_platforms" in marks:
        for mark in metafunc.function.pytestmark:
            if mark.name == "skip_platforms":
                skip_platforms = mark.args

    if "with_interventions" in marks:
        argvalues.append([{"interventions": True, "skip_platforms": skip_platforms}])
        ids.append("with_interventions")

    if "without_interventions" in marks:
        argvalues.append([{"interventions": False, "skip_platforms": skip_platforms}])
        ids.append("without_interventions")

    metafunc.parametrize(["session"], argvalues, ids=ids, indirect=True)


class WebDriver:
    def __init__(self, config):
        self.browser_binary = config.getoption("browser_binary")
        self.webdriver_binary = config.getoption("webdriver_binary")
        self.port = config.getoption("webdriver_port")
        self.headless = config.getoption("headless")
        self.proc = None

    def command_line_driver(self):
        raise NotImplementedError

    def capabilities(self, use_interventions):
        raise NotImplementedError

    def __enter__(self):
        assert self.proc is None
        self.proc = subprocess.Popen(self.command_line_driver())
        return self

    def __exit__(self, *args, **kwargs):
        self.proc.kill()


class FirefoxWebDriver(WebDriver):
    def command_line_driver(self):
        return [self.webdriver_binary, "--port", str(self.port), "-vv"]

    def capabilities(self, use_interventions):
        fx_options = {"binary": self.browser_binary}

        interventions_prefs = [
            "perform_injections",
            "perform_ua_overrides",
            "enable_shims",
            "enable_picture_in_picture_overrides",
        ]
        fx_options["prefs"] = {
            f"extensions.webcompat.{pref}": use_interventions
            for pref in interventions_prefs
        }
        if self.headless:
            fx_options["args"] = ["--headless"]

        return {
            "pageLoadStrategy": "normal",
            "moz:firefoxOptions": fx_options,
        }


@pytest.fixture(scope="session")
def config_file(request):
    path = request.config.getoption("config")
    if not path:
        return None
    with open(path) as f:
        return json.load(f)


@pytest.fixture
def credentials(request, config_file):
    bug_number = re.findall("\d+", str(request.fspath.basename))[0]

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
def session(request, driver):
    use_interventions = request.param.get("interventions")
    print(f"use_interventions {use_interventions}")
    if use_interventions is None:
        raise ValueError(
            "Missing intervention marker in %s:%s"
            % (request.fspath, request.function.__name__)
        )
    capabilities = driver.capabilities(use_interventions)
    print(capabilities)

    url = f"http://localhost:{driver.port}"
    with Remote(command_executor=url, desired_capabilities=capabilities) as session:
        yield session


@pytest.fixture(autouse=True)
def skip_platforms(request, session):
    platform = session.capabilities["platformName"]
    if request.node.get_closest_marker("skip_platforms"):
        if request.node.get_closest_marker("skip_platforms").args[0] == platform:
            pytest.skip(f"Skipped on platform: {platform}")
