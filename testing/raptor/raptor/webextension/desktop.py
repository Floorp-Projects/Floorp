#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import shutil

from mozpower import MozPower
from mozrunner import runners

from logger.logger import RaptorLogger
from outputhandler import OutputHandler
from perftest import PerftestDesktop
from .base import WebExtension

LOG = RaptorLogger(component="raptor-webext-desktop")


class WebExtensionDesktop(PerftestDesktop, WebExtension):
    def __init__(self, *args, **kwargs):
        super(WebExtensionDesktop, self).__init__(*args, **kwargs)

        # create the desktop browser runner
        LOG.info("creating browser runner using mozrunner")
        self.output_handler = OutputHandler()
        process_args = {"processOutputLine": [self.output_handler]}
        firefox_args = ["--allow-downgrade"]
        runner_cls = runners[self.config["app"]]
        self.runner = runner_cls(
            self.config["binary"],
            profile=self.profile,
            cmdargs=firefox_args,
            process_args=process_args,
            symbols_path=self.config["symbols_path"],
        )

        # Force Firefox to immediately exit for content crashes
        self.runner.env["MOZ_CRASHREPORTER_SHUTDOWN"] = "1"

        if self.config["enable_webrender"]:
            self.runner.env["MOZ_WEBRENDER"] = "1"
            self.runner.env["MOZ_ACCELERATED"] = "1"
        else:
            self.runner.env["MOZ_WEBRENDER"] = "0"

    def launch_desktop_browser(self, test):
        raise NotImplementedError

    def start_runner_proc(self):
        # launch the browser via our previously-created runner
        self.runner.start()

        proc = self.runner.process_handler
        self.output_handler.proc = proc

        # give our control server the browser process so it can shut it down later
        self.control_server.browser_proc = proc

    def process_exists(self):
        return self.runner.is_running()

    def run_test(self, test, timeout):
        # tests will be run warm (i.e. NO browser restart between page-cycles)
        # unless otheriwse specified in the test INI by using 'cold = true'
        mozpower_measurer = None
        if self.config.get("power_test", False):
            powertest_name = test["name"].replace("/", "-").replace("\\", "-")
            output_dir = os.path.join(
                self.artifact_dir, "power-measurements-%s" % powertest_name
            )
            test_dir = os.path.join(output_dir, powertest_name)

            try:
                if not os.path.exists(output_dir):
                    os.mkdir(output_dir)
                if not os.path.exists(test_dir):
                    os.mkdir(test_dir)
            except Exception:
                LOG.critical(
                    "Could not create directories to store power testing data."
                )
                raise

            # Start power measurements with IPG creating a power usage log
            # every 30 seconds with 1 data point per second (or a 1000 milli-
            # second sampling rate).
            mozpower_measurer = MozPower(
                ipg_measure_duration=30,
                sampling_rate=1000,
                output_file_path=os.path.join(test_dir, "power-usage"),
            )
            mozpower_measurer.initialize_power_measurements()

        if test.get("cold", False) is True:
            self.__run_test_cold(test, timeout)
        else:
            self.__run_test_warm(test, timeout)

        if mozpower_measurer:
            mozpower_measurer.finalize_power_measurements(test_name=test["name"])
            perfherder_data = mozpower_measurer.get_perfherder_data()

            if not self.config.get("run_local", False):
                # when not running locally, zip the data and delete the folder which
                # was placed in the zip
                powertest_name = test["name"].replace("/", "-").replace("\\", "-")
                power_data_path = os.path.join(
                    self.artifact_dir, "power-measurements-%s" % powertest_name
                )
                shutil.make_archive(power_data_path, "zip", power_data_path)
                shutil.rmtree(power_data_path)

            for data_type in perfherder_data:
                self.control_server.submit_supporting_data(perfherder_data[data_type])

    def __run_test_cold(self, test, timeout):
        """
        Run the Raptor test but restart the entire browser app between page-cycles.

        Note: For page-load tests, playback will only be started once - at the beginning of all
        browser cycles, and then stopped after all cycles are finished. That includes the import
        of the mozproxy ssl cert and turning on the browser proxy.

        Since we're running in cold-mode, before this point (in manifest.py) the
        'expected-browser-cycles' value was already set to the initial 'page-cycles' value;
        and the 'page-cycles' value was set to 1 as we want to perform one page-cycle per
        browser restart.

        The 'browser-cycle' value is the current overall browser start iteration. The control
        server will receive the current 'browser-cycle' and the 'expected-browser-cycles' in
        each results set received; and will pass that on as part of the results so that the
        results processing will know results for multiple browser cycles are being received.

        The default will be to run in warm mode; unless 'cold = true' is set in the test INI.
        """
        LOG.info(
            "test %s is running in cold mode; browser WILL be restarted between "
            "page cycles" % test["name"]
        )

        for test["browser_cycle"] in range(1, test["expected_browser_cycles"] + 1):

            LOG.info(
                "begin browser cycle %d of %d for test %s"
                % (test["browser_cycle"], test["expected_browser_cycles"], test["name"])
            )

            self.run_test_setup(test)

            if test["browser_cycle"] == 1:

                if not self.is_localhost:
                    self.delete_proxy_settings_from_profile()

            else:
                # initial browser profile was already created before run_test was called;
                # now additional browser cycles we want to create a new one each time
                self.build_browser_profile()

                # Update runner profile
                self.runner.profile = self.profile

                self.run_test_setup(test)

            # now start the browser/app under test
            self.launch_desktop_browser(test)

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

            self.wait_for_test_finish(test, timeout, self.process_exists)

    def __run_test_warm(self, test, timeout):
        self.run_test_setup(test)

        if not self.is_localhost:
            self.delete_proxy_settings_from_profile()

        # start the browser/app under test
        self.launch_desktop_browser(test)

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        self.wait_for_test_finish(test, timeout, self.process_exists)

    def run_test_teardown(self, test):
        # browser should be closed by now but this is a backup-shutdown (if not in debug-mode)
        if not self.debug_mode:
            # If the runner was not started in the first place, stop() will silently
            # catch RunnerNotStartedError
            self.runner.stop()
        else:
            # in debug mode, and running locally, leave the browser running
            if self.config["run_local"]:
                LOG.info(
                    "* debug-mode enabled - please shutdown the browser manually..."
                )
                self.runner.wait(timeout=None)

        super(WebExtensionDesktop, self).run_test_teardown(test)

    def check_for_crashes(self):
        super(WebExtensionDesktop, self).check_for_crashes()

        try:
            self.runner.check_for_crashes()
        except NotImplementedError:  # not implemented for Chrome
            pass

        self.crashes += self.runner.crashed

    def clean_up(self):
        self.runner.stop()

        super(WebExtensionDesktop, self).clean_up()


