# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Module to handle chrom* profiling.
"""

import json
import os
import zipfile
from pathlib import Path

import mozfile
from logger.logger import RaptorLogger

here = Path(__file__).resolve().parent
LOG = RaptorLogger(component="raptor-chrome-trace")
from raptor_profiling import RaptorProfiling


class ChromeTrace(RaptorProfiling):
    """
    Handle Tracing for chrom* applications.

    This allows us to collect Chrome profiling data and to zip results into one file.
    """

    def __init__(self, upload_dir, raptor_config, test_config):
        super().__init__(upload_dir, raptor_config, test_config)

        # define the key in the results json for traces
        self.profile_entry_string = "timeline"

        # Make sure no archive already exists in the location where
        # we plan to output our profiler archive
        self.profile_arcname = Path(
            self.upload_dir, "profile_{0}.zip".format(self.test_config["name"])
        )
        LOG.info("Clearing archive {0}".format(self.profile_arcname))
        mozfile.remove(self.profile_arcname)

        LOG.info(
            "Activating chrome tracing! temp profile dir: {0}".format(
                self.temp_profile_dir
            )
        )

    @property
    def _is_extra_profiler_run(self):
        return self.raptor_config.get("extra_profiler_run", False)

    def output_trace(self):
        """Collect and output Chrome Trace data for one pagecycle

        Write the profiles into a set of folders formatted as:
        <TEST-NAME>-<TEST-RUN-TYPE>.
        <TEST-RUN-TYPE> can be pageload-{warm,cold} or {test-type}
        only for the tests that are not a pageload test.
        For example, "cnn-pageload-warm".
        The file names are formatted as <ITERATION-TYPE>-<ITERATION>
        to clearly indicate without redundant information.
        For example, "browser-cycle-1".
        """

        profiles = self.collect_profiles()
        if len(profiles) == 0:
            if self._is_extra_profiler_run:
                LOG.info("No profiles collected in the extra profiler run")
            else:
                LOG.error("No profiles collected")
            return

        test_type = self.test_config.get("type", "pageload")
        try:
            mode = zipfile.ZIP_DEFLATED
        except NameError:
            mode = zipfile.ZIP_STORED

        with zipfile.ZipFile(self.profile_arcname, "a", mode) as arc:
            for profile_info in profiles:
                profile_path = profile_info["path"]

                LOG.info("Opening profile at %s" % profile_path)
                try:
                    profile = self._open_profile_file(profile_path)
                except FileNotFoundError:
                    if self._is_extra_profiler_run:
                        LOG.info("Trace not found on extra profiler run.")
                    else:
                        LOG.error("Trace not found.")
                    continue

                try:
                    test_run_type = (
                        "{0}-{1}".format(test_type, profile_info["type"])
                        if test_type == "pageload"
                        else test_type
                    )
                    folder_name = "%s-%s" % (self.test_config["name"], test_run_type)
                    iteration = Path(profile_path).parts[-1].split("-")[-1]
                    if test_type == "pageload" and profile_info["type"] == "cold":
                        iteration_type = "browser-cycle"
                    elif profile_info["type"] == "warm":
                        iteration_type = "page-cycle"
                    else:
                        iteration_type = "iteration"
                    profile_name = "-".join([iteration_type, iteration])
                    path_in_zip = Path(folder_name, profile_name)

                    LOG.info(
                        "Adding profile %s to archive %s as %s"
                        % (profile_path, self.profile_arcname, path_in_zip)
                    )
                    arc.writestr(
                        str(path_in_zip),
                        json.dumps(profile, ensure_ascii=False).encode("utf-8"),
                    )
                except Exception:
                    LOG.exception(
                        "Failed to add profile %s to archive %s"
                        % (profile_path, self.profile_arcname)
                    )
                    raise

        # save the latest chrome trace archive to an env var, so later on
        # it can be viewed automatically via the view-gecko-profile tool.
        # since we are using the firefox profiler to view the trace, it
        # is convenient to just use the same env var.
        os.environ["RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE"] = str(
            self.profile_arcname
        )  # convert posixpath object to string for environment

    def clean(self):
        """
        Clean up temp folders created with the instance creation.
        """
        mozfile.remove(self.temp_profile_dir)
