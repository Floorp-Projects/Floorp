# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""Mozlog aims to standardize log formatting within Mozilla.
It simply wraps Python's logging_ module and adds a few convenience methods for logging test results and events.
"""

from logger import *
from loglistener import LogMessageServer
