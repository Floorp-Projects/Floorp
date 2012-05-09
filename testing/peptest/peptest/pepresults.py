# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# singleton class
class Results(object):
    """
    Singleton class for keeping track of which
    tests are currently running and which
    tests have failed
    """
    _instance = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = super(Results, cls).__new__(
                                cls, *args, **kwargs)
        return cls._instance

    def __init__(self):
        self.currentTest = None
        self.currentAction = None
        self.fails = {}

    def has_fails(self):
        for k, v in self.fails.iteritems():
            if len(v) > 0:
                return True
        return False

    def get_metric(self, test):
        return sum([x*x / 1000.0 for x in self.fails[test]])
