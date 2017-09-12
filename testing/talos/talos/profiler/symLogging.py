# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function

import sys
import threading
import time

gEnableTracing = False


def LogTrace(string):
    global gEnableTracing
    if gEnableTracing:
        threadName = threading.currentThread().getName().ljust(12)
        print(time.asctime() + " " + threadName + " TRACE " + string, file=sys.stdout)


def LogError(string):
    threadName = threading.currentThread().getName().ljust(12)
    print(time.asctime() + " " + threadName + " ERROR " + string, file=sys.stderr)


def LogMessage(string):
    threadName = threading.currentThread().getName().ljust(12)
    print(time.asctime() + " " + threadName + "       " + string, file=sys.stdout)
