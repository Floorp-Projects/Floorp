# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import json
import tempfile
import shutil
import os
import mozfile
from .symbolication import ProfileSymbolicator
from mozlog import get_proxy_logger

LOG = get_proxy_logger("profiler")


def save_gecko_profile(profile, filename):
    with open(filename, "w") as f:
        json.dump(profile, f)


def symbolicate_profile_json(profile_path, objdir_path):
    """
    Symbolicate a single JSON profile.
    """
    temp_dir = tempfile.mkdtemp()
    firefox_symbol_path = os.path.join(temp_dir, "firefox")
    windows_symbol_path = os.path.join(temp_dir, "windows")
    os.mkdir(firefox_symbol_path)
    os.mkdir(windows_symbol_path)

    symbol_paths = {"FIREFOX": firefox_symbol_path, "WINDOWS": windows_symbol_path}

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
            "symbolPaths": symbol_paths,
        }
    )

    symbol_path = os.path.join(objdir_path, "dist", "crashreporter-symbols")
    missing_symbols_zip = os.path.join(tempfile.mkdtemp(), "missingsymbols.zip")

    if mozfile.is_url(symbol_path):
        symbolicator.integrate_symbol_zip_from_url(symbol_path)
    elif os.path.isfile(symbol_path):
        symbolicator.integrate_symbol_zip_from_file(symbol_path)
    elif os.path.isdir(symbol_path):
        symbol_paths["FIREFOX"] = symbol_path

    LOG.info(
        "Symbolicating the performance profile... This could take a couple "
        "of minutes."
    )

    try:
        with open(profile_path, "r") as profile_file:
            profile = json.load(profile_file)
        symbolicator.dump_and_integrate_missing_symbols(profile, missing_symbols_zip)
        symbolicator.symbolicate_profile(profile)
        # Overwrite the profile in place.
        save_gecko_profile(profile, profile_path)
    except MemoryError:
        LOG.error(
            "Ran out of memory while trying"
            " to symbolicate profile {0}".format(profile_path)
        )
    except Exception as e:
        LOG.error("Encountered an exception during profile symbolication")
        LOG.error(e)

    shutil.rmtree(temp_dir)
