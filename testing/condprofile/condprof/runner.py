# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Script that launches profiles creation.
"""
from __future__ import absolute_import
import os
import shutil
import asyncio

from condprof.creator import ProfileCreator
from condprof.desktop import DesktopEnv
from condprof.android import AndroidEnv
from condprof.changelog import Changelog
from condprof.scenarii import scenarii
from condprof.util import logger, get_version, get_current_platform, extract_from_dmg
from condprof.customization import get_customizations, find_customization
from condprof.client import read_changelog, ProfileNotFoundError


class Runner:
    def __init__(
        self,
        profile,
        firefox,
        geckodriver,
        archive,
        device_name,
        strict,
        force_new,
        visible,
        skip_logs=False,
    ):
        self.force_new = force_new
        self.profile = profile
        self.geckodriver = geckodriver
        self.archive = archive
        self.device_name = device_name
        self.strict = strict
        self.visible = visible
        self.skip_logs = skip_logs
        self.env = {}
        # unpacking a dmg
        # XXX do something similar if we get an apk (but later)
        # XXX we want to do
        #   adb install -r target.apk
        #   and get the installed app name
        if firefox is not None and firefox.endswith("dmg"):
            target = os.path.join(os.path.dirname(firefox), "firefox.app")
            extract_from_dmg(firefox, target)
            firefox = os.path.join(target, "Contents", "MacOS", "firefox")
        self.firefox = firefox
        self.android = self.firefox is not None and self.firefox.startswith(
            "org.mozilla"
        )

    def prepare(self, scenario, customization):
        self.scenario = scenario
        self.customization = customization

        # early checks to avoid extra work
        if self.customization != "all":
            if find_customization(self.customization) is None:
                raise IOError("Cannot find customization %r" % self.customization)

        if self.scenario != "all" and self.scenario not in scenarii:
            raise IOError("Cannot find scenario %r" % self.scenario)

        if not self.android and self.firefox is not None:
            logger.info("Verifying Desktop Firefox binary")
            # we want to verify we do have a firefox binary
            # XXX so lame
            if not os.path.exists(self.firefox):
                if "MOZ_FETCHES_DIR" in os.environ:
                    target = os.path.join(os.environ["MOZ_FETCHES_DIR"], self.firefox)
                    if os.path.exists(target):
                        self.firefox = target

            if not os.path.exists(self.firefox):
                raise IOError("Cannot find %s" % self.firefox)

            version = get_version(self.firefox)
            logger.info("Working with Firefox %s" % version)

        logger.info(os.environ)
        if self.archive:
            self.archive = os.path.abspath(self.archive)
            logger.info("Archives directory is %s" % self.archive)
            if not os.path.exists(self.archive):
                os.makedirs(self.archive, exist_ok=True)

        logger.info("Verifying Geckodriver binary presence")
        if shutil.which(self.geckodriver) is None and not os.path.exists(
            self.geckodriver
        ):
            raise IOError("Cannot find %s" % self.geckodriver)

        if not self.skip_logs:
            try:
                if self.android:
                    plat = "%s-%s" % (
                        self.device_name,
                        self.firefox.split("org.mozilla.")[-1],
                    )
                else:
                    plat = get_current_platform()
                self.changelog = read_changelog(plat)
                logger.info("Got the changelog from TaskCluster")
            except ProfileNotFoundError:
                logger.info("changelog not found on TaskCluster, creating a local one.")
                self.changelog = Changelog(self.archive)
        else:
            self.changelog = []

    def _create_env(self):
        if self.android:
            klass = AndroidEnv
        else:
            klass = DesktopEnv

        return klass(
            self.profile, self.firefox, self.geckodriver, self.archive, self.device_name
        )

    def display_error(self, scenario, customization):
        logger.error("%s x %s failed." % (scenario, customization), exc_info=True)
        if self.strict:
            raise

    async def one_run(self, scenario, customization):
        """Runs one single conditioned profile.

        Create an instance of the environment and run the ProfileCreator.
        """
        self.env = self._create_env()
        return await ProfileCreator(
            scenario,
            customization,
            self.archive,
            self.changelog,
            self.force_new,
            self.env,
            skip_logs=self.skip_logs,
        ).run(not self.visible)

    async def run_all(self):
        """Runs the conditioned profile builders"""
        if self.scenario != "all":
            selected_scenario = [self.scenario]
        else:
            selected_scenario = scenarii.keys()

        # this is the loop that generates all combinations of profile
        # for the current platform when "all" is selected
        res = []
        failures = 0
        for scenario in selected_scenario:
            if self.customization != "all":
                try:
                    res.append(await self.one_run(scenario, self.customization))
                except Exception:
                    failures += 1
                    self.display_error(scenario, self.customization)
            else:
                for customization in get_customizations():
                    logger.info("Customization %s" % customization)
                    try:
                        res.append(await self.one_run(scenario, customization))
                    except Exception:
                        failures += 1
                        self.display_error(scenario, customization)

        return failures, [one_res for one_res in res if one_res]

    def save(self):
        self.changelog.save(self.archive)


def run(
    archive,
    firefox=None,
    scenario="all",
    profile=None,
    customization="all",
    visible=False,
    archives_dir="/tmp/archives",
    force_new=False,
    strict=True,
    geckodriver="geckodriver",
    device_name=None,
):
    runner = Runner(
        profile, firefox, geckodriver, archive, device_name, strict, force_new, visible
    )

    runner.prepare(scenario, customization)
    loop = asyncio.get_event_loop()

    try:
        failures, results = loop.run_until_complete(runner.run_all())
        logger.info("Saving changelog in %s" % archive)
        runner.save()
        if failures > 0:
            raise Exception("At least one scenario failed")
    finally:
        loop.close()
