#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

from __future__ import absolute_import
import datetime
import enum
import os
import subprocess
import sys
import time

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript
from mozharness.mozilla.automation import (
    EXIT_STATUS_DICT,
    TBPL_FAILURE,
)
from mozharness.mozilla.mozbase import MozbaseMixin
from mozharness.mozilla.testing.android import AndroidMixin
from mozharness.mozilla.testing.testbase import TestingMixin


class TestMode(enum.Enum):
    OPTIMIZED_SHADER_COMPILATION = 0
    UNOPTIMIZED_SHADER_COMPILATION = 1
    SHADER_TEST = 2
    REFTEST = 3


class AndroidWrench(TestingMixin, BaseScript, MozbaseMixin, AndroidMixin):
    def __init__(self, require_config_file=False):
        # code in BaseScript.__init__ iterates all the properties to attach
        # pre- and post-flight listeners, so we need _is_emulator be defined
        # before that happens. Doesn't need to be a real value though.
        self._is_emulator = None

        super(AndroidWrench, self).__init__()
        if self.device_serial is None:
            # Running on an emulator.
            self._is_emulator = True
            self.device_serial = "emulator-5554"
            self.use_gles3 = True
        else:
            # Running on a device, ensure self.is_emulator returns False.
            # The adb binary is preinstalled on the bitbar image and is
            # already on the $PATH.
            self._is_emulator = False
            self._adb_path = "adb"
        self._errored = False

    @property
    def is_emulator(self):
        """Overrides the is_emulator property on AndroidMixin."""
        if self._is_emulator is None:
            self._is_emulator = self.device_serial is None
        return self._is_emulator

    def activate_virtualenv(self):
        """Overrides the method on AndroidMixin to be a no-op, because the
        setup for wrench doesn't require a special virtualenv."""
        pass

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = {}

        abs_dirs["abs_work_dir"] = os.path.expanduser("~/.wrench")
        if os.environ.get("MOZ_AUTOMATION", "0") == "1":
            # In automation use the standard work dir if there is one
            parent_abs_dirs = super(AndroidWrench, self).query_abs_dirs()
            if "abs_work_dir" in parent_abs_dirs:
                abs_dirs["abs_work_dir"] = parent_abs_dirs["abs_work_dir"]

        abs_dirs["abs_blob_upload_dir"] = os.path.join(abs_dirs["abs_work_dir"], "logs")
        abs_dirs["abs_apk_path"] = os.environ.get(
            "WRENCH_APK", "gfx/wr/target/android-artifacts/debug/apk/wrench.apk"
        )
        abs_dirs["abs_reftests_path"] = os.environ.get(
            "WRENCH_REFTESTS", "gfx/wr/wrench/reftests"
        )
        if os.environ.get("MOZ_AUTOMATION", "0") == "1":
            fetches_dir = os.environ.get("MOZ_FETCHES_DIR")
            work_dir = (
                fetches_dir
                if fetches_dir and self.is_emulator
                else abs_dirs["abs_work_dir"]
            )
            abs_dirs["abs_sdk_dir"] = os.path.join(work_dir, "android-sdk-linux")
            abs_dirs["abs_avds_dir"] = os.path.join(work_dir, "android-device")
            abs_dirs["abs_bundletool_path"] = os.path.join(work_dir, "bundletool.jar")
        else:
            mozbuild_path = os.environ.get(
                "MOZBUILD_STATE_PATH", os.path.expanduser("~/.mozbuild")
            )
            mozbuild_sdk = os.environ.get(
                "ANDROID_SDK_HOME", os.path.join(mozbuild_path, "android-sdk-linux")
            )
            abs_dirs["abs_sdk_dir"] = mozbuild_sdk
            avds_dir = os.environ.get(
                "ANDROID_EMULATOR_HOME", os.path.join(mozbuild_path, "android-device")
            )
            abs_dirs["abs_avds_dir"] = avds_dir
            abs_dirs["abs_bundletool_path"] = os.path.join(
                mozbuild_path, "bundletool.jar"
            )

        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def logcat_start(self):
        """Overrides logcat_start in android.py - ensures any pre-existing logcat
        is cleared before starting to record the new logcat. This is helpful
        when running multiple times in a local emulator."""
        logcat_cmd = [self.adb_path, "-s", self.device_serial, "logcat", "-c"]
        self.info(" ".join(logcat_cmd))
        subprocess.check_call(logcat_cmd)
        super(AndroidWrench, self).logcat_start()

    def wait_until_process_done(self, process_name, timeout):
        """Waits until the specified process has exited. Polls the process list
            every 5 seconds until the process disappears.

        :param process_name: string containing the package name of the
            application.
        :param timeout: integer specifying the maximum time in seconds
            to wait for the application to finish.
        :returns: boolean - True if the process exited within the indicated
            timeout, False if the process had not exited by the timeout.
        """
        end_time = datetime.datetime.now() + datetime.timedelta(seconds=timeout)
        while self.device.process_exist(process_name, timeout=timeout):
            if datetime.datetime.now() > end_time:
                stop_cmd = [
                    self.adb_path,
                    "-s",
                    self.device_serial,
                    "shell",
                    "am",
                    "force-stop",
                    process_name,
                ]
                subprocess.check_call(stop_cmd)
                return False
            time.sleep(5)

        return True

    def setup_sdcard(self, test_mode):
        # Note that we hard-code /sdcard/wrench as the path here, rather than
        # using something like self.device.test_root, because it needs to be
        # kept in sync with the path hard-coded inside the wrench source code.
        self.device.rm("/sdcard/wrench", recursive=True, force=True)
        self.device.mkdir("/sdcard/wrench", parents=True)
        if test_mode == TestMode.REFTEST:
            self.device.push(
                self.query_abs_dirs()["abs_reftests_path"], "/sdcard/wrench/reftests"
            )
        args_file = os.path.join(self.query_abs_dirs()["abs_work_dir"], "wrench_args")
        with open(args_file, "w") as argfile:
            if self.is_emulator:
                argfile.write("env: WRENCH_REFTEST_CONDITION_EMULATOR=1\n")
            else:
                argfile.write("env: WRENCH_REFTEST_CONDITION_DEVICE=1\n")
            if test_mode == TestMode.OPTIMIZED_SHADER_COMPILATION:
                argfile.write("--precache test_init")
            elif test_mode == TestMode.UNOPTIMIZED_SHADER_COMPILATION:
                argfile.write("--precache --use-unoptimized-shaders test_init")
            elif test_mode == TestMode.SHADER_TEST:
                argfile.write("--precache test_shaders")
            elif test_mode == TestMode.REFTEST:
                argfile.write("reftest")
        self.device.push(args_file, "/sdcard/wrench/args")

    def run_tests(self, timeout):
        self.timed_screenshots(None)
        self.device.launch_application(
            app_name="org.mozilla.wrench",
            activity_name="android.app.NativeActivity",
            intent=None,
        )
        self.info("App launched")
        done = self.wait_until_process_done("org.mozilla.wrench", timeout=timeout)
        if not done:
            self._errored = True
            self.error("Wrench still running after timeout")

    def scrape_logcat(self):
        """Wrench will dump the test output to logcat, but for convenience we
        want it to show up in the main log. So we scrape it out of the logcat
        and dump it to our own log. Note that all output from wrench goes
        through the cargo-apk glue stuff, which uses the RustAndroidGlueStdouterr
        tag on the output. Also it limits the line length to 512 bytes
        (including the null terminator). For reftest unexpected-fail output
        this means that the base64 image dump gets wrapped over multiple
        lines, so part of what this function does is unwrap that so that the
        resulting log is readable by the reftest analyzer."""

        with open(self.logcat_path(), "r", encoding="utf-8") as f:
            self.info("=== scraped logcat output ===")
            tag = "RustAndroidGlueStdouterr: "
            long_line = None
            for line in f:
                tag_index = line.find(tag)
                if tag_index == -1:
                    # not a line we care about
                    continue
                line = line[tag_index + len(tag) :].rstrip()
                if (
                    long_line is None
                    and "REFTEST " not in line
                    and "panicked" not in line
                ):
                    # non-interesting line
                    continue
                if long_line is not None:
                    # continuation of a wrapped line
                    long_line += line
                if len(line) >= 511:
                    if long_line is None:
                        # start of a new long line
                        long_line = line
                    # else "middle" of a long line that keeps going to the next line
                    continue
                # this line doesn't wrap over to the next, so we can
                # print it
                if long_line is not None:
                    line = long_line
                    long_line = None
                if "UNEXPECTED-FAIL" in line or "panicked" in line:
                    self._errored = True
                    self.error(line)
                else:
                    self.info(line)
            self.info("=== end scraped logcat output ===")
            self.info("(see logcat artifact for full logcat")

    def setup_emulator(self):
        avds_dir = self.query_abs_dirs()["abs_avds_dir"]
        if not os.path.exists(avds_dir):
            self.error("Unable to find android AVDs at %s" % avds_dir)
            return

        sdk_path = self.query_abs_dirs()["abs_sdk_dir"]
        if not os.path.exists(sdk_path):
            self.error("Unable to find android SDK at %s" % sdk_path)
            return
        self.start_emulator()

    def do_test(self):
        if self.is_emulator:
            self.setup_emulator()

        self.verify_device()
        self.info("Logging device properties...")
        self.info(self.shell_output("getprop"))
        self.info("Installing APK...")
        self.install_android_app(self.query_abs_dirs()["abs_apk_path"], replace=True)

        if not self._errored:
            self.info("Setting up SD card...")
            self.setup_sdcard(TestMode.OPTIMIZED_SHADER_COMPILATION)
            self.info("Running optimized shader compilation tests...")
            self.run_tests(60)

        if not self._errored:
            self.info("Setting up SD card...")
            self.setup_sdcard(TestMode.UNOPTIMIZED_SHADER_COMPILATION)
            self.info("Running unoptimized shader compilation tests...")
            self.run_tests(60)

        if not self._errored:
            self.info("Setting up SD card...")
            self.setup_sdcard(TestMode.SHADER_TEST)
            self.info("Running shader tests...")
            self.run_tests(60 * 5)

        if not self._errored:
            self.info("Setting up SD card...")
            self.setup_sdcard(TestMode.REFTEST)
            self.info("Running reftests...")
            self.run_tests(60 * 30)

        self.info("Tests done; parsing logcat...")
        self.logcat_stop()
        self.scrape_logcat()
        self.info("All done!")

    def check_errors(self):
        if self._errored:
            self.info("Errors encountered, terminating with error code...")
            exit(EXIT_STATUS_DICT[TBPL_FAILURE])


if __name__ == "__main__":
    test = AndroidWrench()
    test.do_test()
    test.check_errors()
