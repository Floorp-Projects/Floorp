# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import logging
from subprocess import check_call, CalledProcessError
import sys

from redo import retrying

log = logging.getLogger(__name__)


def main():
    from argparse import ArgumentParser

    parser = ArgumentParser()
    parser.add_argument(
        "-a", "--attempts", type=int, default=5,
        help="How many times to retry.")
    parser.add_argument(
        "-s", "--sleeptime", type=int, default=60,
        help="How long to sleep between attempts. Sleeptime doubles after each attempt.")
    parser.add_argument(
        "-m", "--max-sleeptime", type=int, default=5*60,
        help="Maximum length of time to sleep between attempts (limits backoff length).")
    parser.add_argument("-v", "--verbose", action="store_true", default=False)
    parser.add_argument("cmd", nargs="+", help="Command to run. Eg: wget http://blah")

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.INFO)
        logging.getLogger("retry").setLevel(logging.INFO)
    else:
        logging.basicConfig(level=logging.ERROR)
        logging.getLogger("retry").setLevel(logging.ERROR)

    try:
        with retrying(check_call, attempts=args.attempts, sleeptime=args.sleeptime,
                      max_sleeptime=args.max_sleeptime,
                      retry_exceptions=(CalledProcessError,)) as r_check_call:
            r_check_call(args.cmd)
    except KeyboardInterrupt:
        sys.exit(-1)
    except Exception as e:
        log.error("Unable to run command after %d attempts" % args.attempts, exc_info=True)
        rc = getattr(e, "returncode", -2)
        sys.exit(rc)

if __name__ == "__main__":
    main()
