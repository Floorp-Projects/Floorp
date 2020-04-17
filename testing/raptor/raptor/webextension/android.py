#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import posixpath
import shutil
import tempfile
import time

import mozcrash
from cpu import start_android_cpu_profiler
from logger.logger import RaptorLogger
from mozdevice import ADBDevice
from performance_tuning import tune_performance
from perftest import PerftestAndroid
from power import (
    init_android_power_test,
    finish_android_power_test,
    enable_charging,
    disable_charging
)
from signal_handler import SignalHandlerException
from utils import write_yml_file
from webextension.base import WebExtension

LOG = RaptorLogger(component="raptor-webext-android")


class WebExtensionAndroid(PerftestAndroid, WebExtension):
    def __init__(self, app, binary, activity=None, intent=None, **kwargs):
        super(WebExtensionAndroid, self).__init__(
            app, binary, profile_class="firefox", **kwargs
        )

        self.config.update({"activity": activity, "intent": intent})

        self.remote_test_root = os.path.abspath(
            os.path.join(os.sep, "sdcard", "raptor")
        )
        self.remote_profile = os.path.join(self.remote_test_root, "profile")
        self.os_baseline_data = None
        self.power_test_time = None
        self.screen_off_timeout = 0
        self.screen_brightness = 127
        self.app_launched = False

    def setup_adb_device(self):
        if self.device is None:
            self.device = ADBDevice(verbose=True)
            if not self.config.get("disable_perf_tuning", False):
                tune_performance(self.device, log=LOG)

        if self.config['power_test']:
            disable_charging(self.device)

        LOG.info("creating remote root folder for raptor: %s" % self.remote_test_root)
        self.device.rm(self.remote_test_root, force=True, recursive=True)
        self.device.mkdir(self.remote_test_root)
        self.device.chmod(self.remote_test_root, recursive=True, root=True)

        self.clear_app_data()
        self.set_debug_app_flag()

    def write_android_app_config(self):
        # geckoview supports having a local on-device config file; use this file
        # to tell the app to use the specified browser profile, as well as other opts
        # on-device: /data/local/tmp/com.yourcompany.yourapp-geckoview-config.yaml
        # https://mozilla.github.io/geckoview/tutorials/automation.html#configuration-file-format

        # only supported for geckoview apps
        if self.config["app"] == "fennec":
            return
        LOG.info("creating android app config.yml")

        yml_config_data = dict(
            args=[
                "--profile",
                self.remote_profile,
                "--allow-downgrade",
                "use_multiprocess",
                self.config["e10s"],
            ],
            env=dict(
                LOG_VERBOSE=1,
                R_LOG_LEVEL=6,
                MOZ_WEBRENDER=int(self.config["enable_webrender"]),
            ),
        )

        yml_name = "%s-geckoview-config.yaml" % self.config["binary"]
        yml_on_host = os.path.join(tempfile.mkdtemp(), yml_name)
        write_yml_file(yml_on_host, yml_config_data)
        yml_on_device = os.path.join("/data", "local", "tmp", yml_name)

        try:
            LOG.info("copying %s to device: %s" % (yml_on_host, yml_on_device))
            self.device.rm(yml_on_device, force=True, recursive=True)
            self.device.push(yml_on_host, yml_on_device)

        except Exception:
            LOG.critical("failed to push %s to device!" % yml_on_device)
            raise

    def log_android_device_temperature(self):
        try:
            # retrieve and log the android device temperature
            thermal_zone0 = self.device.shell_output(
                "cat sys/class/thermal/thermal_zone0/temp"
            )
            thermal_zone0 = float(thermal_zone0)
            zone_type = self.device.shell_output(
                "cat sys/class/thermal/thermal_zone0/type"
            )
            LOG.info(
                "(thermal_zone0) device temperature: %.3f zone type: %s"
                % (thermal_zone0 / 1000, zone_type)
            )
        except Exception as exc:
            LOG.warning("Unexpected error: {} - {}".format(exc.__class__.__name__, exc))

    def launch_firefox_android_app(self, test_name):
        LOG.info("starting %s" % self.config["app"])

        extra_args = [
            "-profile", self.remote_profile,
            "--allow-downgrade",
            "--es", "env0",
            "LOG_VERBOSE=1",
            "--es", "env1",
            "R_LOG_LEVEL=6",
            "--es", "env2",
            "MOZ_WEBRENDER=%d" % self.config["enable_webrender"],
            # Force the app to immediately exit for content crashes
            "--es", "env3",
            "MOZ_CRASHREPORTER_SHUTDOWN=1",
        ]

        try:
            # make sure the android app is not already running
            self.device.stop_application(self.config["binary"])

            if self.config["app"] == "fennec":
                self.device.launch_fennec(
                    self.config["binary"],
                    extra_args=extra_args,
                    url="about:blank",
                    fail_if_running=False,
                )
            else:

                # command line 'extra' args not used with geckoview apps; instead we use
                # an on-device config.yml file (see write_android_app_config)

                self.device.launch_application(
                    self.config["binary"],
                    self.config["activity"],
                    self.config["intent"],
                    extras=None,
                    url="about:blank",
                    fail_if_running=False,
                )

            # Check if app has started and it's running
            if not self.device.process_exist(self.config["binary"]):
                raise Exception(
                    "Error launching %s. App did not start properly!"
                    % self.config["binary"]
                )
            self.app_launched = True
        except Exception as e:
            LOG.error("Exception launching %s" % self.config["binary"])
            LOG.error("Exception: %s %s" % (type(e).__name__, str(e)))
            if self.config["power_test"]:
                finish_android_power_test(self, test_name)
            raise

        # give our control server the device and app info
        self.control_server.device = self.device
        self.control_server.app_name = self.config["binary"]

    def copy_cert_db(self, source_dir, target_dir):
        # copy browser cert db (that was previously created via certutil) from source to target
        cert_db_files = ["pkcs11.txt", "key4.db", "cert9.db"]
        for next_file in cert_db_files:
            _source = os.path.join(source_dir, next_file)
            _dest = os.path.join(target_dir, next_file)
            if os.path.exists(_source):
                LOG.info("copying %s to %s" % (_source, _dest))
                shutil.copyfile(_source, _dest)
            else:
                LOG.critical("unable to find ssl cert db file: %s" % _source)

    def run_tests(self, tests, test_names):
        self.setup_adb_device()

        return super(WebExtensionAndroid, self).run_tests(tests, test_names)

    def run_test_setup(self, test):
        super(WebExtensionAndroid, self).run_test_setup(test)
        self.set_reverse_ports()

    def run_test_teardown(self, test):
        LOG.info("removing reverse socket connections")
        self.device.remove_socket_connections("reverse")

        super(WebExtensionAndroid, self).run_test_teardown(test)

    def run_test(self, test, timeout):
        # tests will be run warm (i.e. NO browser restart between page-cycles)
        # unless otheriwse specified in the test INI by using 'cold = true'
        try:

            if self.config["power_test"]:
                # gather OS baseline data
                init_android_power_test(self)
                LOG.info("Running OS baseline, pausing for 1 minute...")
                time.sleep(60)
                LOG.info("Finishing baseline...")
                finish_android_power_test(self, "os-baseline", os_baseline=True)

                # initialize for the test
                init_android_power_test(self)

            if test.get("cold", False) is True:
                self.__run_test_cold(test, timeout)
            else:
                self.__run_test_warm(test, timeout)

        except SignalHandlerException:
            self.device.stop_application(self.config["binary"])
            if self.config['power_test']:
                enable_charging(self.device)

        finally:
            if self.config["power_test"]:
                finish_android_power_test(self, test["name"])

    def __run_test_cold(self, test, timeout):
        """
        Run the Raptor test but restart the entire browser app between page-cycles.

        Note: For page-load tests, playback will only be started once - at the beginning of all
        browser cycles, and then stopped after all cycles are finished. The proxy is set via prefs
        in the browser profile so those will need to be set again in each new profile/cycle.
        Note that instead of using the certutil tool each time to create a db and import the
        mitmproxy SSL cert (it's done in mozbase/mozproxy) we will simply copy the existing
        cert db from the first cycle's browser profile into the new clean profile; this way
        we don't have to re-create the cert db on each browser cycle.

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

            self.clear_app_data()
            self.set_debug_app_flag()

            if test["browser_cycle"] == 1:
                if test.get("playback") is not None:
                    # an ssl cert db has now been created in the profile; copy it out so we
                    # can use the same cert db in future test cycles / browser restarts
                    local_cert_db_dir = tempfile.mkdtemp()
                    LOG.info(
                        "backing up browser ssl cert db that was created via certutil"
                    )
                    self.copy_cert_db(
                        self.config["local_profile_dir"], local_cert_db_dir
                    )

                if not self.is_localhost:
                    self.delete_proxy_settings_from_profile()

            else:
                # double-check to ensure app has been shutdown
                self.device.stop_application(self.config["binary"])

                # initial browser profile was already created before run_test was called;
                # now additional browser cycles we want to create a new one each time
                self.build_browser_profile()

                if test.get("playback") is not None:
                    # get cert db from previous cycle profile and copy into new clean profile
                    # this saves us from having to start playback again / recreate cert db etc.
                    LOG.info("copying existing ssl cert db into new browser profile")
                    self.copy_cert_db(
                        local_cert_db_dir, self.config["local_profile_dir"]
                    )

                self.run_test_setup(test)

            if test.get("playback") is not None:
                self.turn_on_android_app_proxy()

            self.copy_profile_to_device()
            self.log_android_device_temperature()

            # write android app config.yml
            self.write_android_app_config()

            # now start the browser/app under test
            self.launch_firefox_android_app(test["name"])

            # set our control server flag to indicate we are running the browser/app
            self.control_server._finished = False

            if self.config["cpu_test"]:
                # start measuring CPU usage
                self.cpu_profiler = start_android_cpu_profiler(self)

            self.wait_for_test_finish(test, timeout)

            # in debug mode, and running locally, leave the browser running
            if self.debug_mode and self.config["run_local"]:
                LOG.info(
                    "* debug-mode enabled - please shutdown the browser manually..."
                )
                self.runner.wait(timeout=None)

            # break test execution if a exception is present
            if len(self.results_handler.page_timeout_list) > 0:
                break

    def __run_test_warm(self, test, timeout):
        LOG.info(
            "test %s is running in warm mode; browser will NOT be restarted between "
            "page cycles" % test["name"]
        )

        self.run_test_setup(test)

        if not self.is_localhost:
            self.delete_proxy_settings_from_profile()

        if test.get("playback") is not None:
            self.turn_on_android_app_proxy()

        self.clear_app_data()
        self.set_debug_app_flag()
        self.copy_profile_to_device()
        self.log_android_device_temperature()

        # write android app config.yml
        self.write_android_app_config()

        # now start the browser/app under test
        self.launch_firefox_android_app(test["name"])

        # set our control server flag to indicate we are running the browser/app
        self.control_server._finished = False

        if self.config["cpu_test"]:
            # start measuring CPU usage
            self.cpu_profiler = start_android_cpu_profiler(self)

        self.wait_for_test_finish(test, timeout)

        # in debug mode, and running locally, leave the browser running
        if self.debug_mode and self.config["run_local"]:
            LOG.info("* debug-mode enabled - please shutdown the browser manually...")

    def check_for_crashes(self):
        super(WebExtensionAndroid, self).check_for_crashes()

        if not self.app_launched:
            LOG.info("skipping check_for_crashes: application has not been launched")
            return
        self.app_launched = False

        try:
            dump_dir = tempfile.mkdtemp()
            remote_dir = posixpath.join(self.remote_profile, "minidumps")
            if not self.device.is_dir(remote_dir):
                return
            self.device.pull(remote_dir, dump_dir)
            self.crashes += mozcrash.log_crashes(LOG, dump_dir, self.config["symbols_path"])
        finally:
            try:
                shutil.rmtree(dump_dir)
            except Exception:
                LOG.warning("unable to remove directory: %s" % dump_dir)

    def clean_up(self):
        LOG.info("removing test folder for raptor: %s" % self.remote_test_root)
        self.device.rm(self.remote_test_root, force=True, recursive=True)

        if self.config['power_test']:
            enable_charging(self.device)

        super(WebExtensionAndroid, self).clean_up()
