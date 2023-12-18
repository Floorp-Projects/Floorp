import argparse
import json
import os
import re
import socket
import subprocess
import threading
import time
from contextlib import suppress
from urllib.parse import urlparse

import pytest
import webdriver
from mozprofile import Preferences, Profile
from mozrunner import FirefoxRunner

from .context import using_context


def get_arg_value(arg_names, args):
    """Get an argument value from a list of arguments

    This assumes that argparse argument parsing is close enough to the target
    to be compatible, at least with the set of inputs we have.

    :param arg_names: - List of names for the argument e.g. ["--foo", "-f"]
    :param args: - List of arguments to parse
    :returns: - Optional string argument value
    """
    parser = argparse.ArgumentParser()
    parser.add_argument(*arg_names, action="store", dest="value", default=None)
    parsed, _ = parser.parse_known_args(args)
    return parsed.value


@pytest.fixture(scope="module")
def browser(full_configuration):
    """Start a Firefox instance without using geckodriver.

    geckodriver will automatically use the --remote-allow-hosts and
    --remote.allow.origins command line arguments.

    Starting Firefox without geckodriver allows to set those command line arguments
    as needed. The fixture method returns the browser instance that should be used
    to connect to a RemoteAgent supported protocol (CDP, WebDriver BiDi).
    """
    current_browser = None

    def _browser(use_bidi=False, use_cdp=False, extra_args=None, extra_prefs=None):
        nonlocal current_browser

        # If the requested preferences and arguments match the ones for the
        # already started firefox, we can reuse the current firefox instance,
        # return the instance immediately.
        if current_browser:
            if (
                current_browser.use_bidi == use_bidi
                and current_browser.use_cdp == use_cdp
                and current_browser.extra_args == extra_args
                and current_browser.extra_prefs == extra_prefs
                and current_browser.is_running
            ):
                return current_browser

            # Otherwise, if firefox is already started, terminate it because we need
            # to create a new instance for the provided preferences.
            current_browser.quit()

        binary = full_configuration["browser"]["binary"]
        env = full_configuration["browser"]["env"]
        firefox_options = full_configuration["capabilities"]["moz:firefoxOptions"]
        current_browser = Browser(
            binary,
            firefox_options,
            use_bidi=use_bidi,
            use_cdp=use_cdp,
            extra_args=extra_args,
            extra_prefs=extra_prefs,
            env=env,
        )
        current_browser.start()
        return current_browser

    yield _browser

    # Stop firefox at the end of the test module.
    if current_browser is not None:
        current_browser.quit()
        current_browser = None


@pytest.fixture
def profile_folder(configuration):
    firefox_options = configuration["capabilities"]["moz:firefoxOptions"]
    return get_arg_value(["--profile"], firefox_options["args"])


@pytest.fixture
def custom_profile(profile_folder):
    # Clone the known profile for automation preferences
    profile = Profile.clone(profile_folder)

    yield profile

    profile.cleanup()


@pytest.fixture
def geckodriver(configuration):
    """Start a geckodriver instance directly."""
    driver = None

    def _geckodriver(config=None, hostname=None, extra_args=None):
        nonlocal driver

        if config is None:
            config = configuration

        driver = Geckodriver(config, hostname, extra_args)
        driver.start()

        return driver

    yield _geckodriver

    if driver is not None:
        driver.stop()


@pytest.fixture
def user_prefs(profile_folder):
    user_js = os.path.join(profile_folder, "user.js")

    prefs = {}
    for pref_name, pref_value in Preferences().read_prefs(user_js):
        prefs[pref_name] = pref_value

    return prefs


class Browser:
    def __init__(
        self,
        binary,
        firefox_options,
        use_bidi=False,
        use_cdp=False,
        extra_args=None,
        extra_prefs=None,
        env=None,
    ):
        self.use_bidi = use_bidi
        self.bidi_port_file = None
        self.use_cdp = use_cdp
        self.cdp_port_file = None
        self.extra_args = extra_args
        self.extra_prefs = extra_prefs

        self.debugger_address = None
        self.remote_agent_host = None
        self.remote_agent_port = None

        # Prepare temporary profile
        _profile_arg, profile_folder = firefox_options["args"]
        self.profile = Profile.clone(profile_folder)
        if self.extra_prefs is not None:
            self.profile.set_preferences(self.extra_prefs)

        if use_cdp:
            self.cdp_port_file = os.path.join(
                self.profile.profile, "DevToolsActivePort"
            )
            with suppress(FileNotFoundError):
                os.remove(self.cdp_port_file)
        if use_bidi:
            self.webdriver_bidi_file = os.path.join(
                self.profile.profile, "WebDriverBiDiServer.json"
            )
            with suppress(FileNotFoundError):
                os.remove(self.webdriver_bidi_file)

        cmdargs = ["-no-remote"]
        if self.use_bidi or self.use_cdp:
            cmdargs.extend(["--remote-debugging-port", "0"])
        if self.extra_args is not None:
            cmdargs.extend(self.extra_args)
        self.runner = FirefoxRunner(
            binary=binary, profile=self.profile, cmdargs=cmdargs, env=env
        )

    @property
    def is_running(self):
        return self.runner.is_running()

    def start(self):
        # Start Firefox.
        self.runner.start()

        if self.use_bidi:
            # Wait until the WebDriverBiDiServer.json file is ready
            while not os.path.exists(self.webdriver_bidi_file):
                time.sleep(0.1)

            # Read the connection details from file
            data = json.loads(open(self.webdriver_bidi_file).read())
            self.remote_agent_host = data["ws_host"]
            self.remote_agent_port = int(data["ws_port"])

        if self.use_cdp:
            # Wait until the DevToolsActivePort file is ready
            while not os.path.exists(self.cdp_port_file):
                time.sleep(0.1)

            # Read the port if needed and the debugger address from the
            # DevToolsActivePort file
            lines = open(self.cdp_port_file).readlines()
            assert len(lines) == 2

            if self.remote_agent_port is None:
                self.remote_agent_port = int(lines[0].strip())
            self.debugger_address = lines[1].strip()

    def quit(self, clean_profile=True):
        if self.is_running:
            self.runner.stop()
            self.runner.cleanup()

        if clean_profile:
            self.profile.cleanup()

    def wait(self):
        if self.is_running is True:
            self.runner.wait()