class WebExtensionFirefox(WebExtensionDesktop):
    def disable_non_local_connections(self):
        # For Firefox we need to set MOZ_DISABLE_NONLOCAL_CONNECTIONS=1 env var before startup
        # when testing release builds from mozilla-beta/release. This is because of restrictions
        # on release builds that require webextensions to be signed unless this env var is set
        LOG.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=1")
        os.environ["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"

    def enable_non_local_connections(self):
        # pageload tests need to be able to access non-local connections via mitmproxy
        LOG.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=0")
        os.environ["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "0"

    def launch_desktop_browser(self, test):
        LOG.info("starting %s" % self.config["app"])
        if self.config["is_release_build"]:
            self.disable_non_local_connections()

        # if running debug-mode, tell Firefox to open the browser console on startup
        if self.debug_mode:
            self.runner.cmdargs.extend(["-jsconsole"])

        self.start_runner_proc()

        if self.config["is_release_build"] and test.get("playback") is not None:
            self.enable_non_local_connections()

        # if geckoProfile is enabled, initialize it
        if self.config["gecko_profile"] is True:
            self._init_gecko_profiling(test)
            # tell the control server the gecko_profile dir; the control server
            # will receive the filename of the stored gecko profile from the web
            # extension, and will move it out of the browser user profile to
            # this directory; where it is picked-up by gecko_profile.symbolicate
            self.control_server.gecko_profile_dir = (
                self.gecko_profiler.gecko_profile_dir
            )


class WebExtensionDesktopChrome(WebExtensionDesktop):
    def setup_chrome_args(self, test):
        # Setup chrome args and add them to the runner's args
        chrome_args = self.desktop_chrome_args(test)
        if " ".join(chrome_args) not in " ".join(self.runner.cmdargs):
            self.runner.cmdargs.extend(chrome_args)

    def launch_desktop_browser(self, test):
        LOG.info("starting %s" % self.config["app"])

        # Setup chrome/chromium specific arguments then start the runner
        self.setup_chrome_args(test)
        self.start_runner_proc()

    def set_browser_test_prefs(self, raw_prefs):
        # add test-specific preferences
        LOG.info(
            "preferences were configured for the test, however \
                        we currently do not install them on non-Firefox browsers."
        )
