# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Maximum number of tabs to open
MAX_TABS = 30

# Default amount of seconds to wait in between opening tabs
PER_TAB_PAUSE = 10

# Default amount of seconds to wait for things to be settled down
SETTLE_WAIT_TIME = 30

# Amount of times to run through the test suite
ITERATIONS = 5

__all__ = [
    "MAX_TABS",
    "PER_TAB_PAUSE",
    "SETTLE_WAIT_TIME",
    "ITERATIONS",
    "webservers",
    "process_perf_data",
]
