# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
module to handle Gecko profiling.
"""
from __future__ import absolute_import

import json
import os
import tempfile
import zipfile

import mozfile
from mozlog import get_proxy_logger
from mozgeckoprofiler import ProfileSymbolicator, save_gecko_profile

LOG = get_proxy_logger()


class GeckoProfile(object):
    """
    Handle Gecko profiling.

    This allow to collect Gecko profiling data and to zip results in one file.
    """

    def __init__(self, upload_dir, browser_config, test_config):
        self.upload_dir = upload_dir
        self.browser_config, self.test_config = browser_config, test_config
        self.cleanup = True

        # Create a temporary directory into which the tests can put
        # their profiles. These files will be assembled into one big
        # zip file later on, which is put into the MOZ_UPLOAD_DIR.
        gecko_profile_dir = tempfile.mkdtemp()

        gecko_profile_interval = test_config.get("gecko_profile_interval", 1)
        gecko_profile_entries = test_config.get("gecko_profile_entries", 1000000)
        gecko_profile_features = test_config.get(
            "gecko_profile_features", "js,leaf,stackwalk,cpu,threads"
        )
        gecko_profile_threads = test_config.get(
            "gecko_profile_threads", "GeckoMain,Compositor"
        )
        if browser_config["enable_webrender"]:
            gecko_profile_threads += ",WR,Renderer"

        # Make sure no archive already exists in the location where
        # we plan to output our profiler archive
        self.profile_arcname = os.path.join(
            self.upload_dir, "profile_{0}.zip".format(test_config["name"])
        )
        LOG.info("Clearing archive {0}".format(self.profile_arcname))
        mozfile.remove(self.profile_arcname)

        self.symbol_paths = {
            "FIREFOX": tempfile.mkdtemp(),
            "THUNDERBIRD": tempfile.mkdtemp(),
            "WINDOWS": tempfile.mkdtemp(),
        }

        LOG.info(
            "Activating Gecko Profiling. Temp. profile dir:"
            " {0}, interval: {1}, entries: {2}".format(
                gecko_profile_dir, gecko_profile_interval, gecko_profile_entries
            )
        )

        self.profiling_info = {
            "gecko_profile_interval": gecko_profile_interval,
            "gecko_profile_entries": gecko_profile_entries,
            "gecko_profile_dir": gecko_profile_dir,
            "gecko_profile_features": gecko_profile_features,
            "gecko_profile_threads": gecko_profile_threads,
        }

    def option(self, name):
        return self.profiling_info["gecko_profile_" + name]

    def update_env(self, env):
        """
        update the given env to update some env vars if required.
        """
        if not self.test_config.get("gecko_profile_startup"):
            return
        # Set environment variables which will cause profiling to
        # start as early as possible. These are consumed by Gecko
        # itself, not by Talos JS code.
        env.update(
            {
                "MOZ_PROFILER_STARTUP": "1",
                # Temporary: Don't run Base Profiler, see bug 1630448.
                # TODO: Remove when fix lands in bug 1648324 or bug 1648325.
                "MOZ_PROFILER_STARTUP_NO_BASE": "1",
                "MOZ_PROFILER_STARTUP_INTERVAL": str(self.option("interval")),
                "MOZ_PROFILER_STARTUP_ENTRIES": str(self.option("entries")),
                "MOZ_PROFILER_STARTUP_FEATURES": str(self.option("features")),
                "MOZ_PROFILER_STARTUP_FILTERS": str(self.option("threads")),
            }
        )

    def _save_gecko_profile(
        self, cycle, symbolicator, missing_symbols_zip, profile_path
    ):
        try:
            with open(profile_path, "r") as profile_file:
                profile = json.load(profile_file)
            symbolicator.dump_and_integrate_missing_symbols(
                profile, missing_symbols_zip
            )
            symbolicator.symbolicate_profile(profile)
            save_gecko_profile(profile, profile_path)
        except MemoryError:
            LOG.critical(
                "Ran out of memory while trying"
                " to symbolicate profile {0} (cycle {1})".format(profile_path, cycle),
                exc_info=True,
            )
        except Exception:
            LOG.critical(
                "Encountered an exception during profile"
                " symbolication {0} (cycle {1})".format(profile_path, cycle),
                exc_info=True,
            )

    def symbolicate(self, cycle):
        """
        Symbolicate Gecko profiling data for one cycle.

        :param cycle: the number of the cycle of the test currently run.
        """
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

        if self.browser_config["symbols_path"]:
            if mozfile.is_url(self.browser_config["symbols_path"]):
                symbolicator.integrate_symbol_zip_from_url(
                    self.browser_config["symbols_path"]
                )
            elif os.path.isfile(self.browser_config["symbols_path"]):
                symbolicator.integrate_symbol_zip_from_file(
                    self.browser_config["symbols_path"]
                )
            elif os.path.isdir(self.browser_config["symbols_path"]):
                sym_path = self.browser_config["symbols_path"]
                symbolicator.options["symbolPaths"]["FIREFOX"] = sym_path
                self.cleanup = False

        missing_symbols_zip = os.path.join(self.upload_dir, "missingsymbols.zip")

        try:
            mode = zipfile.ZIP_DEFLATED
        except NameError:
            mode = zipfile.ZIP_STORED

        gecko_profile_dir = self.option("dir")

        with zipfile.ZipFile(self.profile_arcname, "a", mode) as arc:
            # Collect all individual profiles that the test
            # has put into gecko_profile_dir.
            for profile_filename in os.listdir(gecko_profile_dir):
                testname = profile_filename
                if testname.endswith(".profile"):
                    testname = testname[0:-8]
                profile_path = os.path.join(gecko_profile_dir, profile_filename)
                self._save_gecko_profile(
                    cycle, symbolicator, missing_symbols_zip, profile_path
                )

                # Our zip will contain one directory per subtest,
                # and each subtest directory will contain one or
                # more cycle_i.profile files. For example, with
                # test_config['name'] == 'tscrollx',
                # profile_filename == 'iframe.svg.profile', i == 0,
                # we'll get path_in_zip ==
                # 'profile_tscrollx/iframe.svg/cycle_0.profile'.
                cycle_name = "cycle_{0}.profile".format(cycle)
                path_in_zip = os.path.join(
                    "profile_{0}".format(self.test_config["name"]), testname, cycle_name
                )
                LOG.info(
                    "Adding profile {0} to archive {1}".format(
                        path_in_zip, self.profile_arcname
                    )
                )
                try:
                    arc.write(profile_path, path_in_zip)
                except Exception:
                    LOG.exception(
                        "Failed to copy profile {0} as {1} to"
                        " archive {2}".format(
                            profile_path, path_in_zip, self.profile_arcname
                        )
                    )
            # save the latest gecko profile archive to an env var, so later on
            # it can be viewed automatically via the view-gecko-profile tool
            os.environ["TALOS_LATEST_GECKO_PROFILE_ARCHIVE"] = self.profile_arcname

    def clean(self):
        """
        Clean up temp folders created with the instance creation.
        """
        mozfile.remove(self.option("dir"))
        if self.cleanup:
            for symbol_path in self.symbol_paths.values():
                mozfile.remove(symbol_path)
