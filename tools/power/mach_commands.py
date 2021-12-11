# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, absolute_import

from distutils.version import StrictVersion

from mach.decorators import (
    Command,
    CommandArgument,
)


def is_osx_10_10_or_greater(cls):
    import platform

    release = platform.mac_ver()[0]
    return release and StrictVersion(release) >= StrictVersion("10.10")


# Get system power consumption and related measurements.
@Command(
    "power",
    category="misc",
    conditions=[is_osx_10_10_or_greater],
    description="Get system power consumption and related measurements for "
    "all running browsers. Available only on Mac OS X 10.10 and above. "
    "Requires root access.",
)
@CommandArgument(
    "-i",
    "--interval",
    type=int,
    default=30000,
    help="The sample period, measured in milliseconds. Defaults to 30000.",
)
def power(command_context, interval):
    """
    Get system power consumption and related measurements.
    """
    import os
    import re
    import subprocess

    rapl = os.path.join(command_context.topobjdir, "dist", "bin", "rapl")

    interval = str(interval)

    # Run a trivial command with |sudo| to gain temporary root privileges
    # before |rapl| and |powermetrics| are called. This ensures that |rapl|
    # doesn't start measuring while |powermetrics| is waiting for the root
    # password to be entered.
    try:
        subprocess.check_call(["sudo", "true"])
    except Exception:
        print("\nsudo failed; aborting")
        return 1

    # This runs rapl in the background because nothing in this script
    # depends on the output. This is good because we want |rapl| and
    # |powermetrics| to run at the same time.
    subprocess.Popen([rapl, "-n", "1", "-i", interval])

    lines = subprocess.check_output(
        [
            "sudo",
            "powermetrics",
            "--samplers",
            "tasks",
            "--show-process-coalition",
            "--show-process-gpu",
            "-n",
            "1",
            "-i",
            interval,
        ],
        universal_newlines=True,
    )

    # When run with --show-process-coalition, |powermetrics| groups outputs
    # into process coalitions, each of which has a leader.
    #
    # For example, when Firefox runs from the dock, its coalition looks
    # like this:
    #
    #   org.mozilla.firefox
    #     firefox
    #     plugin-container
    #
    # When Safari runs from the dock:
    #
    #   com.apple.Safari
    #     Safari
    #     com.apple.WebKit.Networking
    #     com.apple.WebKit.WebContent
    #     com.apple.WebKit.WebContent
    #
    # When Chrome runs from the dock:
    #
    #   com.google.Chrome
    #     Google Chrome
    #     Google Chrome Helper
    #     Google Chrome Helper
    #
    # In these cases, we want to print the whole coalition.
    #
    # Also, when you run any of them from the command line, things are the
    # same except that the leader is com.apple.Terminal and there may be
    # non-browser processes in the coalition, e.g.:
    #
    #  com.apple.Terminal
    #    firefox
    #    plugin-container
    #    <and possibly other, non-browser processes>
    #
    # Also, the WindowServer and kernel coalitions and processes are often
    # relevant.
    #
    # We want to print all these but omit uninteresting coalitions. We
    # could do this by properly parsing powermetrics output, but it's
    # simpler and more robust to just grep for a handful of identifying
    # strings.

    print()  # blank line between |rapl| output and |powermetrics| output

    for line in lines.splitlines():
        # Search for the following things.
        #
        # - '^Name' is for the columns headings line.
        #
        # - 'firefox' and 'plugin-container' are for Firefox
        #
        # - 'Safari\b' and 'WebKit' are for Safari. The '\b' excludes
        #   SafariCloudHistoryPush, which is a process that always
        #   runs, even when Safari isn't open.
        #
        # - 'Chrome' is for Chrome.
        #
        # - 'Terminal' is for the terminal. If no browser is running from
        #   within the terminal, it will show up unnecessarily. This is a
        #   minor disadvantage of this very simple parsing strategy.
        #
        # - 'WindowServer' is for the WindowServer.
        #
        # - 'kernel' is for the kernel.
        #
        if re.search(
            r"(^Name|firefox|plugin-container|Safari\b|WebKit|Chrome|Terminal|WindowServer|kernel)",  # NOQA: E501
            line,
        ):
            print(line)

    return 0
