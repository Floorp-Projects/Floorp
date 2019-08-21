# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals

import mozunit
import os
import mock

from raptor import cpu
from raptor.raptor import RaptorAndroid


def test_no_device():
    raptor = RaptorAndroid('geckoview', 'org.mozilla.org.mozilla.geckoview_example', cpu_test=True)
    raptor.device = None
    resp = cpu.generate_android_cpu_profile(raptor, 'no_control_server_device')

    assert resp is None


def test_usage_with_invalid_data_returns_zero():
    with mock.patch('mozdevice.adb.ADBDevice') as device:
        with mock.patch('raptor.raptor.RaptorControlServer') as control_server:
            # Create a device that returns invalid data
            device.shell_output.side_effect = ['8.0.0', 'geckoview']
            device._verbose = True

            # Create a control server
            control_server.cpu_test = True
            control_server.device = device
            raptor = RaptorAndroid('geckoview', 'org.mozilla.geckoview_example', cpu_test=True)
            raptor.config['cpu_test'] = True
            raptor.control_server = control_server
            raptor.device = device

            # Verify the call to submit data was made
            cpuinfo_data = {
                'type': 'cpu',
                'test': 'usage_with_invalid_data_returns_zero',
                'unit': '%',
                'values': {
                    'browser_cpu_usage': float(0)
                }
            }
            cpu.generate_android_cpu_profile(
                raptor,
                "usage_with_invalid_data_returns_zero")
            control_server.submit_supporting_data.assert_called_once_with(cpuinfo_data)


def test_usage_with_output():
    with mock.patch('mozdevice.adb.ADBDevice') as device:
        with mock.patch('raptor.raptor.RaptorControlServer') as control_server:
            # Override the shell output with sample CPU usage details
            filepath = os.path.abspath(os.path.dirname(__file__)) + '/files/'
            with open(filepath + 'top-info.txt', 'r') as f:
                test_data = f.read()
            device.shell_output.side_effect = ['8.0.0', test_data]
            device._verbose = True

            # Create a control server
            control_server.cpu_test = True
            control_server.test_name = 'cpuunittest'
            control_server.device = device
            control_server.app_name = 'org.mozilla.geckoview_example'
            raptor = RaptorAndroid('geckoview', 'org.mozilla.geckoview_example', cpu_test=True)
            raptor.device = device
            raptor.config['cpu_test'] = True
            raptor.control_server = control_server

            # Verify the response contains our expected CPU % of 93.7
            cpuinfo_data = {
                u'type': u'cpu',
                u'test': u'usage_with_integer_cpu_info_output',
                u'unit': u'%',
                u'values': {
                    u'browser_cpu_usage': float(93.7)
                }
            }
            cpu.generate_android_cpu_profile(
                raptor,
                "usage_with_integer_cpu_info_output")
            control_server.submit_supporting_data.assert_called_once_with(cpuinfo_data)


def test_usage_with_fallback():
    with mock.patch('mozdevice.adb.ADBDevice') as device:
        with mock.patch('raptor.raptor.RaptorControlServer') as control_server:
            device._verbose = True

            # Return what our shell call to dumpsys would give us
            shell_output = ' 34% 14781/org.mozilla.geckoview_example: 26% user + 7.5% kernel'

            # We set the version to be less than Android 8
            device.shell_output.side_effect = ['7.0.0', shell_output]

            # Create a control server
            control_server.cpu_test = True
            control_server.test_name = 'cpuunittest'
            control_server.device = device
            control_server.app_name = 'org.mozilla.geckoview_example'
            raptor = RaptorAndroid('geckoview', 'org.mozilla.geckoview_example', cpu_test=True)
            raptor.device = device
            raptor.config['cpu_test'] = True
            raptor.control_server = control_server

            # Verify the response contains our expected CPU % of 34
            cpuinfo_data = {
                u'type': u'cpu',
                u'test': u'usage_with_fallback',
                u'unit': u'%',
                u'values': {
                    u'browser_cpu_usage': float(34)
                }
            }
            cpu.generate_android_cpu_profile(
                raptor,
                "usage_with_fallback")
            control_server.submit_supporting_data.assert_called_once_with(cpuinfo_data)


if __name__ == '__main__':
    mozunit.main()
