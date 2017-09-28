#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

"""A wrapper around pywebsocket's standalone.py which causes it to ignore
SIGINT.

"""

import signal
import sys

if __name__ == '__main__':
    sys.path = ['pywebsocket'] + sys.path
    import standalone

    # If we received --interactive as the first argument, ignore SIGINT so
    # pywebsocket doesn't die on a ctrl+c meant for the debugger.  Otherwise,
    # die immediately on SIGINT so we don't print a messy backtrace.
    if len(sys.argv) >= 2 and sys.argv[1] == '--interactive':
        del sys.argv[1]
        signal.signal(signal.SIGINT, signal.SIG_IGN)
    else:
        signal.signal(signal.SIGINT, lambda signum, frame: sys.exit(1))

    standalone._main()
