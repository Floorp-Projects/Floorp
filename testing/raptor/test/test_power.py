# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals

import sys

import os
from unittest import mock
import tempfile

import mozunit

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))

raptor_dir = os.path.join(os.path.dirname(here), "raptor")
sys.path.insert(0, raptor_dir)

import power
from webextension import WebExtensionAndroid


def test_android7_power():
    if not os.getenv("MOZ_UPLOAD_DIR"):
        os.environ["MOZ_UPLOAD_DIR"] = tempfile.mkdtemp()

    with mock.patch("mozdevice.adb.ADBDevice") as device:
        with mock.patch("control_server.RaptorControlServer") as control_server:
            # Override the shell output with sample CPU usage details
            filepath = os.path.abspath(os.path.dirname(__file__)) + "/files/"
            f = open(filepath + "batterystats-android-7.txt", "r")
            batterystats_return_value = f.read()

            # Multiple shell output calls are performed
            # and only those with non-None output are required
            device.shell_output.return_value = None
            device.shell_output.side_effect = [
                None,
                None,
                "Test value",
                "Test value",
                batterystats_return_value,
                "7.0.0",
            ]

            device._verbose = True
            device.version = 7

            # Create a control server
            control_server.power_test = True
            control_server.test_name = "gve-pytest"
            control_server.device = device
            control_server.app_name = "org.mozilla.geckoview_example"
            web_extension = WebExtensionAndroid(
                "geckoview", "org.mozilla.geckoview_example", power_test=True
            )
            web_extension.device = device
            web_extension.config["power_test"] = True
            web_extension.control_server = control_server
            web_extension.power_test_time = 20  # minutes
            web_extension.os_baseline_data = {
                "type": "power",
                "test": "gve-pytest",
                "unit": "mAh",
                "values": {"cpu": float(5), "wifi": float(5), "screen": float(5)},
            }

            # Verify the response contains our expected calculations
            # (no proportional measure on android 7)
            power_data = {
                "type": "power",
                "test": "gve-pytest",
                "unit": "mAh",
                "values": {
                    "cpu": float(14.5),
                    "wifi": float(0.132),
                    "screen": float(70.7),
                },
            }

            pc_data = {
                "type": "power",
                "test": "gve-pytest-%change",
                "unit": "%",
                "values": {
                    "cpu": float(14.5),
                    "wifi": float(0.132000000000005),
                    "screen": float(70.70000000000002),
                },
            }

            power.finish_android_power_test(web_extension, "gve-pytest")

            control_server.submit_supporting_data.assert_has_calls(
                [
                    mock.call(power_data),
                    mock.call(pc_data),
                    mock.call(web_extension.os_baseline_data),
                ]
            )


def test_android8_power():
    if not os.getenv("MOZ_UPLOAD_DIR"):
        os.environ["MOZ_UPLOAD_DIR"] = tempfile.mkdtemp()

    with mock.patch("mozdevice.adb.ADBDevice") as device:
        with mock.patch("control_server.RaptorControlServer") as control_server:
            # Override the shell output with sample CPU usage details
            filepath = os.path.abspath(os.path.dirname(__file__)) + "/files/"
            f = open(filepath + "batterystats-android-8.txt", "r")
            batterystats_return_value = f.read()
            print(type(batterystats_return_value))

            # Multiple shell output calls are performed
            # and only those with non-None output are required
            device.shell_output.return_value = None
            device.shell_output.side_effect = [
                None,
                None,
                "Test value",
                "Test value",
                batterystats_return_value,
                "8.0.0",
            ]

            device._verbose = True
            device.version = 8

            # Create a control server
            control_server.power_test = True
            control_server.test_name = "gve-pytest"
            control_server.device = device
            control_server.app_name = "org.mozilla.geckoview_example"
            web_extension = WebExtensionAndroid(
                "geckoview", "org.mozilla.geckoview_example", power_test=True
            )
            web_extension.device = device
            web_extension.config["power_test"] = True
            web_extension.control_server = control_server
            web_extension.power_test_time = 20  # minutes
            web_extension.os_baseline_data = {
                "type": "power",
                "test": "gve-pytest",
                "unit": "mAh",
                "values": {
                    "cpu": float(5),
                    "wifi": float(5),
                    "screen": float(5),
                    "proportional": float(5),
                },
            }

            # Verify the response contains our expected calculations
            power_data = {
                "type": "power",
                "test": "gve-pytest",
                "unit": "mAh",
                "values": {
                    "cpu": float(4.7),
                    "wifi": float(0.000556),
                    "screen": float(51.5),
                    "proportional": float(11.2),
                },
            }

            pc_data = {
                "type": "power",
                "test": "gve-pytest-%change",
                "unit": "%",
                "values": {
                    "cpu": float(4.700000000000017),
                    "wifi": float(0.0005559999999888987),
                    "screen": float(51.5),
                    "proportional": float(11.199999999999989),
                },
            }

            power.finish_android_power_test(web_extension, "gve-pytest")

            control_server.submit_supporting_data.assert_has_calls(
                [
                    mock.call(power_data),
                    mock.call(pc_data),
                    mock.call(web_extension.os_baseline_data),
                ]
            )


def test_androidos_baseline_power():
    if not os.getenv("MOZ_UPLOAD_DIR"):
        os.environ["MOZ_UPLOAD_DIR"] = tempfile.mkdtemp()

    with mock.patch("mozdevice.adb.ADBDevice") as device:
        with mock.patch("control_server.RaptorControlServer") as control_server:
            # Override the shell output with sample CPU usage details
            filepath = os.path.abspath(os.path.dirname(__file__)) + "/files/"
            f = open(filepath + "batterystats-android-8.txt", "r")
            batterystats_return_value = f.read()

            # Multiple shell output calls are performed
            # and only those with non-None output are required
            device.shell_output.return_value = None
            device.shell_output.side_effect = [
                None,
                None,
                "Test value",
                "Test value",
                batterystats_return_value,
                "8.0.0",
            ]

            device._verbose = True
            device.version = 8

            # Create a control server
            control_server.power_test = True
            control_server.test_name = "gve-pytest"
            control_server.device = device
            control_server.app_name = "org.mozilla.geckoview_example"
            web_extension = WebExtensionAndroid(
                "geckoview", "org.mozilla.geckoview_example", power_test=True
            )
            web_extension.device = device
            web_extension.config["power_test"] = True
            web_extension.control_server = control_server

            # Expected OS baseline calculation result
            os_baseline_data = {
                "type": "power",
                "test": "gve-pytest",
                "unit": "mAh",
                "values": {
                    "cpu": float(10.786654),
                    "wifi": float(2.26132),
                    "screen": float(51.66),
                    "proportional": float(11.294805199999999),
                },
            }

            # Verify the response contains our expected calculations
            power.finish_android_power_test(
                web_extension, "gve-pytest", os_baseline=True
            )

            assert web_extension.os_baseline_data == os_baseline_data


if __name__ == "__main__":
    mozunit.main()
