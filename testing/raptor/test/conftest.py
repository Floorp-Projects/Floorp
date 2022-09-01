from __future__ import absolute_import

import json
import os
import sys

import pytest

from argparse import Namespace

# need this so the raptor unit tests can find raptor/raptor classes
here = os.path.abspath(os.path.dirname(__file__))
raptor_dir = os.path.join(os.path.dirname(here), "raptor")
sys.path.insert(0, raptor_dir)

from perftest import Perftest
from webextension import WebExtensionFirefox
from browsertime import Browsertime


@pytest.fixture
def options(request):
    opts = {
        "app": "firefox",
        "binary": "path/to/dummy/browser",
        "browsertime_visualmetrics": False,
        "extra_prefs": {},
    }

    if hasattr(request.module, "OPTIONS"):
        opts.update(request.module.OPTIONS)
    return opts


@pytest.fixture
def browsertime_options(options):
    options["browsertime_node"] = "browsertime_node"
    options["browsertime_browsertimejs"] = "browsertime_browsertimejs"
    options["browsertime_ffmpeg"] = "browsertime_ffmpeg"
    options["browsertime_geckodriver"] = "browsertime_geckodriver"
    options["browsertime_chromedriver"] = "browsertime_chromedriver"
    options["browsertime_video"] = "browsertime_video"
    options["browsertime_visualmetrics"] = "browsertime_visualmetrics"
    options["browsertime_no_ffwindowrecorder"] = "browsertime_no_ffwindowrecorder"
    return options


@pytest.fixture
def raptor(options):
    return WebExtensionFirefox(**options)


@pytest.fixture
def mock_test():
    return {
        "name": "raptor-firefox-tp6",
        "test_url": "/dummy/url",
        "secondary_url": "/dummy/url-2",
    }


@pytest.fixture(scope="session")
def get_prefs():
    def _inner(browser):
        import raptor

        prefs_dir = os.path.join(raptor.__file__, "preferences")
        with open(os.path.join(prefs_dir, "{}.json".format(browser)), "r") as fh:
            return json.load(fh)


@pytest.fixture(scope="session")
def filedir():
    return os.path.join(here, "files")


@pytest.fixture
def get_binary():
    from moztest.selftest import fixtures

    def inner(app):
        if app != "firefox":
            pytest.xfail(reason="{} support not implemented".format(app))

        binary = fixtures.binary()
        if not binary:
            pytest.skip("could not find a {} binary".format(app))
        return binary

    return inner


@pytest.fixture
def create_args():
    args = Namespace(
        app="firefox",
        test="raptor-tp6-unittest",
        binary="path/to/binary",
        gecko_profile=False,
        extra_profiler_run=False,
        debug_mode=False,
        page_cycles=None,
        page_timeout=None,
        test_url_params=None,
        host=None,
        run_local=True,
        browsertime=True,
        cold=False,
        live_sites=False,
        enable_marionette_trace=False,
        collect_perfstats=False,
        chimera=False,
    )

    def inner(**kwargs):
        for next_arg in kwargs:
            print(next_arg)
            print(kwargs[next_arg])
            setattr(args, next_arg, kwargs[next_arg])
        return args

    return inner


@pytest.fixture(scope="module")
def ConcretePerftest():
    class PerftestImplementation(Perftest):
        def check_for_crashes(self):
            super(PerftestImplementation, self).check_for_crashes()

        def clean_up(self):
            super(PerftestImplementation, self).clean_up()

        def run_test(self, test, timeout):
            super(PerftestImplementation, self).run_test(test, timeout)

        def run_test_setup(self, test):
            super(PerftestImplementation, self).run_test_setup(test)

        def run_test_teardown(self, test):
            super(PerftestImplementation, self).run_test_teardown(test)

        def set_browser_test_prefs(self):
            super(PerftestImplementation, self).set_browser_test_prefs()

        def get_browser_meta(self):
            return (), ()

        def setup_chrome_args(self, test):
            super(PerftestImplementation, self).setup_chrome_args(test)

    return PerftestImplementation


@pytest.fixture(scope="module")
def ConcreteBrowsertime():
    class BrowsertimeImplementation(Browsertime):
        @property
        def browsertime_args(self):
            return []

        def get_browser_meta(self):
            return (), ()

        def setup_chrome_args(self, test):
            super(BrowsertimeImplementation, self).setup_chrome_args(test)

    return BrowsertimeImplementation
