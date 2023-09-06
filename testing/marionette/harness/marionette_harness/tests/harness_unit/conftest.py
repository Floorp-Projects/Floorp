# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pytest

from unittest.mock import Mock, MagicMock

from marionette_driver.marionette import Marionette

from marionette_harness.runner.httpd import FixtureServer


@pytest.fixture(scope="module")
def logger():
    """
    Fake logger to help with mocking out other runner-related classes.
    """
    import mozlog

    return Mock(spec=mozlog.structuredlog.StructuredLogger)


@pytest.fixture
def mach_parsed_kwargs(logger):
    """
    Parsed and verified dictionary used during simplest
    call to mach marionette-test
    """
    return {
        "adb_path": None,
        "addons": None,
        "address": None,
        "app": None,
        "app_args": [],
        "avd": None,
        "avd_home": None,
        "binary": "/path/to/firefox",
        "browsermob_port": None,
        "browsermob_script": None,
        "device_serial": None,
        "emulator": False,
        "emulator_bin": None,
        "gecko_log": None,
        "jsdebugger": False,
        "log_errorsummary": None,
        "log_html": None,
        "log_mach": None,
        "log_mach_buffer": None,
        "log_mach_level": None,
        "log_mach_verbose": None,
        "log_raw": None,
        "log_raw_level": None,
        "log_tbpl": None,
        "log_tbpl_buffer": None,
        "log_tbpl_compact": None,
        "log_tbpl_level": None,
        "log_unittest": None,
        "log_xunit": None,
        "logger_name": "Marionette-based Tests",
        "prefs": {},
        "prefs_args": None,
        "prefs_files": None,
        "profile": None,
        "pydebugger": None,
        "repeat": None,
        "run_until_failure": None,
        "server_root": None,
        "shuffle": False,
        "shuffle_seed": 2276870381009474531,
        "socket_timeout": 60.0,
        "startup_timeout": 60,
        "symbols_path": None,
        "test_tags": None,
        "tests": ["/path/to/unit-tests.ini"],
        "testvars": None,
        "this_chunk": None,
        "timeout": None,
        "total_chunks": None,
        "verbose": None,
        "workspace": None,
        "logger": logger,
    }


@pytest.fixture
def mock_httpd(request):
    """Mock httpd instance"""
    httpd = MagicMock(spec=FixtureServer)
    return httpd


@pytest.fixture
def mock_marionette(request):
    """Mock marionette instance"""
    marionette = MagicMock(spec=dir(Marionette()))
    if "has_crashed" in request.fixturenames:
        marionette.check_for_crash.return_value = request.getfixturevalue("has_crashed")
    return marionette
