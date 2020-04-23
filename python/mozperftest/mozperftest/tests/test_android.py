#!/usr/bin/env python
import mozunit
import pytest

from mozperftest.tests.support import get_running_env
from mozperftest.environment import SYSTEM
from mozperftest.system.android import DeviceError


def test_android_failure():
    mach_cmd, metadata, env = get_running_env(android=True)
    system = env.layers[SYSTEM]
    # this will fail since we don't have a phone connected
    with system as android, pytest.raises(DeviceError):
        android(metadata)


if __name__ == "__main__":
    mozunit.main()