class Geckodriver:
    PORT_RE = re.compile(b".*Listening on [^ :]*:(\d+)")

    def __init__(self, configuration, hostname=None, extra_args=None):
        self.config = configuration["webdriver"]
        self.requested_capabilities = configuration["capabilities"]
        self.hostname = hostname or configuration["host"]
        self.extra_args = extra_args or []
        self.env = configuration["browser"]["env"]

        self.command = None
        self.proc = None
        self.port = None
        self.reader_thread = None

        self.capabilities = {"alwaysMatch": self.requested_capabilities}
        self.session = None

    @property
    def remote_agent_port(self):
        webSocketUrl = self.session.capabilities.get("webSocketUrl")
        assert webSocketUrl is not None

        return urlparse(webSocketUrl).port

    def start(self):
        self.command = (
            [self.config["binary"], "--port", "0"]
            + self.config["args"]
            + self.extra_args
        )

        print(f"Running command: {' '.join(self.command)}")
        self.proc = subprocess.Popen(self.command, env=self.env, stdout=subprocess.PIPE)

        self.reader_thread = threading.Thread(
            target=readOutputLine,
            args=(self.proc.stdout, self.processOutputLine),
            daemon=True,
        )
        self.reader_thread.start()
        # Wait for the port to become ready
        end_time = time.time() + 10
        while time.time() < end_time:
            returncode = self.proc.poll()
            if returncode is not None:
                raise ChildProcessError(
                    f"geckodriver terminated with code {returncode}"
                )
            if self.port is not None:
                with socket.socket() as sock:
                    if sock.connect_ex((self.hostname, self.port)) == 0:
                        break
            else:
                time.sleep(0.1)
        else:
            if self.port is None:
                raise OSError(
                    f"Failed to read geckodriver port started on {self.hostname}"
                )
            raise ConnectionRefusedError(
                f"Failed to connect to geckodriver on {self.hostname}:{self.port}"
            )

        self.session = webdriver.Session(
            self.hostname, self.port, capabilities=self.capabilities
        )

        return self

    def processOutputLine(self, line):
        if self.port is None:
            m = self.PORT_RE.match(line)
            if m is not None:
                self.port = int(m.groups()[0])

    def stop(self):
        if self.session is not None:
            self.delete_session()
        if self.proc:
            self.proc.kill()
        self.port = None
        if self.reader_thread is not None:
            self.reader_thread.join()

    def new_session(self):
        self.session.start()

    def delete_session(self):
        self.session.end()


def readOutputLine(stream, callback):
    while True:
        line = stream.readline()
        if not line:
            break

        callback(line)


def clear_pref(session, pref):
    """Clear the user-defined value from the specified preference.

    :param pref: Name of the preference.
    """
    with using_context(session, "chrome"):
        session.execute_script(
            """
           const { Preferences } = ChromeUtils.importESModule(
             "resource://gre/modules/Preferences.sys.mjs"
           );
           Preferences.reset(arguments[0]);
           """,
            args=(pref,),
        )


def get_pref(session, pref):
    """Get the value of the specified preference.

    :param pref: Name of the preference.
    """
    with using_context(session, "chrome"):
        pref_value = session.execute_script(
            """
            const { Preferences } = ChromeUtils.importESModule(
              "resource://gre/modules/Preferences.sys.mjs"
            );

            let pref = arguments[0];

            prefs = new Preferences();
            return prefs.get(pref, null);
            """,
            args=(pref,),
        )
        return pref_value


def set_pref(session, pref, value):
    """Set the value of the specified preference.

    :param pref: Name of the preference.
    :param value: The value to set the preference to. If the value is None,
                  reset the preference to its default value. If no default
                  value exists, the preference will cease to exist.
    """
    if value is None:
        clear_pref(session, pref)
        return

    with using_context(session, "chrome"):
        session.execute_script(
            """
            const { Preferences } = ChromeUtils.importESModule(
              "resource://gre/modules/Preferences.sys.mjs"
            );

            const [pref, value] = arguments;

            prefs = new Preferences();
            prefs.set(pref, value);
            """,
            args=(pref, value),
        )


@pytest.fixture
def use_pref(session):
    """Set a specific pref value."""
    reset_values = {}

    def _use_pref(pref, value):
        if pref not in reset_values:
            reset_values[pref] = get_pref(session, pref)

        set_pref(session, pref, value)

    yield _use_pref

    for pref, reset_value in reset_values.items():
        set_pref(session, pref, reset_value)
