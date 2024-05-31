import pathlib
import random
import time
from unittest import mock

import mozunit
import pytest

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

catch_command = False

ARGS = {
    "AndroidStartUp-test-name": "cold_main_first_frame",
    "AndroidStartUp-product": "fenix",
}


class FakeDevice:
    def __init__(self, **kwargs):
        self.name = ""
        pass

    def uninstall_app(self, app):
        pass

    def install_app(self, app_name, replace=True):
        return app_name

    def is_app_installed(self, name):
        self.name = name
        if "not_installed_name" == name or "geckoview_example.apk" == name:
            return False
        else:
            return True

    def stop_application(self, package_id):
        pass

    def shell(self, cmd):
        pass

    def shell_output(self, cmd):
        if self.name == "bad_name":
            return ["No activity found"]
        if cmd == "logcat -d":
            return (
                "ActivityManager: Start proc 23943:org.mozilla.fenix/u0a283 \n"
                "ActivityManager: Start proc 23942:org.mozilla.fenix/u0a283 \n"
                "11-23 14:10:13.391 13135 13135 I "
                "GeckoSession: handleMessage GeckoView:PageStart uri=\n"
                "11-23 14:10:13.391 13135 13135 I "
                "GeckoSession: handleMessage GeckoView:PageStart uri="
            )
        if (
            cmd
            == "cmd package resolve-activity --brief -a android.intent.action.VIEW -d https://example.com org.mozilla.focus"
            and "focus" in self.name
        ):
            return "3 \n lines \n not 2"
        elif self.name == "name_for_multiple_Totaltime_strings":
            return "2 lines but \n no TotalTime"
        elif self.name == "name_for_single_total_time":
            return "TotalTime: 123 \n test"
        else:
            return "2 lines that \n is all"


def running_env(**kw):
    return get_running_env(flavor="mobile-browser", **kw)


@mock.patch(
    "mozperftest.system.android_startup.PROD_TO_CHANNEL_TO_PKGID",
    {"app_name": {"nightly": "not_installed_name"}},
)
@mock.patch(
    "mozperftest.system.android_startup.MOZILLA_PRODUCTS",
    ["app_name"],
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.system.android_startup.ON_TRY", True)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_install_failed_CI(*mocked):
    ARGS["AndroidStartUp-product"] = "app_name"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpInstallError):
        test.run(metadata)
    pass


@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.system.android_startup.ON_TRY", True)
@mock.patch("time.sleep", return_value=time.sleep(0))
@mock.patch(
    "mozperftest.system.android_startup.MOZILLA_PRODUCTS",
    ["app_name"],
)
@mock.patch(
    "mozperftest.system.android_startup.PROD_TO_CHANNEL_TO_PKGID",
    {"app_name": {"nightly": "not_installed_name"}},
)
def test_app_not_found_CI(*mocked):
    ARGS["AndroidStartUp-product"] = "app_name"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpInstallError):
        test.run(metadata)
    pass


@mock.patch(
    "mozperftest.system.android_startup.PROD_TO_CHANNEL_TO_PKGID",
    {"app_name": {"nightly": "not_installed_name"}},
)
@mock.patch(
    "mozperftest.system.android_startup.MOZILLA_PRODUCTS",
    ["app_name"],
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.utils.ON_TRY", False)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_not_installed_locally(*mocked):
    ARGS["AndroidStartUp-product"] = "app_name"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpInstallError):
        test.run(metadata)
    pass


@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.system.android_startup.ON_TRY", False)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_invalid_test_name(*mocked):
    ARGS["AndroidStartUp-product"] = "fenix"
    ARGS["AndroidStartUp-test-name"] = "Invalid Test name"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpUnknownTestError):
        test.run(metadata)
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.system.android_startup.ON_TRY", False)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_multiple_matching_lines(*mocked):
    ARGS["AndroidStartUp-product"] = "focus"
    ARGS["AndroidStartUp-test-name"] = "cold_view_nav_start"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.system.android_startup.ON_TRY", False)
@mock.patch("time.sleep", return_value=time.sleep(0))
@mock.patch("mozperftest.utils.ON_TRY", True)
def test_multiple_total_time_prefix(*mocked):
    ARGS["AndroidStartUp-test-name"] = "cold_main_first_frame"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.system.android_startup.ON_TRY", False)
@mock.patch("time.sleep", return_value=time.sleep(0))
def test_multiple_start_proc_lines(*mocked):
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)
    pass


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("mozperftest.system.android_startup.ON_TRY", False)
@mock.patch("time.sleep", return_value=time.sleep(0))
@mock.patch(
    "mozperftest.system.android_startup.AndroidStartUp.get_measurement",
    return_value=random.randint(500, 1000),
)
def test_perfherder_layer(*mocked):
    ARGS["AndroidStartUp-product"] = "fenix"
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
@mock.patch("mozperftest.system.android_startup.ON_TRY", False)
def test_custom_apk_startup(get_measurement_mock, time_sleep_mock, path_mock):
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )

    with temp_file(name="user_upload.apk", content="") as sample_apk:
        sample_apk_path = pathlib.Path(sample_apk)
        path_mock.return_value = sample_apk_path

        with mock.patch(
            "mozperftest.system.android_startup.AndroidStartUp.run_tests"
        ) as _:
            test = android_startup.AndroidStartUp(env, mach_cmd)
            test.run_tests = lambda: True
            test.package_id = "FakeID"
            test.product = "fenix"
            assert test.install_apk_onto_device_and_run()


@mock.patch(
    "mozperftest.system.android.AndroidDevice.custom_apk_exists", new=lambda x: False
)
@mock.patch(
    "mozdevice.ADBDevice",
    new=FakeDevice,
)
@mock.patch("time.sleep", return_value=time.sleep(0))
@mock.patch("mozperftest.system.android_startup.ON_TRY", False)
def test_get_measurement_from_nav_start_logcat_match_error(*mocked):
    ARGS["AndroidStartUp-product"] = "focus"
    ARGS["AndroidStartUp-test-name"] = "cold_view_nav_start"
    mach_cmd, metadata, env = running_env(
        tests=[str(EXAMPLE_ANDROID_STARTUP_TEST)], **ARGS
    )
    test = android_startup.AndroidStartUp(env, mach_cmd)
    with pytest.raises(AndroidStartUpMatchingError):
        test.run(metadata)


if __name__ == "__main__":
    mozunit.main()
