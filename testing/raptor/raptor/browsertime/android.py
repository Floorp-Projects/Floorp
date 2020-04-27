#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

from mozdevice import ADBDevice

from logger.logger import RaptorLogger
from performance_tuning import tune_performance
from perftest import PerftestAndroid

from .base import Browsertime

LOG = RaptorLogger(component="raptor-browsertime-android")


class BrowsertimeAndroid(PerftestAndroid, Browsertime):
    """Android setup and configuration for browsertime

    When running raptor-browsertime tests on android, we create the profile (and set the proxy
    prefs in the profile that is using playback) but we don't need to copy it onto the device
    because geckodriver takes care of that.
    We tell browsertime to use our profile (we pass it in with the firefox.profileTemplate arg);
    browsertime creates a copy of that and passes that into geckodriver. Geckodriver then takes
    the profile and copies it onto the mobile device's test root for us; and then it even writes
    the geckoview app config.yaml file onto the device, which points the app to the profile on
    the device's test root.
    Therefore, raptor doesn't have to copy the profile onto the scard (and create the config.yaml)
    file ourselves. Also note when using playback, the nss certificate db is created as usual when
    mitmproxy is started (and saved in the profile) so it is already included in the profile that
    browsertime/geckodriver copies onto the device.
    """

    def __init__(self, app, binary, activity=None, intent=None, **kwargs):
        super(BrowsertimeAndroid, self).__init__(
            app, binary, profile_class="firefox", **kwargs
        )

        self.config.update({"activity": activity, "intent": intent})
        self.remote_test_root = "/data/local/tmp/tests/raptor"
        self.remote_profile = os.path.join(self.remote_test_root, "profile")

    @property
    def browsertime_args(self):
        if self.config['app'] == 'chrome-m':
            args_list = [
                '--browser', 'chrome',
                '--android',
            ]
        else:
            args_list = [
                "--browser", "firefox",
                "--android",
                # Work around a `selenium-webdriver` issue where Browsertime
                # fails to find a Firefox binary even though we're going to
                # actually do things on an Android device.
                "--firefox.binaryPath", self.browsertime_node,
                "--firefox.android.package", self.config["binary"],
                "--firefox.android.activity", self.config["activity"],
            ]

        # if running on Fenix we must add the intent as we use a special non-default one there
        if self.config["app"] == "fenix" and self.config.get("intent") is not None:
            args_list.extend(["--firefox.android.intentArgument=-a"])
            args_list.extend(
                ["--firefox.android.intentArgument", self.config["intent"]]
            )
            args_list.extend(["--firefox.android.intentArgument=-d"])
            args_list.extend(["--firefox.android.intentArgument", str("about:blank")])

        return args_list

    def setup_chrome_args(self, test):
        chrome_args = ["--use-mock-keychain", "--no-default-browser-check", "--no-first-run"]

        if test.get("playback", False):
            pb_args = [
                "--proxy-server=%s:%d" % (self.playback.host, self.playback.port),
                "--proxy-bypass-list=localhost;127.0.0.1",
                "--ignore-certificate-errors",
            ]

            if not self.is_localhost:
                pb_args[0] = pb_args[0].replace("127.0.0.1", self.config["host"])

            chrome_args.extend(pb_args)

        if self.debug_mode:
            chrome_args.extend(["--auto-open-devtools-for-tabs"])

        args_list = []
        for arg in chrome_args:
            args_list.extend(["--chrome.args=" + str(arg.replace("'", '"'))])

        return args_list

    def build_browser_profile(self):
        super(BrowsertimeAndroid, self).build_browser_profile()

        # Merge in the Android profile.
        path = os.path.join(self.profile_data_dir, "raptor-android")
        LOG.info("Merging profile: {}".format(path))
        self.profile.merge(path)
        self.profile.set_preferences(
            {"browser.tabs.remote.autostart": self.config["e10s"]}
        )

        # There's no great way to have "after" advice in Python, so we do this
        # in super and then again here since the profile merging re-introduces
        # the "#MozRunner" delimiters.
        self.remove_mozprofile_delimiters_from_profile()

    def setup_adb_device(self):
        if self.device is None:
            self.device = ADBDevice(verbose=True)
            if not self.config.get("disable_perf_tuning", False):
                tune_performance(self.device, log=LOG)

        self.clear_app_data()
        self.set_debug_app_flag()

    def run_test_setup(self, test):
        super(BrowsertimeAndroid, self).run_test_setup(test)

        self.set_reverse_ports()

        if self.playback:
            self.turn_on_android_app_proxy()
        self.remove_mozprofile_delimiters_from_profile()

    def run_tests(self, tests, test_names):
        self.setup_adb_device()

        if self.config['app'] == "chrome-m":
            # Make sure that chrome is enabled on the device
            self.device.shell_output("pm enable com.android.chrome", root=True)

        return super(BrowsertimeAndroid, self).run_tests(tests, test_names)

    def run_test_teardown(self, test):
        LOG.info("removing reverse socket connections")
        self.device.remove_socket_connections("reverse")

        super(BrowsertimeAndroid, self).run_test_teardown(test)
