# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import


def get_app_cpu_usage(raptor):
    # If we don't find the browser running, we default to 0 usage
    cpu_usage = 0
    app_name = raptor.config['binary']
    verbose = raptor.device._verbose
    raptor.device._verbose = False

    '''
    There are two ways to get CPU usage information:

    1. By using the 'top' command and parsing details
    2. By using 'dumpsys cpuinfo' and parsing the details

    'top' is our first choice if it is available but the
    parameters we use are only available in Android 8 or
    greater, otherwise we fall back to using dumpsys
    '''

    # Get the android version
    android_version = raptor.device.shell_output(
        "getprop ro.build.version.release"
    ).strip()
    major_android_version = int(android_version.split('.')[0])

    if major_android_version >= 8:
        cpuinfo = raptor.device.shell_output("top -O %CPU -n 1").split("\n")
        raptor.device._verbose = verbose
        for line in cpuinfo:
            # 14781 u0_a83         0 92.8  12.4  64:53.04 org.mozilla.geckoview_example
            data = line.split()
            if data[-1] == app_name:
                cpu_usage = float(data[3])
    else:
        cpuinfo = raptor.device.shell_output("dumpsys cpuinfo").split("\n")

        # Gather the app-specific entries
        appcpuinfo = []
        for line in cpuinfo:
            if app_name not in line:
                continue
            appcpuinfo.append(line)

        # Parse the app-specific entries which look like:
        # 34% 14781/org.mozilla.geckoview_example: 26% user + 7.5% kernel
        for line in appcpuinfo:
            data = line.split()
            cpu_usage = float(data[0].strip('%'))

    return cpu_usage


def generate_android_cpu_profile(raptor, test_name):
    if not raptor.device or not raptor.config['cpu_test']:
        return

    result = get_app_cpu_usage(raptor)

    cpuinfo_data = {
        u'type': u'cpu',
        u'test': test_name,
        u'unit': u'%',
        u'values': {
            u'browser_cpu_usage': result
        }
    }
    raptor.control_server.submit_supporting_data(cpuinfo_data)
