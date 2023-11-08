#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import tempfile

import mozcrash
from cmdline import CHROME_ANDROID_APPS, FIREFOX_ANDROID_APPS
from logger.logger import RaptorLogger
from mozdevice import ADBDeviceFactory
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
    XXX: bc: This doesn't work with scoped storage in Android 10 since the shell owns the profile
    directory that is pushed to the device and the profile can no longer be on the sdcard. But when
    geckodriver's android.rs defines the profile to be located on internal storage, it will be
    owned by shell but if we are attempting to eliminate root, then when we run shell commands
    as the app, they will fail due to the app being unable to write to the shell owned profile
    directory.
    """

    def __init__(self, app, binary, activity=None, intent=None, **kwargs):
        super(BrowsertimeAndroid, self).__init__(
            app,
            binary,
            **kwargs,
        )

        self.config.update({"activity": activity, "intent": intent})
        self.remote_profile = None

    def _initialize_device(self):
        if self.device is None:
            self.device = ADBDeviceFactory(verbose=True)
            if not self.config.get("disable_perf_tuning", False):
                tune_performance(self.device, log=LOG)

    @property
    def android_external_storage(self):
        if self._remote_test_root is None:
            self._initialize_device()

            external_storage = self.device.shell_output("echo $EXTERNAL_STORAGE")
            self._remote_test_root = os.path.join(
                external_storage,
                "Android",
                "data",
                self.config["binary"],
                "files",
                "test_root",
            )

        return self._remote_test_root

    @property
    def browsertime_args(self):
        args_list = [
            "--viewPort",
            "1366x695",
            "--videoParams.convert",
            "false",
            "--videoParams.addTimer",
            "false",
            "--videoParams.androidVideoWaitTime",
            "20000",
            "--android.enabled",
            "true",
        ]

        if self.config["app"] in CHROME_ANDROID_APPS:
            args_list.extend(
                [
                    "--browser",
                    "chrome",
                ]
            )
        else:
            activity = self.config["activity"]
            if self.config["app"] == "fenix":
                LOG.info(
                    "Changing initial activity to "
                    "`mozilla.telemetry.glean.debug.GleanDebugActivity`"
                )
                activity = "mozilla.telemetry.glean.debug.GleanDebugActivity"

            args_list.extend(
                [
                    "--browser",
                    "firefox",
                    "--firefox.android.package",
                    self.config["binary"],
                    "--firefox.android.activity",
                    activity,
                ]
            )

        if self.config["app"] == "fenix":
            # See bug 1768889
            args_list.extend(["--ignoreShutdownFailures", "true"])

            # If running on Fenix we must add the intent as we use a
            # special non-default one there
            if self.config.get("intent") is not None:
                args_list.extend(["--firefox.android.intentArgument=-a"])
                args_list.extend(
                    ["--firefox.android.intentArgument", self.config["intent"]]
                )

                # Change glean ping names in all cases on Fenix
                args_list.extend(
                    [
                        "--firefox.android.intentArgument=--es",
                        "--firefox.android.intentArgument=startNext",
                        "--firefox.android.intentArgument=" + self.config["activity"],
                        "--firefox.android.intentArgument=--esa",
                        "--firefox.android.intentArgument=sourceTags",
                        "--firefox.android.intentArgument=automation",
                        "--firefox.android.intentArgument=--ez",
                        "--firefox.android.intentArgument=performancetest",
                        "--firefox.android.intentArgument=true",
                    ]
                )

                args_list.extend(["--firefox.android.intentArgument=-d"])
                args_list.extend(
                    ["--firefox.android.intentArgument", str("about:blank")]
                )

        return args_list

    def setup_chrome_args(self, test):
        chrome_args = [
            "--use-mock-keychain",
            "--no-default-browser-check",
            "--no-first-run",
        ]

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

        if self.config["app"] in FIREFOX_ANDROID_APPS:
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
        self._initialize_device()

        self.clear_app_data()
        self.set_debug_app_flag()
        self.device.run_as_package = self.config["binary"]

        self.geckodriver_profile = os.path.join(
            self.android_external_storage,
            "%s-geckodriver-profile" % self.config["binary"],
        )

        # make sure no remote profile exists
        if self.device.exists(self.geckodriver_profile):
            self.device.rm(self.geckodriver_profile, force=True, recursive=True)

    def check_for_crashes(self):
        super(BrowsertimeAndroid, self).check_for_crashes()

        try:
            dump_dir = tempfile.mkdtemp()
            remote_dir = os.path.join(self.geckodriver_profile, "minidumps")
            if not self.device.is_dir(remote_dir):
                return
            self.device.pull(remote_dir, dump_dir)
            self.crashes += mozcrash.log_crashes(
                LOG, dump_dir, self.config["symbols_path"]
            )
        except Exception as e:
            LOG.error(
                "Could not pull the crash data!",
                exc_info=True,
            )
            raise e
        finally:
            try:
                shutil.rmtree(dump_dir)
            except Exception:
                LOG.warning("unable to remove directory: %s" % dump_dir)

    def run_test_setup(self, test):
        super(BrowsertimeAndroid, self).run_test_setup(test)

        self.set_reverse_ports()

        if self.config["app"] in FIREFOX_ANDROID_APPS:
            if self.playback:
                self.turn_on_android_app_proxy()
            self.remove_mozprofile_delimiters_from_profile()

    def run_tests(self, tests, test_names):
        self.setup_adb_device()

        if self.config["app"] in CHROME_ANDROID_APPS:
            # Make sure that chrome is enabled on the device
            self.device.shell_output("pm enable com.android.chrome")

        return super(BrowsertimeAndroid, self).run_tests(tests, test_names)

    def run_test_teardown(self, test):
        LOG.info("removing reverse socket connections")
        self.device.remove_socket_connections("reverse")

        super(BrowsertimeAndroid, self).run_test_teardown(test)
