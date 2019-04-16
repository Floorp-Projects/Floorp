# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import re


def get_app_memory_usage(raptor):
    app_name = raptor.config['binary']
    total = 0
    re_total_memory = re.compile(r'TOTAL:\s+(\d+)')
    verbose = raptor.device._verbose
    raptor.device._verbose = False
    meminfo = raptor.device.shell_output("dumpsys meminfo %s" % app_name).split('\n')
    raptor.device._verbose = verbose
    for line in meminfo:
        match = re_total_memory.search(line)
        if match:
            total = int(match.group(1))
            break
    return total


def generate_android_memory_profile(raptor, test_name):
    if not raptor.device or not raptor.config['memory_test']:
        return
    foreground = get_app_memory_usage(raptor)
    # put app into background
    verbose = raptor.device._verbose
    raptor.device._verbose = False
    raptor.device.shell_output("am start -a android.intent.action.MAIN "
                               "-c android.intent.category.HOME")
    raptor.device._verbose = verbose
    background = get_app_memory_usage(raptor)
    meminfo_data = {'type': 'memory',
                    'test': test_name,
                    'unit': 'KB',
                    'values': {
                        'foreground': foreground,
                        'background': background
                    }}
    raptor.control_server.submit_supporting_data(meminfo_data)
