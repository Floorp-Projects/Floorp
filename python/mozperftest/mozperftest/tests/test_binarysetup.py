#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from unittest import mock

import mozunit

from mozperftest.environment import SYSTEM
from mozperftest.tests.support import get_running_env
from mozperftest.utils import silence


def get_binarysetup_layer(layers):
    for layer in layers:
        if layer.__class__.__name__ == "BinarySetup":
            return layer
    return None


def test_binarysetup_binary_arg():
    args = {
        "flavor": "desktop-browser",
        "app": "firefox",
        "binary": "/path/to/binary",
        "no-version-producer": True,
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary == "/path/to/binary"


@mock.patch("mozperftest.system.setups.ON_TRY", new=False)
@mock.patch("mozperftest.utils.ON_TRY", new=False)
def test_binarysetup_desktop():
    args = {
        "flavor": "desktop-browser",
        "app": "firefox",
        "no-version-producer": True,
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]
    binarysetup = get_binarysetup_layer(system.layers)

    with system as layer, silence(system):
        mocked_binary_path = mock.MagicMock()
        mocked_binary_path.return_value = "/fake/path"
        binarysetup.mach_cmd.get_binary_path = mocked_binary_path
        layer(metadata)

    assert metadata.binary == "/fake/path"


@mock.patch("mozperftest.system.setups.ON_TRY", new=False)
@mock.patch("mozperftest.utils.ON_TRY", new=False)
@mock.patch("mozperftest.system.setups.subprocess")
def test_binarysetup_desktop_chrome(mocked_subprocess):
    args = {
        "flavor": "desktop-browser",
        "app": "chrome",
        "no-version-producer": True,
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    mocked_subprocess.check_output.return_value = "/fake/path/chrome".encode()
    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary == "/fake/path/chrome"


def test_binarysetup_mobile_firefox():
    args = {
        "flavor": "mobile-browser",
        "app": "fenix",
        "no-version-producer": True,
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary == "org.mozilla.fenix"


def test_binarysetup_mobile_chrome():
    args = {
        "flavor": "mobile-browser",
        "app": "chrome-m",
        "no-version-producer": True,
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary == "com.android.chrome"


def test_binarysetup_unknown_app():
    args = {
        "flavor": "mobile-browser",
        "app": "unknown",
        "no-version-producer": True,
    }

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary is None


if __name__ == "__main__":
    mozunit.main()
