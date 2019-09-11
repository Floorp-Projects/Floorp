# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import time
import threading

from logger.logger import RaptorLogger

LOG = RaptorLogger(component='raptor-cpu')


class AndroidCPUProfiler(object):
    """AndroidCPUProfiler is used to measure CPU usage over time for an
    app in testing. Polls usage at a defined interval and then submits this
    as a supporting perfherder data measurement.

    ```
    # Initialize the profiler and start polling for CPU usage of the app
    cpu_profiler = AndroidCPUProfiler(raptor, poll_interval=1)
    cpu_profiler.start_polling()

    # Or call the following to perform the commands above
    cpu_profiler = start_android_cpu_profiler(raptor)

    # Run test...

    # Stop measuring and generate perfherder data
    cpu_profiler.generate_android_cpu_profile("raptor-tp6m-1")
    ```
    """

    def __init__(self, raptor, poll_interval=10):
        """Initialize the Android CPU Profiler.

        :param RaptorAndroid raptor: raptor object which contains android device.
        :param float poll_interval: interval, in seconds, at which we should poll
            for CPU usage. Defaults to 10 seconds.
        """
        self.raptor = raptor
        self.polls = []
        self.poll_interval = poll_interval
        self.pause_polling = False

        # Get the android version
        android_version = self.raptor.device.shell_output(
            "getprop ro.build.version.release"
        ).strip()
        self.android_version = int(android_version.split('.')[0])

        # Prepare the polling thread (set as daemon for clean exiting)
        self.thread = threading.Thread(target=self.poll_cpu, args=(poll_interval,))
        self.thread.daemon = True

    def start_polling(self):
        """Start the thread responsible for polling CPU usage.
        """
        self.thread.start()

    def stop_polling(self):
        """Pauses CPU usage polling.
        """
        self.pause_polling = True

    def poll_cpu(self, poll_interval):
        """Polls CPU usage at an interval of `poll_interval`.

        :param float poll_interval: interval at which we poll for CPU usage.
        """
        while True:
            time.sleep(self.poll_interval)
            if self.pause_polling:
                continue
            self.polls.append(self.get_app_cpu_usage())

    def get_app_cpu_usage(self):
        """Gather a point on an android app's CPU usage.

        :return float: CPU usage found for the app at this point in time.
        """
        cpu_usage = 0
        app_name = self.raptor.config['binary']
        verbose = self.raptor.device._verbose
        self.raptor.device._verbose = False

        if self.android_version >= 8:
            # On android 8 we can use the -O option to order the entries
            # in top by %CPU.
            cpuinfo = self.raptor.device.shell_output("top -O %CPU -n 1").split("\n")

            for line in cpuinfo:
                # A line looks like:
                # 14781 u0_a83         0 92.8  12.4  64:53.04 org.mozilla.geckoview_example
                data = line.split()
                if data[-1] == app_name:
                    cpu_usage = float(data[3])
        else:
            # On android 7, -O is not required since top already orders them by %CPU.
            # top also has different columns on this android version.
            cpuinfo = self.raptor.device.shell_output("top -n 1").split("\n")

            # Parse the app-specific entries which look like:
            # 21165 u0_a196  10 -10  14% S    67 1442392K 163356K  fg org.mozilla.geckoview_example

            # Gather the app-specific entries
            appcpuinfo = []
            for line in cpuinfo:
                if not line.strip().endswith(app_name):
                    continue
                appcpuinfo.append(line)

            # Get CPU usage
            for line in appcpuinfo:
                data = line.split()
                cpu_usage = float(data[4].strip('%'))

        self.raptor.device._verbose = verbose
        return cpu_usage

    def generate_android_cpu_profile(self, test_name):
        """Use the CPU usage data which was sampled to produce a supporting
        perfherder data measurement and submit it to the raptor control
        server. Returns the minimum, maximum, and average CPU usage.

        :param str test_name: name of the test that was run.
        """
        self.stop_polling()
        if len(self.polls) == 0:
            LOG.info("No CPU usage data found, submitting as 0% usage.")
            self.polls.append(0)

        avg_cpuinfo_data = {
            u'type': u'cpu',
            u'test': test_name + '-avg',
            u'unit': u'%',
            u'values': {
                u'avg': sum(self.polls)/len(self.polls)
            }
        }
        self.raptor.control_server.submit_supporting_data(avg_cpuinfo_data)

        min_cpuinfo_data = {
            u'type': u'cpu',
            u'test': test_name + '-min',
            u'unit': u'%',
            u'values': {
                u'min': min(self.polls)
            }
        }
        self.raptor.control_server.submit_supporting_data(min_cpuinfo_data)

        max_cpuinfo_data = {
            u'type': u'cpu',
            u'test': test_name + '-max',
            u'unit': u'%',
            u'values': {
                u'max': max(self.polls)
            }
        }
        self.raptor.control_server.submit_supporting_data(max_cpuinfo_data)


def start_android_cpu_profiler(raptor, **kwargs):
    """Start the android CPU profiler and return the profiler
    object so measurements can be obtained.

    :return AndroidCPUProfiler: the profiler performing the CPU measurements.
    """
    if not raptor.device:
        LOG.error("No ADB device found in raptor, not creating AndroidCPUProfiler.")
        return

    cpu_profiler = AndroidCPUProfiler(raptor, **kwargs)
    cpu_profiler.start_polling()

    return cpu_profiler
