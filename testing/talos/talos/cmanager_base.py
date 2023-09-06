# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


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
                self.registeredCounters[counter] = [self.allCounters[counter], []]

    def getCounterValue(self, counterName):
        """Returns the last value of the counter 'counterName'"""
