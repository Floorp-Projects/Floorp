#!/usr/bin/env python
import mozunit
import pytest
from unittest import mock

from mozperftest.tests.support import get_running_env, requests_content, temp_file
from mozperftest.environment import SYSTEM
from mozperftest.system.android import DeviceError
from mozperftest.system.android_perf_tuner import PerformanceTuner
from mozperftest.utils import silence


class FakeDevice:
    def __init__(self, **args):
        self.apps = []
        self._logger = mock.MagicMock()
        self._have_su = True
        self._have_android_su = True

    def clear_logcat(self, *args, **kwargs):
        return True

    def shell_output(self, *args, **kwargs):
        return "A Fake Device"

    def shell_bool(self, *args, **kwargs):
        return True

    def uninstall_app(self, apk_name):
        return True

    def install_app(self, apk, replace=True):
        if apk not in self.apps:
            self.apps.append(apk)

    def is_app_installed(self, app_name):
        return True


@mock.patch("mozperftest.system.android.ADBLoggedDevice", new=FakeDevice)
def test_android():
    args = {
        "flavor": "mobile-browser",
        "android-install-apk": ["this.apk"],
        "android": True,
        "android-timeout": 30,
        "android-capture-adb": "stdout",
        "android-app-name": "org.mozilla.fenix",
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    with system as android, silence(system):
        android(metadata)


@mock.patch("mozperftest.system.android.ADBLoggedDevice")
def test_android_perf_tuning_rooted(device):
    # Check to make sure that performance tuning runs
    # on rooted devices correctly
    device._have_su = True
    device._have_android_su = True
    with mock.patch(
        "mozperftest.system.android_perf_tuner.PerformanceTuner.set_kernel_performance_parameters"
    ) as mockfunc:
        tuner = PerformanceTuner(device)
        tuner.tune_performance()
        mockfunc.assert_called()


@mock.patch("mozperftest.system.android.ADBLoggedDevice")
def test_android_perf_tuning_nonrooted(device):
    # Check to make sure that performance tuning runs
    # on non-rooted devices correctly
    device._have_su = False
    device._have_android_su = False
    with mock.patch(
        "mozperftest.system.android_perf_tuner.PerformanceTuner.set_kernel_performance_parameters"
    ) as mockfunc:
        tuner = PerformanceTuner(device)
        tuner.tune_performance()
        mockfunc.assert_not_called()


@mock.patch("mozperftest.system.android_perf_tuner.PerformanceTuner")
@mock.patch("mozperftest.system.android.ADBLoggedDevice")
def test_android_with_perftuning(device, tuner):
    args = {
        "flavor": "mobile-browser",
        "android-install-apk": ["this.apk"],
        "android": True,
        "android-timeout": 30,
        "android-capture-adb": "stdout",
        "android-app-name": "org.mozilla.fenix",
        "android-perf-tuning": True,
    }
    tuner.return_value = tuner

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    with system as android, silence(system):
        android(metadata)

    # Make sure the tuner was actually called
    tuner.tune_performance.assert_called()


def test_android_failure():
    # no patching so it'll try for real and fail
    args = {
        "flavor": "mobile-browser",
        "android-install-apk": ["this"],
        "android": True,
        "android-timeout": 120,
        "android-app-name": "org.mozilla.fenix",
        "android-capture-adb": "stdout",
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    with system as android, silence(system), pytest.raises(DeviceError):
        android(metadata)


@mock.patch("mozperftest.utils.requests.get", new=requests_content())
@mock.patch("mozperftest.system.android.ADBLoggedDevice")
def test_android_apk_alias(device):
    args = {
        "flavor": "mobile-browser",
        "android-install-apk": ["fenix_fennec_nightly_armeabi_v7a"],
        "android": True,
        "android-app-name": "org.mozilla.fennec_aurora",
        "android-capture-adb": "stdout",
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    with system as android, silence(system):
        android(metadata)
    # XXX really ?
    assert device.mock_calls[1][1][0] == "org.mozilla.fennec_aurora"
    assert device.mock_calls[2][1][0].endswith("target.apk")


@mock.patch("mozperftest.utils.requests.get", new=requests_content())
@mock.patch("mozperftest.system.android.ADBLoggedDevice")
def test_android_timeout(device):
    args = {
        "flavor": "mobile-browser",
        "android-install-apk": ["gve_nightly_api16"],
        "android": True,
        "android-timeout": 60,
        "android-app-name": "org.mozilla.geckoview_example",
        "android-capture-adb": "stdout",
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    with system as android, silence(system):
        android(metadata)
    options = device.mock_calls[0][-1]
    assert options["timeout"] == 60


@mock.patch("mozperftest.utils.requests.get", new=requests_content())
def test_android_log_adb():
    with temp_file() as log_adb:
        args = {
            "flavor": "mobile-browser",
            "android-install-apk": ["gve_nightly_api16"],
            "android": True,
            "android-timeout": 60,
            "android-app-name": "org.mozilla.geckoview_example",
            "android-capture-adb": log_adb,
        }

        mach_cmd, metadata, env = get_running_env(**args)
        system = env.layers[SYSTEM]
        with system as android, silence(system), pytest.raises(DeviceError):
            android(metadata)
        with open(log_adb) as f:
            assert "DEBUG ADBLoggedDevice" in f.read()


@mock.patch("mozperftest.utils.requests.get", new=requests_content())
@mock.patch("mozperftest.system.android.ADBLoggedDevice")
def test_android_log_cat(device):
    with temp_file() as log_cat:
        args = {
            "flavor": "mobile-browser",
            "android-install-apk": ["gve_nightly_api16"],
            "android": True,
            "android-timeout": 60,
            "android-app-name": "org.mozilla.geckoview_example",
            "android-capture-logcat": log_cat,
            "android-clear-logcat": True,
            "android-capture-adb": "stdout",
        }

        mach_cmd, metadata, env = get_running_env(**args)
        system = env.layers[SYSTEM]
        andro = system.layers[1]

        with system as layer, silence(system):
            andro.device = device
            andro.device.get_logcat = mock.Mock(result_value=[])
            layer(metadata)

        andro.device.get_logcat.assert_called()
        andro.device.clear_logcat.assert_called()


if __name__ == "__main__":
    mozunit.main()
