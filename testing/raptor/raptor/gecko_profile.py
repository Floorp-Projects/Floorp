# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
module to handle Gecko profiling.
"""
from __future__ import absolute_import

import gzip
import json
import os
import tempfile
import zipfile

import mozfile

from logger.logger import RaptorLogger
from mozgeckoprofiler import ProfileSymbolicator

here = os.path.dirname(os.path.realpath(__file__))
LOG = RaptorLogger(component="raptor-gecko-profile")


class GeckoProfile(object):
    """
    Handle Gecko profiling.

    This allows us to collect Gecko profiling data and to zip results into one file.
    """

    def __init__(self, upload_dir, raptor_config, test_config):
        self.upload_dir = upload_dir
        self.raptor_config = raptor_config
        self.test_config = test_config
        self.cleanup = True

        # Create a temporary directory into which the tests can put
        # their profiles. These files will be assembled into one big
        # zip file later on, which is put into the MOZ_UPLOAD_DIR.
        self.gecko_profile_dir = tempfile.mkdtemp()

        # Each test INI can specify gecko_profile_interval and entries but they
        # can be overrided by user input.
        gecko_profile_interval = raptor_config.get(
            "gecko_profile_interval", None
        ) or test_config.get("gecko_profile_interval", 1)
        gecko_profile_entries = raptor_config.get(
            "gecko_profile_entries", None
        ) or test_config.get("gecko_profile_entries", 1000000)

        # We need symbols_path; if it wasn't passed in on cmdline, set it
        # use objdir/dist/crashreporter-symbols for symbolsPath if none provided
        if (
            not self.raptor_config["symbols_path"]
            and self.raptor_config["run_local"]
            and "MOZ_DEVELOPER_OBJ_DIR" in os.environ
        ):
            self.raptor_config["symbols_path"] = os.path.join(
                os.environ["MOZ_DEVELOPER_OBJ_DIR"], "dist", "crashreporter-symbols"
            )

        # turn on crash reporter if we have symbols
        os.environ["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        if self.raptor_config["symbols_path"]:
            os.environ["MOZ_CRASHREPORTER"] = "1"
        else:
            os.environ["MOZ_CRASHREPORTER_DISABLE"] = "1"

        # Make sure no archive already exists in the location where
        # we plan to output our profiler archive
        self.profile_arcname = os.path.join(
            self.upload_dir, "profile_{0}.zip".format(test_config["name"])
        )
        LOG.info("Clearing archive {0}".format(self.profile_arcname))
        mozfile.remove(self.profile_arcname)

        self.symbol_paths = {
            "FIREFOX": tempfile.mkdtemp(),
            "WINDOWS": tempfile.mkdtemp(),
        }

        LOG.info(
            "Activating gecko profiling, temp profile dir:"
            " {0}, interval: {1}, entries: {2}".format(
                self.gecko_profile_dir, gecko_profile_interval, gecko_profile_entries
            )
        )

    def _open_gecko_profile(self, profile_path):
        """Open a gecko profile and return the contents."""
        if profile_path.endswith(".gz"):
            with gzip.open(profile_path, "r") as profile_file:
                profile = json.load(profile_file)
        else:
            with open(profile_path, "r", encoding="utf-8") as profile_file:
                profile = json.load(profile_file)
        return profile

    def _symbolicate_profile(self, profile, missing_symbols_zip, symbolicator):
        try:
            symbolicator.dump_and_integrate_missing_symbols(
                profile, missing_symbols_zip
            )
            symbolicator.symbolicate_profile(profile)
            return profile
        except MemoryError:
            LOG.critical("Ran out of memory while trying to symbolicate profile")
            raise
        except Exception:
            LOG.critical("Encountered an exception during profile symbolication")
            # Do not raise an exception and return the profile so we won't block
            # the profile capturing pipeline if symbolication fails.
            return profile

    def collect_profiles(self):
        """Returns all profiles files."""

        def __get_test_type():
            """Returns the type of test that was run.

            For benchmark/scenario tests, we return those specific types,
            but for pageloads we return cold or warm depending on the --cold
            flag.
            """
            if self.test_config.get("type", "pageload") not in (
                "benchmark",
                "scenario",
            ):
                return "cold" if self.raptor_config.get("cold", False) else "warm"
            else:
                return self.test_config.get("type", "benchmark")

        res = []
        if self.raptor_config.get("browsertime"):
            topdir = self.raptor_config.get("browsertime_result_dir")

            # Get the browsertime.json file along with the cold/warm splits
            # if they exist from a chimera test
            results = {"main": None, "cold": None, "warm": None}
            profiling_dir = os.path.join(topdir, "profiling")
            is_extra_profiler_run = self.raptor_config.get(
                "extra_profiler_run", False
            ) and os.path.isdir(profiling_dir)
            result_dir = profiling_dir if is_extra_profiler_run else topdir

            for filename in os.listdir(result_dir):
                if filename == "browsertime.json":
                    results["main"] = os.path.join(result_dir, filename)
                elif filename == "cold-browsertime.json":
                    results["cold"] = os.path.join(result_dir, filename)
                elif filename == "warm-browsertime.json":
                    results["warm"] = os.path.join(result_dir, filename)
                if all(results.values()):
                    break

            if not any(results.values()):
                if is_extra_profiler_run:
                    LOG.info(
                        "Could not find any browsertime result JSONs in the artifacts "
                        " for the extra profiler run"
                    )
                    return []
                else:
                    raise Exception(
                        "Could not find any browsertime result JSONs in the artifacts"
                    )

            profile_locations = []
            if self.raptor_config.get("chimera", False) and not is_extra_profiler_run:
                if results["warm"] is None or results["cold"] is None:
                    raise Exception(
                        "The test ran in chimera mode but we found no cold "
                        "and warm browsertime JSONs. Cannot symbolicate profiles."
                    )
                profile_locations.extend(
                    [("cold", results["cold"]), ("warm", results["warm"])]
                )
            else:
                # When we don't run in chimera mode, it means that we
                # either ran a benchmark, scenario test, separate
                # warm/cold pageload tests or extra profiling run.
                profile_locations.append(
                    (
                        __get_test_type(),
                        results["main"],
                    )
                )

            for testtype, results_json in profile_locations:
                with open(results_json, encoding="utf-8") as f:
                    data = json.load(f)
                results_dir = os.path.dirname(results_json)
                for entry in data:
                    for rel_profile_path in entry["files"]["geckoProfiles"]:
                        res.append(
                            {
                                "path": os.path.join(results_dir, rel_profile_path),
                                "type": testtype,
                            }
                        )
        else:
            # Raptor-webext stores its profiles in the self.gecko_profile_dir
            # directory
            for profile in os.listdir(self.gecko_profile_dir):
                res.append(
                    {
                        "path": os.path.join(self.gecko_profile_dir, profile),
                        "type": __get_test_type(),
                    }
                )

        LOG.info("Found %s profiles: %s" % (len(res), str(res)))
        return res

    def symbolicate(self):
        """
        Symbolicate Gecko profiling data for one pagecycle.

        """
        profiles = self.collect_profiles()
        if len(profiles) == 0:
            LOG.error("No profiles collected")
            return

        symbolicator = ProfileSymbolicator(
            {
                # Trace-level logging (verbose)
                "enableTracing": 0,
                # Fallback server if symbol is not found locally
                "remoteSymbolServer": "https://symbols.mozilla.org/symbolicate/v4",
                # Maximum number of symbol files to keep in memory
                "maxCacheEntries": 2000000,
                # Frequency of checking for recent symbols to
                # cache (in hours)
                "prefetchInterval": 12,
                # Oldest file age to prefetch (in hours)
                "prefetchThreshold": 48,
                # Maximum number of library versions to pre-fetch
                # per library
                "prefetchMaxSymbolsPerLib": 3,
                # Default symbol lookup directories
                "defaultApp": "FIREFOX",
                "defaultOs": "WINDOWS",
                # Paths to .SYM files, expressed internally as a
                # mapping of app or platform names to directories
                # Note: App & OS names from requests are converted
                # to all-uppercase internally
                "symbolPaths": self.symbol_paths,
            }
        )

        if self.raptor_config.get("symbols_path") is not None:
            if mozfile.is_url(self.raptor_config["symbols_path"]):
                symbolicator.integrate_symbol_zip_from_url(
                    self.raptor_config["symbols_path"]
                )
            elif os.path.isfile(self.raptor_config["symbols_path"]):
                symbolicator.integrate_symbol_zip_from_file(
                    self.raptor_config["symbols_path"]
                )
            elif os.path.isdir(self.raptor_config["symbols_path"]):
                sym_path = self.raptor_config["symbols_path"]
                symbolicator.options["symbolPaths"]["FIREFOX"] = sym_path
                self.cleanup = False

        missing_symbols_zip = os.path.join(self.upload_dir, "missingsymbols.zip")

        try:
            mode = zipfile.ZIP_DEFLATED
        except NameError:
            mode = zipfile.ZIP_STORED

        with zipfile.ZipFile(self.profile_arcname, "a", mode) as arc:
            for profile_info in profiles:
                profile_path = profile_info["path"]

                LOG.info("Opening profile at %s" % profile_path)
                profile = self._open_gecko_profile(profile_path)

                LOG.info("Symbolicating profile from %s" % profile_path)
                symbolicated_profile = self._symbolicate_profile(
                    profile, missing_symbols_zip, symbolicator
                )

                try:
                    # Write the profiles into a set of folders formatted as:
                    # <TEST-NAME>-<TEST_TYPE>. The file names have a count prefixed
                    # to them to prevent any naming conflicts. The count is the
                    # number of files already in the folder.
                    folder_name = "%s-%s" % (
                        self.test_config["name"],
                        profile_info["type"],
                    )
                    profile_name = "-".join(
                        [
                            str(
                                len([f for f in arc.namelist() if folder_name in f]) + 1
                            ),
                            os.path.split(profile_path)[-1],
                        ]
                    )
                    path_in_zip = os.path.join(folder_name, profile_name)

                    LOG.info(
                        "Adding profile %s to archive %s as %s"
                        % (profile_path, self.profile_arcname, path_in_zip)
                    )
                    arc.writestr(
                        path_in_zip,
                        json.dumps(symbolicated_profile, ensure_ascii=False).encode(
                            "utf-8"
                        ),
                    )
                except Exception:
                    LOG.exception(
                        "Failed to add symbolicated profile %s to archive %s"
                        % (profile_path, self.profile_arcname)
                    )
                    raise

        # save the latest gecko profile archive to an env var, so later on
        # it can be viewed automatically via the view-gecko-profile tool
        os.environ["RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE"] = self.profile_arcname

    def clean(self):
        """
        Clean up temp folders created with the instance creation.
        """
        mozfile.remove(self.gecko_profile_dir)
        if self.cleanup:
            for symbol_path in self.symbol_paths.values():
                mozfile.remove(symbol_path)
