# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import pathlib
import re
import statistics
import time
from datetime import datetime

import mozdevice

from mozperftest.utils import ON_TRY

from .android import AndroidDevice

DATETIME_FORMAT = "%Y.%m.%d"
PAGE_START_MOZ = re.compile("GeckoSession: handleMessage GeckoView:PageStart uri=")

PROD_FENIX = "fenix"
PROD_FOCUS = "focus"
PROD_GVEX = "geckoview_example"
PROD_CHRM = "chrome-m"
MOZILLA_PRODUCTS = [PROD_FENIX, PROD_FOCUS, PROD_GVEX]

MEASUREMENT_DATA = ["mean", "median", "standard_deviation"]
OLD_VERSION_FOCUS_PAGE_START_LINE_COUNT = 3
NEW_VERSION_FOCUS_PAGE_START_LINE_COUNT = 2
STDOUT_LINE_COUNT = 2

TEST_COLD_MAIN_FF = "cold_main_first_frame"
TEST_COLD_MAIN_RESTORE = "cold_main_session_restore"
TEST_COLD_VIEW_FF = "cold_view_first_frame"
TEST_COLD_VIEW_NAV_START = "cold_view_nav_start"
TEST_URI = "https://example.com"

PROD_TO_CHANNEL_TO_PKGID = {
    PROD_FENIX: {
        "nightly": "org.mozilla.fenix",
        "beta": "org.mozilla.firefox.beta",
        "release": "org.mozilla.firefox",
        "debug": "org.mozilla.fenix.debug",
    },
    PROD_FOCUS: {
        "nightly": "org.mozilla.focus.nightly",
        "beta": "org.mozilla.focus.beta",  # only present since post-fenix update.
        "release": "org.mozilla.focus",
        "debug": "org.mozilla.focus.debug",
    },
    PROD_GVEX: {
        "nightly": "org.mozilla.geckoview_example",
        "release": "org.mozilla.geckoview_example",
    },
    PROD_CHRM: {
        "nightly": "com.android.chrome",
        "release": "com.android.chrome",
    },
}
TEST_LIST = [
    "cold_main_first_frame",
    "cold_view_nav_start",
    "cold_view_first_frame",
    "cold_main_session_restore",
]
# "cold_view_first_frame", "cold_main_session_restore" are 2 disabled tests(broken)


class AndroidStartUpDownloadError(Exception):
    """Failure downloading Firefox Nightly APK"""

    pass


class AndroidStartUpInstallError(Exception):
    """Failure installing Firefox on the android device"""

    pass


class AndroidStartUpUnknownTestError(Exception):
    """
    Test name provided is not one avaiable to test, this is either because
    the test is currently not being tested or a typo in the spelling
    """

    pass


class AndroidStartUpMatchingError(Exception):
    """
    We expected a certain number of matches but did not get them
    """

    pass


class AndroidStartUpData:
    def open_data(self, data):
        return {
            "name": "AndroidStartUp",
            "subtest": data["name"],
            "data": [
                {"file": "android_startup", "value": value, "xaxis": xaxis}
                for xaxis, value in enumerate(data["values"])
            ],
            "shouldAlert": True,
        }

    def transform(self, data):
        return data

    merge = transform


