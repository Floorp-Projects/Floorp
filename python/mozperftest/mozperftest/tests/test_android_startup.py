import copy
import json
import pathlib
import random
import time
from datetime import date
from unittest import mock

import mozunit
import pytest
import requests

from mozperftest.system import android_startup
from mozperftest.system.android_startup import (
    AndroidStartUpInstallError,
    AndroidStartUpMatchingError,
    AndroidStartUpUnknownTestError,
)
from mozperftest.tests.support import (
    EXAMPLE_ANDROID_STARTUP_TEST,
    get_running_env,
    temp_file,
)

SAMPLE_APK_METADATA = {
    "name": "fenix_nightly_armeabi-v7a_2022_09_27.apk",
    "date": date(2022, 9, 27),
    "commit": "",
    "architecture": "armeabi-v7a",
    "product": "fenix",
}

ARGS = {
    "AndroidStartUp-test-name": "cold_view_nav_start",
    "AndroidStartUp-product": "fenix",
    "AndroidStartUp-release-channel": "nightly",
    "apk_metadata": SAMPLE_APK_METADATA,
    "test-date": "2023.01.01",
}


class FakeDevice:
    def __init__(self, **kwargs):
        self.name = ""
        pass

    def uninstall_app(self, app):
        pass

    def install_app(self, app_name):
        return app_name

    def is_app_installed(self, name):
        self.name = name
        if name == "is_app_installed_fail":
            return False
        else:
            return True

    def stop_application(self, package_id):
        pass

    def shell(self, cmd):
        pass

    def shell_output(self, cmd):
        if cmd == "logcat -d":
            return (
                "ActivityManager: Start proc 23943:org.mozilla.fenix/u0a283 \n"
                "ActivityManager: Start proc 23942:org.mozilla.fenix/u0a283 \n"
                "11-23 14:10:13.391 13135 13135 I "
                "GeckoSession: handleMessage GeckoView:PageStart uri=\n"
                "11-23 14:10:13.391 13135 13135 I "
                "GeckoSession: handleMessage GeckoView:PageStart uri="
            )
        if self.name == "name_for_intent_not_2_lines":
            return "3 \n lines \n not 2"
        elif self.name == "name_for_multiple_Totaltime_strings":
            return "2 lines but \n no TotalTime"
        elif self.name == "name_for_single_total_time":
            return "TotalTime: 123 \n test"


def setup_metadata(metadata, **kwargs):
    new_metadata = copy.copy(metadata)
    for key, value in kwargs.items():
        new_metadata[key] = value
    return new_metadata


def running_env(**kw):
    return get_running_env(flavor="mobile-browser", **kw)


def init_mocked_request(status_code, **kwargs):
    mock_data = {}
    for key, value in kwargs.items():
        mock_data["data"][key] = value
    mock_request = requests.Response()
    mock_request.status_code = status_code
    mock_request._content = json.dumps(mock_data).encode("utf-8")
    return mock_request


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("requests.get", return_value=init_mocked_request(200))
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_install_of_nightly_failed(*mocked):
    SAMPLE_APK_METADATA["name"] = "is_app_installed_fail"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpInstallError):
        test.run(metadata)
    SAMPLE_APK_METADATA["name"] = "fenix_nightly_armeabi-v7a_2022_09_27.apk"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    pass


@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_invalid_test_name(*mocked):
    ARGS["AndroidStartUp-test-name"] = "BAD TEST NAME"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpUnknownTestError):
        test.run(metadata)
    ARGS["AndroidStartUp-test-name"] = "cold_main_first_frame"
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_multiple_matching_lines(*mocked):
    SAMPLE_APK_METADATA["name"] = "name_for_intent_not_2_lines"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)
    SAMPLE_APK_METADATA["name"] = "fenix_nightly_armeabi-v7a_2022_09_27.apk"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_multiple_total_time_prefix(*mocked):
    SAMPLE_APK_METADATA["name"] = "name_for_multiple_Totaltime_strings"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)
    SAMPLE_APK_METADATA["name"] = "fenix_nightly_armeabi-v7a_2022_09_27.apk"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_multiple_start_proc_lines(*mocked):
    SAMPLE_APK_METADATA["name"] = "name_for_multiple_Totaltime_strings"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)
    SAMPLE_APK_METADATA["name"] = "fenix_nightly_armeabi-v7a_2022_09_27.apk"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
@mock.patch(
    "mozperftest.system.android_startup.AndroidStartUp.get_measurement",
    return_value=random.randint(500, 1000),
)
def test_perfherder_layer(*mocked):
    SAMPLE_APK_METADATA["name"] = "name_for_multiple_Totaltime_strings"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    test.run(metadata)


@mock.patch("mozperftest.system.android.Path")
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
@mock.patch(
    "mozperftest.system.android_startup.AndroidStartUp.get_measurement",
    return_value=random.randint(500, 1000),
)
def test_custom_apk_startup(get_measurement_mock, time_sleep_mock, path_mock):
    SAMPLE_APK_METADATA["name"] = "name_for_multiple_Totaltime_strings"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )

    with temp_file(name="user_upload.apk", content="") as sample_apk:
        sample_apk = pathlib.Path(sample_apk)
        path_mock.return_value = sample_apk

        with mock.patch(
            "mozperftest.system.android_startup.AndroidStartUp.run_tests"
        ) as _:
            test = android_startup.AndroidStartUp(env, mach_cmd)
            test.run_tests = lambda: True
            test.package_id = "FakeID"
            assert test.run_performance_analysis(SAMPLE_APK_METADATA)


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_get_measurement_from_nav_start_logcat_match_error(*mocked):
    SAMPLE_APK_METADATA["name"] = "name_for_single_total_time"
    ARGS["apk_metadata"] = SAMPLE_APK_METADATA
    ARGS["AndroidStartUp-test-name"] = "cold_view_nav_start"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)


if __name__ == "__main__":
    mozunit.main()
