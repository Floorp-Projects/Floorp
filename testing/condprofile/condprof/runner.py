# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Script that launches profiles creation.
"""
import os
import argparse
import asyncio
import sys
import shutil

# easier than setting PYTHONPATH in various platforms
if __name__ == "__main__":
    sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

from condprof.check_install import check  # NOQA

check()  # NOQA

from condprof.creator import ProfileCreator  # NOQA
from condprof.desktop import DesktopEnv  # NOQA
from condprof.android import AndroidEnv  # NOQA
from condprof.changelog import Changelog  # NOQA
from condprof.scenarii import scenarii  # NOQA
from condprof.util import (
    LOG,
    ERROR,
    get_version,
    get_current_platform,
    extract_from_dmg,
)  # NOQA
from condprof.customization import get_customizations  # NOQA
from condprof.client import read_changelog, ProfileNotFoundError  # NOQA


def main(args=sys.argv[1:]):
    parser = argparse.ArgumentParser(description="Profile Creator")
    parser.add_argument("archive", help="Archives Dir", type=str, default=None)
    parser.add_argument("--firefox", help="Firefox Binary", type=str, default=None)
    parser.add_argument("--scenario", help="Scenario to use", type=str, default="all")
    parser.add_argument(
        "--profile", help="Existing profile Dir", type=str, default=None
    )
    parser.add_argument(
        "--customization", help="Profile customization to use", type=str, default="all"
    )
    parser.add_argument(
        "--fresh-profile",
        help="Create a fresh profile",
        action="store_true",
        default=False,
    )
    parser.add_argument(
        "--visible", help="Don't use headless mode", action="store_true", default=False
    )
    parser.add_argument(
        "--archives-dir", help="Archives local dir", type=str, default="/tmp/archives"
    )
    parser.add_argument(
        "--force-new", help="Create from scratch", action="store_true", default=False
    )
    parser.add_argument(
        "--strict",
        help="Errors out immediatly on a scenario failure",
        action="store_true",
        default=True,
    )
    parser.add_argument(
        "--geckodriver",
        help="Path to the geckodriver binary",
        type=str,
        default=sys.platform.startswith("win") and "geckodriver.exe" or "geckodriver",
    )

    parser.add_argument(
        "--device-name", help="Name of the device", type=str, default=None
    )

    args = parser.parse_args(args=args)

    # unpacking a dmg
    # XXX do something similar if we get an apk (but later)
    # XXX we want to do
    #   adb install -r target.apk
    #   and get the installed app name
    if args.firefox is not None and args.firefox.endswith("dmg"):
        target = os.path.join(os.path.dirname(args.firefox), "firefox.app")
        extract_from_dmg(args.firefox, target)
        args.firefox = os.path.join(target, "Contents", "MacOS", "firefox")

    args.android = args.firefox is not None and args.firefox.startswith("org.mozilla")

    if not args.android and args.firefox is not None:
        LOG("Verifying Desktop Firefox binary")
        # we want to verify we do have a firefox binary
        # XXX so lame
        if not os.path.exists(args.firefox):
            if "MOZ_FETCHES_DIR" in os.environ:
                target = os.path.join(os.environ["MOZ_FETCHES_DIR"], args.firefox)
                if os.path.exists(target):
                    args.firefox = target

        if not os.path.exists(args.firefox):
            raise IOError("Cannot find %s" % args.firefox)

        version = get_version(args.firefox)
        LOG("Working with Firefox %s" % version)

    LOG(os.environ)
    args.archive = os.path.abspath(args.archive)
    LOG("Archives directory is %s" % args.archive)
    if not os.path.exists(args.archive):
        os.makedirs(args.archive, exist_ok=True)

    LOG("Verifying Geckodriver binary presence")
    if shutil.which(args.geckodriver) is None and not os.path.exists(args.geckodriver):
        raise IOError("Cannot find %s" % args.geckodriver)

    try:
        if args.android:
            plat = "%s-%s" % (args.device_name, args.firefox.split("org.mozilla.")[-1])
        else:
            plat = get_current_platform()
        changelog = read_changelog(plat)
        LOG("Got the changelog from TaskCluster")
    except ProfileNotFoundError:
        LOG("changelog not found on TaskCluster, creating a local one.")
        changelog = Changelog(args.archive)
    loop = asyncio.get_event_loop()

    async def one_run(scenario, customization):
        if args.android:
            env = AndroidEnv(
                args.profile,
                args.firefox,
                args.geckodriver,
                args.archive,
                args.device_name,
            )
        else:
            env = DesktopEnv(
                args.profile,
                args.firefox,
                args.geckodriver,
                args.archive,
                args.device_name,
            )
        return await ProfileCreator(
            scenario, customization, args.archive, changelog, args.force_new, env
        ).run(not args.visible)

    async def run_all(args):
        if args.scenario != "all":
            selected_scenario = [args.scenario]
        else:
            selected_scenario = scenarii.keys()

        # this is the loop that generates all combinations of profile
        # for the current platform when "all" is selected
        res = []
        failures = 0
        for scenario in selected_scenario:
            if args.customization != "all":
                try:
                    res.append(await one_run(scenario, args.customization))
                except Exception:
                    failures += 1
                    ERROR("Something went wrong on this one.")
                    if args.strict:
                        raise
            else:
                for customization in get_customizations():
                    LOG("Customization %s" % customization)
                    try:
                        res.append(await one_run(scenario, customization))
                    except Exception:
                        failures += 1
                        ERROR("Something went wrong on this one.")
                        if args.strict:
                            raise
        return failures, [one_res for one_res in res if one_res]

    try:
        failures, results = loop.run_until_complete(run_all(args))
        LOG("Saving changelog in %s" % args.archive)
        changelog.save(args.archive)
        if failures > 0:
            raise Exception("At least one scenario failed")
    finally:
        loop.close()


if __name__ == "__main__":
    main()
