#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

"""
A script to set up startup profiling with the Firefox Profiler. See
https://profiler.firefox.com/docs/#/./guide-remote-profiling?id=startup-profiling
for more information.
"""

import argparse
import os
import tempfile
from subprocess import run

PATH_PREFIX = "/data/local/tmp"

PROD_FENIX = "fenix"
PROD_GVE = "geckoview_example"
PRODUCTS = [PROD_FENIX, PROD_GVE]

GV_CONFIG = b"""env:
  MOZ_PROFILER_STARTUP: 1
  MOZ_PROFILER_STARTUP_INTERVAL: 5
  MOZ_PROFILER_STARTUP_FEATURES: js,stackwalk,leaf,screenshots,ipcmessages,java,cpu
  MOZ_PROFILER_STARTUP_FILTERS: GeckoMain,Compositor,Renderer,IPDL Background
"""


def parse_args():
    p = argparse.ArgumentParser(
        description=(
            "Easily enable start up profiling using the Firefox Profiler. Finish capturing the profile in "
            "about:debugging on desktop. See "
            "https://profiler.firefox.com/docs/#/./guide-remote-profiling?id=startup-profiling for "
            "details."
        )
    )
    p.add_argument(
        "command",
        choices=["activate", "deactivate"],
        help=(
            "whether to activate or deactive start up "
            "profiling for the given release channel"
        ),
    )
    p.add_argument(
        "release_channel",
        choices=["nightly", "beta", "release", "debug"],
        help=(
            "the release channel to "
            "change the startup profiling state of the command on"
        ),
    )

    p.add_argument(
        "-p",
        "--product",
        choices=PRODUCTS,
        default=PROD_FENIX,
        help="which product to work on",
    )
    return p.parse_args()


def push(id, filename):
    config = tempfile.NamedTemporaryFile(delete=False)
    try:
        # I think the file needs to be closed to save its contents for adb push to
        # work correctly so we close it here and later delete it manually.
        with config.file as f:
            f.write(GV_CONFIG)

        print("Pushing {} to device.".format(filename))
        run(["adb", "push", config.name, os.path.join(PATH_PREFIX, filename)])
        run(["adb", "shell", "am", "set-debug-app", "--persistent", id])
        print(
            "\nStartup profiling enabled on all future start ups, possibly even after reinstall."
        )
        print("Call script with `deactivate` to disable it.")
        print(
            "DISABLE 'Remote debugging via USB' IN THE APP SETTINGS BEFORE STARTING THE APP & RE-ENABLE TO CAPTURE THE PROFILE.",
            "This avoids the additional overhead added when 'Remote debugging via USB' is enabled during start up.",
            sep=os.linesep,
        )
    finally:
        os.remove(config.name)


def remove(filename):
    print("Removing {} from device.".format(filename))
    run(["adb", "shell", "rm", PATH_PREFIX + "/" + filename])
    run(["adb", "shell", "am", "clear-debug-app"])


def convert_channel_to_id(product, channel):
    if product == PROD_FENIX:
        mapping = {
            "release": "org.mozilla.firefox",
            "beta": "org.mozilla.firefox_beta",
            "nightly": "org.mozilla.fenix",
            "debug": "org.mozilla.fenix.debug",
        }
        return mapping[channel]
    elif product == PROD_GVE:
        return "org.mozilla.geckoview_example"


def main():
    args = parse_args()

    id = convert_channel_to_id(args.product, args.release_channel)
    filename = id + "-geckoview-config.yaml"

    if args.command == "activate":
        push(id, filename)
    elif args.command == "deactivate":
        remove(filename)


if __name__ == "__main__":
    main()
