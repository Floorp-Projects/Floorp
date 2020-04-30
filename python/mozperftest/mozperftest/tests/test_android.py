#!/usr/bin/env python
import mozunit
import pytest
import mock

from mozperftest.tests.support import get_running_env
from mozperftest.environment import SYSTEM
from mozperftest.system.android import DeviceError


class FakeDevice:
    def __init__(self, **args):
        self.apps = []

    def install_app(self, apk, replace=True):
        if apk not in self.apps:
            self.apps.append(apk)

    def is_app_installed(self, app_name):
        return True


@mock.patch("mozperftest.system.android.ADBDevice", new=FakeDevice)
def test_android():
    args = {
        "android-install-apk": ["this"],
        "android": True,
        "android-app-name": "org.mozilla.fenix",
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    with system as android:
        android(metadata)


def test_android_failure():
    # no patching so it'll try for real and fail
    args = {
        "android-install-apk": ["this"],
        "android": True,
        "android-app-name": "org.mozilla.fenix",
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    with system as android, pytest.raises(DeviceError):
        android(metadata)


if __name__ == "__main__":
    mozunit.main()