class AndroidStartUp(AndroidDevice):
    name = "AndroidStartUp"
    activated = False
    arguments = {
        "test-name": {
            "type": str,
            "default": "",
            "help": "This is the startup android test that will be run on the mobile device",
        },
        "product": {
            "type": str,
            "default": "",
            "help": "This is the product that you are testing the startup of(ex. chrome, fenix, focus)",
        },
    }

    def __init__(self, env, mach_cmd):
        super(AndroidStartUp, self).__init__(env, mach_cmd)
        self.android_activity = None
        self.capture_logcat = self.capture_file = self.app_name = None
        self.device = mozdevice.ADBDevice(use_root=False)

    def run(self, metadata):
        options = metadata.script["options"]
        self.test_name = self.get_arg("test-name")
        self.product = self.get_arg("product")
        self.release_channel = options["test_parameters"]["release_channel"]
        self.architecture = options["test_parameters"]["architecture"]
        self.single_date = options["test_parameters"]["single_date"]
        self.date_range = options["test_parameters"]["date_range"]
        self.startup_cache = options["test_parameters"]["startup_cache"]
        self.test_cycles = options["test_parameters"]["test_cycles"]
        self.package_id = PROD_TO_CHANNEL_TO_PKGID[self.product][self.release_channel]
        self.proc_start = re.compile(
            rf"ActivityManager: Start proc \d+:{self.package_id}/"
        )
        self.key_name = f"{self.product}_nightly_{self.architecture}.apk"

        self.get_measurements(metadata)

        # Cleanup
        if self.product in MOZILLA_PRODUCTS:
            self.device.uninstall_app(self.package_id)

        return metadata

    def get_measurements(self, metadata):
        measurements = self.install_apk_onto_device_and_run()
        self.add_to_metadata(measurements, metadata)

    def add_to_metadata(self, measurements, metadata):
        if measurements is not None:
            for key, value in measurements.items():
                metadata.add_result(
                    {
                        "name": f"AndroidStartup:{self.product}",
                        "framework": {"name": "mozperftest"},
                        "transformer": "mozperftest.system.android_startup:AndroidStartUpData",
                        "shouldAlert": True,
                        "results": [
                            {
                                "values": [value],
                                "name": key,
                                "shouldAlert": True,
                            }
                        ],
                    }
                )

    def install_apk_onto_device_and_run(self):
        if self.product in MOZILLA_PRODUCTS and ON_TRY:
            # Installing the application on the device and getting ready to run the tests
            if self.custom_apk_exists():
                install_path = self.custom_apk_path
            else:
                install_path = str(self.get_install_path())
            self.install_application([install_path], self.package_id)
        elif not self.device.is_app_installed(self.package_id):
            raise AndroidStartUpInstallError("Please verify your apk is installed")
        return self.run_tests()

    def get_install_path(self):
        prefix = pathlib.Path(os.getenv("MOZ_FETCHES_DIR", ""))
        if self.product == "fenix":
            return prefix / "target.arm64-v8a.apk"
        elif self.product == "geckoview_example":
            return prefix / "geckoview_example.apk"
        elif self.product == "focus":
            return prefix / "target.arm64-v8a.apk"
        else:
            raise AndroidStartUpInstallError("Unknown product")

    def run_tests(self):
        measurements = {}
        # Iterate through the tests in the test list
        self.info(f"Running {self.test_name} on {self.package_id}...")
        time.sleep(self.get_warmup_delay_seconds())
        self.skip_onboarding(self.test_name)
        test_measurements = []

        for i in range(self.test_cycles):
            start_cmd_args = self.get_start_cmd(self.test_name)
            self.info(start_cmd_args)
            self.device.stop_application(self.package_id)
            time.sleep(1)
            self.info(f"iteration {i + 1}")
            self.device.shell("logcat -c")
            process = self.device.shell_output(start_cmd_args).splitlines()
            test_measurements.append(self.get_measurement(self.test_name, process))

        self.device.stop_application(self.package_id)
        self.info(f"{self.test_name}: {str(test_measurements)}")
        measurements[f"{self.test_name}.{MEASUREMENT_DATA[0]}"] = statistics.mean(
            test_measurements
        )
        self.info(f"Mean: {statistics.mean(test_measurements)}")
        measurements[f"{self.test_name}.{MEASUREMENT_DATA[1]}"] = statistics.median(
            test_measurements
        )
        self.info(f"Median: {statistics.median(test_measurements)}")
        if self.test_cycles > 1:
            measurements[f"{self.test_name}.{MEASUREMENT_DATA[2]}"] = statistics.stdev(
                test_measurements
            )
            self.info(f"Standard Deviation: {statistics.stdev(test_measurements)}")

        return measurements

    def get_measurement(self, test_name, stdout):
        if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_VIEW_FF]:
            return self.get_measurement_from_am_start_log(stdout)
        elif (
            test_name in [TEST_COLD_VIEW_NAV_START, TEST_COLD_MAIN_RESTORE]
            and self.product in MOZILLA_PRODUCTS
        ):
            # We must sleep until the Navigation::Start event occurs. If we don't
            # the script will fail. This can take up to 14s on the G5
            time.sleep(17)
            proc = self.device.shell_output("logcat -d")
            return self.get_measurement_from_nav_start_logcat(proc)
        else:
            raise Exception(
                "invalid test settings selected, please double check that "
                "the test name is valid and that the test is supported for "
                "the browser you are testing"
            )

    def get_measurement_from_am_start_log(self, stdout):
        total_time_prefix = "TotalTime: "
        matching_lines = [line for line in stdout if line.startswith(total_time_prefix)]
        if len(matching_lines) != 1:
            raise AndroidStartUpMatchingError(
                f"Each run should only have 1 {total_time_prefix}."
                f"However, this run unexpectedly had {matching_lines} matching lines"
            )
        duration = int(matching_lines[0][len(total_time_prefix) :])
        return duration

    def get_measurement_from_nav_start_logcat(self, process_output):
        def __line_to_datetime(line):
            date_str = " ".join(line.split(" ")[:2])  # e.g. "05-18 14:32:47.366"
            # strptime needs microseconds. logcat outputs millis so we append zeroes
            date_str_with_micros = date_str + "000"
            return datetime.strptime(date_str_with_micros, "%m-%d %H:%M:%S.%f")

        def __get_proc_start_datetime():
            # This regex may not work on older versions of Android: we don't care
            # yet because supporting older versions isn't in our requirements.
            proc_start_lines = [line for line in lines if self.proc_start.search(line)]
            if len(proc_start_lines) != 1:
                raise AndroidStartUpMatchingError(
                    f"Expected to match 1 process start string but matched {len(proc_start_lines)}"
                )
            return __line_to_datetime(proc_start_lines[0])

        def __get_page_start_datetime():
            page_start_lines = [line for line in lines if PAGE_START_MOZ.search(line)]
            page_start_line_count = len(page_start_lines)
            page_start_assert_msg = "found len=" + str(page_start_line_count)

            # In focus versions <= v8.8.2, it logs 3 PageStart lines and these include actual uris.
            # We need to handle our assertion differently due to the different line count. In focus
            # versions >= v8.8.3, this measurement is broken because the logcat were removed.
            is_old_version_of_focus = (
                "about:blank" in page_start_lines[0] and self.product == PROD_FOCUS
            )
            if is_old_version_of_focus:
                assert (
                    page_start_line_count
                    == OLD_VERSION_FOCUS_PAGE_START_LINE_COUNT  # should be 3
                ), page_start_assert_msg  # Lines: about:blank, target URL, target URL.
            else:
                assert (
                    page_start_line_count
                    == NEW_VERSION_FOCUS_PAGE_START_LINE_COUNT  # Should be 2
                ), page_start_assert_msg  # Lines: about:blank, target URL.
            return __line_to_datetime(
                page_start_lines[1]
            )  # 2nd PageStart is for target URL.

        lines = process_output.split("\n")
        elapsed_seconds = (
            __get_page_start_datetime() - __get_proc_start_datetime()
        ).total_seconds()
        elapsed_millis = round(elapsed_seconds * 1000)
        return elapsed_millis

    def get_warmup_delay_seconds(self):
        """
        We've been told the start up cache is populated ~60s after first start up. As such,
        we should measure start up with the start up cache populated. If the
        args say we shouldn't wait, we only wait a short duration ~= visual completeness.
        """
        return 60 if self.startup_cache else 5

    def get_start_cmd(self, test_name):
        intent_action_prefix = "android.intent.action.{}"
        if test_name in [TEST_COLD_MAIN_FF, TEST_COLD_MAIN_RESTORE]:
            intent = (
                f"-a {intent_action_prefix.format('MAIN')} "
                f"-c android.intent.category.LAUNCHER"
            )
        elif test_name in [TEST_COLD_VIEW_FF, TEST_COLD_VIEW_NAV_START]:
            intent = f"-a {intent_action_prefix.format('VIEW')} -d {TEST_URI}"
        else:
            raise AndroidStartUpUnknownTestError(
                "Unknown test provided please double check the test name and spelling"
            )

        # You can't launch an app without an pkg_id/activity pair
        component_name = self.get_component_name_for_intent(intent)
        cmd = f"am start-activity -W -n {component_name} {intent} "

        # If focus skip onboarding: it is not stateful so must be sent for every cold start intent
        if self.product == PROD_FOCUS:
            cmd += "--ez performancetest true"

        return cmd

    def get_component_name_for_intent(self, intent):
        resolve_component_args = (
            f"cmd package resolve-activity --brief {intent} {self.package_id}"
        )
        result_output = self.device.shell_output(resolve_component_args)
        stdout = result_output.splitlines()
        if len(stdout) != STDOUT_LINE_COUNT:  # Should be 2
            if "No activity found" in stdout:
                raise AndroidStartUpInstallError("Please verify your apk is installed")
            raise AndroidStartUpMatchingError(f"expected 2 lines. Got: {stdout}")
        return stdout[1]

    def skip_onboarding(self, test_name):
        self.enable_notifications(self.package_id)

        if self.product == PROD_FOCUS or test_name not in {
            TEST_COLD_MAIN_FF,
            TEST_COLD_MAIN_RESTORE,
        }:
            return

        if self.product in MOZILLA_PRODUCTS:
            self.skip_app_onboarding(self.package_id)
