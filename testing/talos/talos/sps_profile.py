# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
module to handle sps profilling.
"""

import os
import tempfile
import zipfile
import json
import mozfile

from mozlog import get_proxy_logger

from talos.profiler import symbolication, sps

LOG = get_proxy_logger()


class SpsProfile(object):
    """
    Handle sps profilling.

    This allow to collect sps profiling data and to zip results in one file.
    """
    def __init__(self, upload_dir, browser_config, test_config):
        self.upload_dir = upload_dir
        self.browser_config, self.test_config = browser_config, test_config

        # Create a temporary directory into which the tests can put
        # their profiles. These files will be assembled into one big
        # zip file later on, which is put into the MOZ_UPLOAD_DIR.
        sps_profile_dir = tempfile.mkdtemp()

        sps_profile_interval = test_config.get('sps_profile_interval', 1)
        sps_profile_entries = test_config.get('sps_profile_entries', 1000000)
        sps_profile_threads = 'GeckoMain,Compositor'

        # Make sure no archive already exists in the location where
        # we plan to output our profiler archive
        self.profile_arcname = os.path.join(
            self.upload_dir,
            "profile_{0}.sps.zip".format(test_config['name'])
        )
        LOG.info("Clearing archive {0}".format(self.profile_arcname))
        mozfile.remove(self.profile_arcname)

        self.symbol_paths = {
            'FIREFOX': tempfile.mkdtemp(),
            'THUNDERBIRD': tempfile.mkdtemp(),
            'WINDOWS': tempfile.mkdtemp()
        }

        LOG.info("Activating Gecko Profiling. Temp. profile dir:"
                 " {0}, interval: {1}, entries: {2}"
                 .format(sps_profile_dir,
                         sps_profile_interval,
                         sps_profile_entries))

        self.profiling_info = {
            "sps_profile_interval": sps_profile_interval,
            "sps_profile_entries": sps_profile_entries,
            "sps_profile_dir": sps_profile_dir,
            "sps_profile_threads": sps_profile_threads
        }

    def option(self, name):
        return self.profiling_info["sps_profile_" + name]

    def update_env(self, env):
        """
        update the given env to update some env vars if required.
        """
        if not self.test_config.get('sps_profile_startup'):
            return
        # Set environment variables which will cause profiling to
        # start as early as possible. These are consumed by Gecko
        # itself, not by Talos JS code.
        env.update({
            'MOZ_PROFILER_STARTUP': '1',
            'MOZ_PROFILER_INTERVAL': str(self.option('interval')),
            'MOZ_PROFILER_ENTRIES': str(self.option('entries')),
            "MOZ_PROFILER_THREADS": str(self.option('threads'))
        })

    def _save_sps_profile(self, cycle, symbolicator, missing_symbols_zip,
                          profile_path):
        try:
            with open(profile_path, 'r') as profile_file:
                profile = json.load(profile_file)
            symbolicator.dump_and_integrate_missing_symbols(
                profile,
                missing_symbols_zip)
            symbolicator.symbolicate_profile(profile)
            sps.save_profile(profile, profile_path)
        except MemoryError:
            LOG.critical(
                "Ran out of memory while trying"
                " to symbolicate profile {0} (cycle {1})"
                .format(profile_path, cycle),
                exc_info=True
            )
        except Exception:
            LOG.critical("Encountered an exception during profile"
                         " symbolication {0} (cycle {1})"
                         .format(profile_path, cycle),
                         exc_info=True)

    def symbolicate(self, cycle):
        """
        Symbolicate sps profiling data for one cycle.

        :param cycle: the number of the cycle of the test currently run.
        """
        symbolicator = symbolication.ProfileSymbolicator({
            # Trace-level logging (verbose)
            "enableTracing": 0,
            # Fallback server if symbol is not found locally
            "remoteSymbolServer":
                "http://symbolapi.mozilla.org:80/talos/",
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
            "symbolPaths": self.symbol_paths
        })

        if self.browser_config['symbols_path']:
            if mozfile.is_url(self.browser_config['symbols_path']):
                symbolicator.integrate_symbol_zip_from_url(
                    self.browser_config['symbols_path']
                )
            else:
                symbolicator.integrate_symbol_zip_from_file(
                    self.browser_config['symbols_path']
                )

        missing_symbols_zip = os.path.join(self.upload_dir,
                                           "missingsymbols.zip")

        try:
            mode = zipfile.ZIP_DEFLATED
        except NameError:
            mode = zipfile.ZIP_STORED

        sps_profile_dir = self.option('dir')

        with zipfile.ZipFile(self.profile_arcname, 'a', mode) as arc:
            # Collect all individual profiles that the test
            # has put into sps_profile_dir.
            for profile_filename in os.listdir(sps_profile_dir):
                testname = profile_filename
                if testname.endswith(".sps"):
                    testname = testname[0:-4]
                profile_path = os.path.join(sps_profile_dir, profile_filename)
                self._save_sps_profile(cycle, symbolicator,
                                       missing_symbols_zip,
                                       profile_path)

                # Our zip will contain one directory per subtest,
                # and each subtest directory will contain one or
                # more cycle_i.sps files. For example, with
                # test_config['name'] == 'tscrollx',
                # profile_filename == 'iframe.svg.sps', i == 0,
                # we'll get path_in_zip ==
                # 'profile_tscrollx/iframe.svg/cycle_0.sps'.
                cycle_name = "cycle_{0}.sps".format(cycle)
                path_in_zip = \
                    os.path.join(
                        "profile_{0}".format(self.test_config['name']),
                        testname,
                        cycle_name
                    )
                LOG.info(
                    "Adding profile {0} to archive {1}"
                    .format(path_in_zip, self.profile_arcname)
                )
                try:
                    arc.write(profile_path, path_in_zip)
                except Exception:
                    LOG.exception(
                        "Failed to copy profile {0} as {1} to"
                        " archive {2}".format(profile_path,
                                              path_in_zip,
                                              self.profile_arcname)
                    )

    def clean(self):
        """
        Clean up temp folders created with the instance creation.
        """
        mozfile.remove(self.option('dir'))
        for symbol_path in self.symbol_paths.values():
            mozfile.remove(symbol_path)
