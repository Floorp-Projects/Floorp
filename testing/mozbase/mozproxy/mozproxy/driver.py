# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import os
import signal
import sys

import mozinfo
import mozlog.commandline

from . import get_playback
from .utils import LOG, TOOLTOOL_PATHS

EXIT_SUCCESS = 0
EXIT_EARLY_TERMINATE = 3
EXIT_EXCEPTION = 4


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--mode",
        default="playback",
        choices=["record", "playback"],
        help="Proxy server mode. Use `playback` to replay from the provided file(s). "
        "Use `record` to generate a new recording at the path specified by `--file`. "
        "playback - replay from provided file. "
        "record - generate a new recording at the specified path.",
    )
    parser.add_argument(
        "file",
        nargs="+",
        help="The playback files to replay, or the file that a recording will be saved to. "
        "For playback, it can be any combination of the following: zip file, manifest file, "
        "or a URL to zip/manifest file. "
        "For recording, it's a zip fle.",
    )
    parser.add_argument(
        "--host", default="localhost", help="The host to use for the proxy server."
    )
    parser.add_argument(
        "--tool",
        default="mitmproxy",
        help="The playback tool to use (default: %(default)s).",
    )
    parser.add_argument(
        "--tool-version",
        default="8.1.1",
        help="The playback tool version to use (default: %(default)s)",
    )
    parser.add_argument(
        "--app", default="firefox", help="The app being tested (default: %(default)s)."
    )
    parser.add_argument(
        "--binary",
        required=True,
        help=("The path to the binary being tested (typically firefox)."),
    )
    parser.add_argument(
        "--topsrcdir",
        required=True,
        help="The top of the source directory for this project.",
    )
    parser.add_argument(
        "--objdir", required=True, help="The object directory for this build."
    )
    parser.add_argument(
        "--profiledir", default=None, help="Path to the profile directory."
    )
    parser.add_argument(
        "--local",
        action="store_true",
        help="Run this locally (i.e. not in production).",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        default=False,
        help="Run this locally (i.e. not in production).",
    )
    parser.add_argument(
        "--deterministic",
        action="store_true",
        default=False,
        help="Enable or disable inject_deterministic script when recording.",
    )

    mozlog.commandline.add_logging_group(parser)

    args = parser.parse_args()
    mozlog.commandline.setup_logging("mozproxy", args, {"raw": sys.stdout})

    TOOLTOOL_PATHS.append(
        os.path.join(
            args.topsrcdir, "python", "mozbuild", "mozbuild", "action", "tooltool.py"
        )
    )

    if hasattr(signal, "SIGBREAK"):
        # Terminating on windows is slightly different than other platforms.
        # On POSIX, we just let Python's default SIGINT handler raise a
        # KeyboardInterrupt. This doesn't work on Windows, so instead we wait
        # for a Ctrl+Break event and raise our own KeyboardInterrupt.
        def handle_sigbreak(sig, frame):
            raise KeyboardInterrupt()

        signal.signal(signal.SIGBREAK, handle_sigbreak)

    try:
        if args.mode == "playback":
            if len(args.file) == 0:
                raise Exception("Please provide at least one recording file!")

            # Playback mode
            proxy_service = get_playback(
                {
                    "run_local": args.local,
                    "host": args.host,
                    "binary": args.binary,
                    "obj_path": args.objdir,
                    "platform": mozinfo.os,
                    "playback_tool": args.tool,
                    "playback_version": args.tool_version,
                    "playback_files": args.file,
                    "app": args.app,
                    "local_profile_dir": args.profiledir,
                    "verbose": args.verbose,
                }
            )
        if args.mode == "record":
            # Record mode
            if len(args.file) > 1:
                raise Exception("Please provide only one recording file!")

            LOG.info("Recording will be saved to: %s" % args.file)
            proxy_service = get_playback(
                {
                    "run_local": args.local,
                    "host": args.host,
                    "binary": args.binary,
                    "obj_path": args.objdir,
                    "platform": mozinfo.os,
                    "playback_tool": args.tool,
                    "playback_version": args.tool_version,
                    "record": True,
                    "recording_file": args.file[0],
                    "app": args.app,
                    "local_profile_dir": args.profiledir,
                    "verbose": args.verbose,
                    "inject_deterministic": args.deterministic,
                }
            )
        LOG.info("Proxy settings %s" % proxy_service)
        proxy_service.start()
        LOG.info("Proxy running on port %d" % proxy_service.port)
        # Wait for a keyboard interrupt from the caller so we know when to
        # terminate.
        proxy_service.wait()
        return EXIT_EARLY_TERMINATE
    except KeyboardInterrupt:
        LOG.info("Terminating mozproxy")
        proxy_service.stop()
        return EXIT_SUCCESS
    except Exception as e:
        LOG.error(str(e), exc_info=True)
        return EXIT_EXCEPTION


if __name__ == "__main__":
    main()
