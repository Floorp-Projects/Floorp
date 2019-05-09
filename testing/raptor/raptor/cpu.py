# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import


def get_app_cpu_usage(raptor):
    cpu_usage = 0
    app_name = raptor.config['binary']
    verbose = raptor.device._verbose
    raptor.device._verbose = False
    cpuinfo = raptor.device.shell_output("top -O %CPU -n 1").split("\n")
    raptor.device._verbose = verbose
    '''
    When parsing the output of the shell command, you will
    get a line that looks like this:

    17504 u0_a83       93.7 93.7  14.2   0:12.12 org.mozilla.geckoview_example

    When you split on whitespace you end up with the
    name of the process at index 6 and the
    amount of CPU being used at index 3
    (Remember that indexes start at 0 because COMPUTERS)
    '''
    for line in cpuinfo:
        data = line.split()
        if len(data) == 7 and data[6] == app_name:
            cpu_usage = data[3]
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
