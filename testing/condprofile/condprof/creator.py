# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Creates or updates profiles.

The profile creation works as following:

For each scenario:

- The latest indexed profile is picked on TC, if none we create a fresh profile
- The scenario is done against it
- The profile is uploaded on TC, replacing the previous one as the freshest

For each platform we keep a changelog file that keep track of each update
with the Task ID. That offers us the ability to get a profile from a specific
date in the past.

Artifacts are staying in TaskCluster for 3 months, and then they are removed,
so the oldest profile we can get is 3 months old. Profiles are being updated
continuously, so even after 3 months they are still getting "older".

When Firefox changes its version, profiles from the previous version
should work as expected. Each profile tarball comes with a metadata file
that keep track of the Firefox version that was used and the profile age.
"""
from __future__ import absolute_import
import os
import tempfile

from arsenic import get_session
from arsenic.browsers import Firefox

from condprof.util import fresh_profile, logger, obfuscate_file, obfuscate
from condprof.helpers import close_extra_windows
from condprof.scenarii import scenarii
from condprof.client import get_profile, ProfileNotFoundError
from condprof.archiver import Archiver
from condprof.customization import get_customization
from condprof.metadata import Metadata


START, INIT_GECKODRIVER, START_SESSION, START_SCENARIO = range(4)


class ProfileCreator:
    def __init__(
        self,
        scenario,
        customization,
        archive,
        changelog,
        force_new,
        env,
        skip_logs=False,
    ):
        self.env = env
        self.scenario = scenario
        self.customization = customization
        self.archive = archive
        self.changelog = changelog
        self.force_new = force_new
        self.skip_logs = skip_logs
        self.customization_data = get_customization(customization)
        self.tmp_dir = None

        # Make a temporary directory for the logs if an
        # archive dir is not provided
        if not self.archive:
            self.tmp_dir = tempfile.mkdtemp()

    def _log_filename(self, name):
        filename = "%s-%s-%s.log" % (
            name,
            self.scenario,
            self.customization_data["name"],
        )
        return os.path.join(self.archive or self.tmp_dir, filename)

    async def run(self, headless=True):
        logger.info(
            "Building %s x %s" % (self.scenario, self.customization_data["name"])
        )

        if self.scenario in self.customization_data.get("ignore_scenario", []):
            logger.info("Skipping (ignored scenario in that customization)")
            return

        filter_by_platform = self.customization_data.get("platforms")
        if filter_by_platform and self.env.target_platform not in filter_by_platform:
            logger.info("Skipping (ignored platform in that customization)")
            return

        with self.env.get_device(2828, verbose=True) as device:
            try:
                with self.env.get_browser():
                    metadata = await self.build_profile(device, headless)
            finally:
                if not self.skip_logs:
                    self.env.dump_logs()

        if not self.archive:
            return

        logger.info("Creating archive")
        archiver = Archiver(self.scenario, self.env.profile, self.archive)
        # the archive name is of the form
        # profile-<platform>-<scenario>-<customization>.tgz
        name = "profile-%(platform)s-%(name)s-%(customization)s.tgz"
        name = name % metadata
        archive_name = os.path.join(self.archive, name)
        dir = os.path.dirname(archive_name)
        if not os.path.exists(dir):
            os.makedirs(dir)
        archiver.create_archive(archive_name)
        logger.info("Archive created at %s" % archive_name)
        statinfo = os.stat(archive_name)
        logger.info("Current size is %d" % statinfo.st_size)
        logger.info("Extracting logs")
        if "logs" in metadata:
            logs = metadata.pop("logs")
            for prefix, prefixed_logs in logs.items():
                for log in prefixed_logs:
                    content = obfuscate(log["content"])[1]
                    with open(os.path.join(dir, prefix + "-" + log["name"]), "wb") as f:
                        f.write(content.encode("utf-8"))

        if metadata.get("result", 0) != 0:
            logger.info("The scenario returned a bad exit code")
            raise Exception(metadata.get("result_message", "scenario error"))
        self.changelog.append("update", **metadata)

    async def build_profile(self, device, headless):
        scenario = self.scenario
        profile = self.env.profile
        customization_data = self.customization_data

        scenario_func = scenarii[scenario]
        if scenario in customization_data.get("scenario", {}):
            options = customization_data["scenario"][scenario]
            logger.info("Loaded options for that scenario %s" % str(options))
        else:
            options = {}

        # Adding general options
        options["platform"] = self.env.target_platform

        if not self.force_new:
            try:
                custom_name = customization_data["name"]
                get_profile(profile, self.env.target_platform, scenario, custom_name)
            except ProfileNotFoundError:
                # XXX we'll use a fresh profile for now
                fresh_profile(profile, customization_data)
        else:
            fresh_profile(profile, customization_data)

        logger.info("Updating profile located at %r" % profile)
        metadata = Metadata(profile)

        logger.info("Starting the Gecko app...")
        adb_logs = self._log_filename("adb")
        self.env.prepare(logfile=adb_logs)
        geckodriver_logs = self._log_filename("geckodriver")
        logger.info("Writing geckodriver logs in %s" % geckodriver_logs)
        step = START
        try:
            firefox_instance = Firefox(**self.env.get_browser_args(headless))
            step = INIT_GECKODRIVER
            with open(geckodriver_logs, "w") as glog:
                geckodriver = self.env.get_geckodriver(log_file=glog)
                step = START_SESSION
                async with get_session(geckodriver, firefox_instance) as session:
                    step = START_SCENARIO
                    self.env.check_session(session)
                    logger.info("Running the %s scenario" % scenario)
                    metadata.update(await scenario_func(session, options))
                    logger.info("%s scenario done." % scenario)
                    await close_extra_windows(session)
        except Exception:
            logger.error("%s scenario broke!" % scenario)
            if step == START:
                logger.info("Could not initialize the browser")
            elif step == INIT_GECKODRIVER:
                logger.info("Could not initialize Geckodriver")
            elif step == START_SESSION:
                logger.info(
                    "Could not start the session, check %s first" % geckodriver_logs
                )
            else:
                logger.info("Could not run the scenario, probably a faulty scenario")
            raise
        finally:
            self.env.stop_browser()
            for logfile in (adb_logs, geckodriver_logs):
                if os.path.exists(logfile):
                    obfuscate_file(logfile)
        self.env.collect_profile()

        # writing metadata
        metadata.write(
            name=self.scenario,
            customization=self.customization_data["name"],
            version=self.env.get_browser_version(),
            platform=self.env.target_platform,
        )

        logger.info("Profile at %s.\nDone." % profile)
        return metadata
