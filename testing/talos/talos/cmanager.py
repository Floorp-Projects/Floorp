# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozinfo
import threading


class CounterManager(object):

    counterDict = {}

    def __init__(self):
        self.allCounters = {}
        self.registeredCounters = {}

    def _loadCounters(self):
        """Loads all of the counters defined in the counterDict"""
        for counter in self.counterDict.keys():
            self.allCounters[counter] = self.counterDict[counter]

    def registerCounters(self, counters):
        """Registers a list of counters that will be monitoring.
        Only counters whose names are found in allCounters will be added
        """
        for counter in counters:
            if counter in self.allCounters:
                self.registeredCounters[counter] = \
                    [self.allCounters[counter], []]

    def getCounterValue(self, counterName):
        """Returns the last value of the counter 'counterName'"""


if mozinfo.os == 'linux':
    from talos.cmanager_linux import LinuxCounterManager \
        as DefaultCounterManager
elif mozinfo.os == 'win':
    from talos.cmanager_win32 import WinCounterManager \
        as DefaultCounterManager
else:  # mac
    from talos.cmanager_mac import MacCounterManager \
        as DefaultCounterManager


class CounterManagement(object):
    def __init__(self, process_name, counters, resolution):
        """
        Public interface to manage counters.

        On creation, create a thread to collect counters. Call :meth:`start`
        to start collecting counters with that thread.

        Be sure to call :meth:`stop` to stop the thread.
        """
        assert counters
        self._raw_counters = counters
        self._process_name = process_name
        self._counter_results = \
            dict([(counter, []) for counter in self._raw_counters])

        self._resolution = resolution
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._collect)
        self._process = None

    def _collect(self):
        manager = DefaultCounterManager(self._process_name, self._process,
                                        self._raw_counters)
        while not self._stop.wait(self._resolution):
            # Get the output from all the possible counters
            for count_type in self._raw_counters:
                val = manager.getCounterValue(count_type)
                if val:
                    self._counter_results[count_type].append(val)

    def start(self, process):
        """
        start the counter management thread.

        :param process: a psutil.Process instance representing the browser
                        process.
        """
        self._process = process
        self._thread.start()

    def stop(self):
        self._stop.set()
        self._thread.join()

    def results(self):
        assert not self._thread.is_alive()
        return self._counter_results
