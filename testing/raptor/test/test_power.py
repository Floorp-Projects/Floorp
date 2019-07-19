# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals

import mozunit
import os
import mock
import tempfile

from raptor import power
from raptor.raptor import RaptorAndroid


def test_android7_power():
    if not os.getenv('MOZ_UPLOAD_DIR'):
        os.environ['MOZ_UPLOAD_DIR'] = tempfile.mkdtemp()

    with mock.patch('mozdevice.adb.ADBDevice') as device:
        with mock.patch('raptor.raptor.RaptorControlServer') as control_server:
            # Override the shell output with sample CPU usage details
            filepath = os.path.abspath(os.path.dirname(__file__)) + '/files/'
            f = open(filepath + 'batterystats-android-7.txt', 'r')
            batterystats_return_value = f.read()

            # Multiple shell output calls are performed
            # and only those with non-None output are required
            device.shell_output.return_value = None
            device.shell_output.side_effect = [
                None,
                None,
                'Test value',
                'Test value',
                batterystats_return_value,
                '7.0.0',
            ]

            device._verbose = True
            device.version = 7

            # Create a control server
            control_server.power_test = True
            control_server.test_name = 'gve-pytest'
            control_server.device = device
            control_server.app_name = 'org.mozilla.geckoview_example'
            raptor = RaptorAndroid('geckoview', 'org.mozilla.geckoview_example', power_test=True)
            raptor.device = device
            raptor.config['power_test'] = True
            raptor.control_server = control_server
            raptor.power_test_time = 20  # minutes
            raptor.os_baseline_data = {
                u'type': u'power',
                u'test': u'gve-pytest',
                u'unit': u'mAh',
                u'values': {
                    u'cpu': float(5),
                    u'wifi': float(5),
                    u'screen': float(5)
                }
            }

            # Verify the response contains our expected calculations
            # (no proportional measure on android 7)
            power_data = {
                u'type': u'power',
                u'test': u'gve-pytest',
                u'unit': u'mAh',
                u'values': {
                    u'cpu': float(14.5),
                    u'wifi': float(0.132),
                    u'screen': float(70.7)
                }
            }

            pc_data = {
                u'type': u'power',
                u'test': u'gve-pytest-%change',
                u'unit': u'%',
                u'values': {
                    u'cpu': float(14.5),
                    u'wifi': float(0.132000000000005),
                    u'screen': float(70.70000000000002)
                }
            }

            power.finish_android_power_test(
                raptor,
                'gve-pytest'
            )

            control_server.submit_supporting_data.assert_has_calls([
                    mock.call(power_data),
                    mock.call(pc_data),
                    mock.call(raptor.os_baseline_data)
            ])


def test_android8_power():
    if not os.getenv('MOZ_UPLOAD_DIR'):
        os.environ['MOZ_UPLOAD_DIR'] = tempfile.mkdtemp()

    with mock.patch('mozdevice.adb.ADBDevice') as device:
        with mock.patch('raptor.raptor.RaptorControlServer') as control_server:
            # Override the shell output with sample CPU usage details
            filepath = os.path.abspath(os.path.dirname(__file__)) + '/files/'
            f = open(filepath + 'batterystats-android-8.txt', 'r')
            batterystats_return_value = f.read()
            print(type(batterystats_return_value))

            # Multiple shell output calls are performed
            # and only those with non-None output are required
            device.shell_output.return_value = None
            device.shell_output.side_effect = [
                None,
                None,
                'Test value',
                'Test value',
                batterystats_return_value,
                '8.0.0',
            ]

            device._verbose = True
            device.version = 8

            # Create a control server
            control_server.power_test = True
            control_server.test_name = 'gve-pytest'
            control_server.device = device
            control_server.app_name = 'org.mozilla.geckoview_example'
            raptor = RaptorAndroid('geckoview', 'org.mozilla.geckoview_example', power_test=True)
            raptor.device = device
            raptor.config['power_test'] = True
            raptor.control_server = control_server
            raptor.power_test_time = 20  # minutes
            raptor.os_baseline_data = {
                u'type': u'power',
                u'test': u'gve-pytest',
                u'unit': u'mAh',
                u'values': {
                    u'cpu': float(5),
                    u'wifi': float(5),
                    u'screen': float(5),
                    u'proportional': float(5)
                }
            }

            # Verify the response contains our expected calculations
            power_data = {
                u'type': u'power',
                u'test': u'gve-pytest',
                u'unit': u'mAh',
                u'values': {
                    u'cpu': float(4.7),
                    u'wifi': float(0.000556),
                    u'screen': float(51.5),
                    u'proportional': float(11.2)
                }
            }

            pc_data = {
                u'type': u'power',
                u'test': u'gve-pytest-%change',
                u'unit': u'%',
                u'values': {
                    u'cpu': float(4.700000000000017),
                    u'wifi': float(0.0005559999999888987),
                    u'screen': float(51.5),
                    u'proportional': float(11.199999999999989)
                }
            }

            power.finish_android_power_test(
                raptor,
                'gve-pytest'
            )

            control_server.submit_supporting_data.assert_has_calls([
                    mock.call(power_data),
                    mock.call(pc_data),
                    mock.call(raptor.os_baseline_data)
            ])


def test_androidos_baseline_power():
    if not os.getenv('MOZ_UPLOAD_DIR'):
        os.environ['MOZ_UPLOAD_DIR'] = tempfile.mkdtemp()

    with mock.patch('mozdevice.adb.ADBDevice') as device:
        with mock.patch('raptor.raptor.RaptorControlServer') as control_server:
            # Override the shell output with sample CPU usage details
            filepath = os.path.abspath(os.path.dirname(__file__)) + '/files/'
            f = open(filepath + 'batterystats-android-8.txt', 'r')
            batterystats_return_value = f.read()

            # Multiple shell output calls are performed
            # and only those with non-None output are required
            device.shell_output.return_value = None
            device.shell_output.side_effect = [
                None,
                None,
                'Test value',
                'Test value',
                batterystats_return_value,
                '8.0.0',
            ]

            device._verbose = True
            device.version = 8

            # Create a control server
            control_server.power_test = True
            control_server.test_name = 'gve-pytest'
            control_server.device = device
            control_server.app_name = 'org.mozilla.geckoview_example'
            raptor = RaptorAndroid('geckoview', 'org.mozilla.geckoview_example', power_test=True)
            raptor.device = device
            raptor.config['power_test'] = True
            raptor.control_server = control_server

            # Expected OS baseline calculation result
            os_baseline_data = {
                u'type': u'power',
                u'test': u'gve-pytest',
                u'unit': u'mAh',
                u'values': {
                    u'cpu': float(10.786654),
                    u'wifi': float(2.26132),
                    u'screen': float(51.66),
                    u'proportional': float(11.294805199999999)
                }
            }

            # Verify the response contains our expected calculations
            power.finish_android_power_test(
                raptor,
                'gve-pytest',
                os_baseline=True
            )

            assert raptor.os_baseline_data == os_baseline_data


if __name__ == '__main__':
    mozunit.main()
