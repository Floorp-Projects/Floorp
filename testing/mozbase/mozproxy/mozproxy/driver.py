# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import argparse
import os
import signal
import sys
import time

import mozinfo
import mozlog.commandline

from . import get_playback
from .utils import LOG, TOOLTOOL_PATHS


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--local", action="store_true",
                        help="run this locally (i.e. not in production)")
    parser.add_argument("--tool", default="mitmproxy",
                        help="the playback tool to use (default: %(default)s)")
    parser.add_argument("--host", default="localhost",
                        help="the host to use for the proxy server")
    parser.add_argument("--binary", required=True,
                        help=("the path to the binary being tested (typically "
                              "firefox)"))
    parser.add_argument("--topsrcdir", required=True,
                        help="the top of the source directory for this project")
    parser.add_argument("--objdir", required=True,
                        help="the object directory for this build")
    parser.add_argument("--app", default="firefox",
                        help="the app being tested (default: %(default)s)")
    parser.add_argument("playback", nargs="*", help="the playback file to use")

    mozlog.commandline.add_logging_group(parser)

    args = parser.parse_args()
    mozlog.commandline.setup_logging("mozproxy", args, {"raw": sys.stdout})

    TOOLTOOL_PATHS.append(os.path.join(args.topsrcdir, "python", "mozbuild",
                                       "mozbuild", "action", "tooltool.py"))

    if hasattr(signal, "SIGBREAK"):
        # Terminating on windows is slightly different than other platforms.
        # On POSIX, we just let Python's default SIGINT handler raise a
        # KeyboardInterrupt. This doesn't work on Windows, so instead we wait
        # for a Ctrl+Break event and raise our own KeyboardInterrupt.
        def handle_sigbreak(sig, frame):
            raise KeyboardInterrupt()

        signal.signal(signal.SIGBREAK, handle_sigbreak)

    try:
        playback = get_playback({
            "run_local": args.local,
            "playback_tool": args.tool,
            "host": args.host,
            "binary": args.binary,
            "obj_path": args.objdir,
            "platform": mozinfo.os,
            "playback_files": args.playback,
            "app": args.app
        })
        playback.start()

        LOG.info("Proxy running on port %d" % playback.port)
        # Wait for a keyboard interrupt from the caller so we know when to
        # terminate. We wait using this method to allow Windows to respond to
        # the Ctrl+Break signal so that we can exit cleanly.
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        LOG.info("Terminating mozproxy")
        playback.stop()
    except Exception as e:
        LOG.error(str(e), exc_info=True)
