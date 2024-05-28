#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pathlib
from unittest import mock

import mozunit

from mozperftest.environment import SYSTEM
from mozperftest.tests.support import get_running_env
from mozperftest.utils import silence

HERE = pathlib.Path(__file__).parent
WINDOWS_SAMPLE = HERE / "data" / "windows_version_sample.txt"


@mock.patch("mozperftest.system.setups.mozversion")
def test_versionproducer_mozversion_desktop(mocked_mozversion):
    args = {"flavor": "desktop-browser", "app": "firefox", "no-binary-setup": True}

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    metadata.binary = "/path/to/binary"
    mocked_mozversion.get_version.return_value = {"application_version": "9000"}
    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary == "/path/to/binary"
    assert metadata.binary_version == "9000"


@mock.patch("mozperftest.system.setups.subprocess")
@mock.patch("mozperftest.system.setups.platform")
@mock.patch("mozperftest.system.setups.mozversion")
def test_versionproducer_fallback_linux(
    mocked_mozversion, mocked_platform, mocked_subprocess
):
    args = {"flavor": "desktop-browser", "app": "firefox", "no-binary-setup": True}

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    mocked_mozversion.get_version = lambda x: Exception("")
    mocked_platform.system.return_value = "linux"

    mocked_proc = mock.MagicMock()
    mocked_proc.stdout = "Mozilla Firefox 125.0.3"
    mocked_subprocess.run.return_value = mocked_proc

    metadata.binary = "/path/to/binary"
    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary_version == "125.0.3"


@mock.patch("mozperftest.system.setups.subprocess")
@mock.patch("mozperftest.system.setups.platform")
@mock.patch("mozperftest.system.setups.mozversion")
def test_versionproducer_fallback_windows(
    mocked_mozversion, mocked_platform, mocked_subprocess
):
    args = {"flavor": "desktop-browser", "app": "firefox", "no-binary-setup": True}

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    mocked_mozversion.get_version = lambda x: Exception("")
    mocked_platform.system.return_value = "windows"

    with WINDOWS_SAMPLE.open() as f:
        sample_txt = f.read()
    mocked_subprocess.check_output.return_value = sample_txt.encode()

    metadata.binary = "/path/to/binary"
    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary_version == "125.0.3.8881"


@mock.patch("mozperftest.system.setups.pathlib.Path.open")
@mock.patch("mozperftest.system.setups.platform")
@mock.patch("mozperftest.system.setups.mozversion")
def test_versionproducer_fallback_mac(mocked_mozversion, mocked_platform, mocked_open):
    args = {"flavor": "desktop-browser", "app": "firefox", "no-binary-setup": True}

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    mocked_mozversion.get_version = lambda x: Exception("")
    mocked_platform.system.return_value = "mac"

    with mock.patch("plistlib.load") as mocked_load:
        mocked_load.return_value = {"CFBundleShortVersionString": "125.1.3"}

        metadata.binary = "/path/to/binary"
        with system as layer, silence(system):
            layer(metadata)

        assert metadata.binary_version == "125.1.3"


@mock.patch("mozperftest.system.setups.mozversion")
def test_versionproducer_mozversion_mobile(mocked_mozversion):
    args = {"flavor": "mobile-browser", "app": "geckoview", "no-binary-setup": True}

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    metadata.binary = "org.mozilla.geckoview_example"
    mocked_mozversion.get_version.return_value = {"application_version": "9000"}
    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary == "org.mozilla.geckoview_example"
    assert metadata.binary_version == "9000"


@mock.patch("mozdevice.ADBDeviceFactory")
@mock.patch("mozperftest.system.setups.mozversion")
def test_versionproducer_fallback_mobile(mocked_mozversion, mocked_devicefactory):
    args = {"flavor": "mobile-browser", "app": "geckoview", "no-binary-setup": True}

    mach_cmd, metadata, env = get_running_env(**args)
    system = env.layers[SYSTEM]

    mocked_mozversion.get_version = lambda x: Exception("")

    mocked_device = mock.MagicMock()
    mocked_device.shell_output.return_value = """
    Packages:
        versionName=127.0a1-nightly
        splits=[base]
        apkSigningVersion=2
    """
    mocked_devicefactory.return_value = mocked_device

    metadata.binary = "org.mozilla.geckoview_example"
    with system as layer, silence(system):
        layer(metadata)

    assert metadata.binary == "org.mozilla.geckoview_example"
    assert metadata.binary_version == "127.0"


if __name__ == "__main__":
    mozunit.main()
