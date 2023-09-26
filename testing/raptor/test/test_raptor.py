import os
import sys
import threading
import traceback
from unittest import mock

import mozunit
import pytest
from mozprofile import BaseProfile

# need this so the raptor unit tests can find output & filter classes
here = os.path.abspath(os.path.dirname(__file__))
raptor_dir = os.path.join(os.path.dirname(here), "raptor")
sys.path.insert(0, raptor_dir)


from browsertime import BrowsertimeAndroid, BrowsertimeDesktop

DEFAULT_TIMEOUT = 125


class TestBrowserThread(threading.Thread):
    def __init__(self, raptor_instance, tests, names):
        super(TestBrowserThread, self).__init__()
        self.raptor_instance = raptor_instance
        self.tests = tests
        self.names = names
        self.exc = None

    def print_error(self):
        if self.exc is None:
            return
        type, value, tb = self.exc
        traceback.print_exception(type, value, tb, None, sys.stderr)

    def run(self):
        try:
            self.raptor_instance.run_tests(self.tests, self.names)
        except BaseException:
            self.exc = sys.exc_info()


# Perftest tests
@pytest.mark.parametrize(
    "perftest_class, app_name",
    [
        [BrowsertimeDesktop, "firefox"],
        [BrowsertimeAndroid, "geckoview"],
    ],
)
def test_build_profile(options, perftest_class, app_name, get_prefs):
    options["app"] = app_name

    # We need to do the mock ourselves because of how the perftest_class
    # is being defined
    original_get = perftest_class.get_browser_meta
    perftest_class.get_browser_meta = mock.MagicMock()
    perftest_class.get_browser_meta.return_value = (app_name, "100")

    perftest_instance = perftest_class(**options)
    perftest_class.get_browser_meta = original_get

    assert isinstance(perftest_instance.profile, BaseProfile)
    if app_name != "firefox":
        return

    # These prefs are set in mozprofile
    firefox_prefs = [
        'user_pref("app.update.checkInstallTime", false);',
        'user_pref("app.update.disabledForTesting", true);',
        'user_pref("'
        'security.turn_off_all_security_so_that_viruses_can_take_over_this_computer", true);',
    ]
    # This pref is set in raptor
    raptor_pref = 'user_pref("security.enable_java", false);'

    prefs_file = os.path.join(perftest_instance.profile.profile, "user.js")
    with open(prefs_file, "r") as fh:
        prefs = fh.read()
        for firefox_pref in firefox_prefs:
            assert firefox_pref in prefs
        assert raptor_pref in prefs


def test_perftest_host_ip(ConcretePerftest, options, get_prefs):
    os.environ["HOST_IP"] = "some_dummy_host_ip"
    options["host"] = "HOST_IP"

    perftest = ConcretePerftest(**options)

    assert perftest.config["host"] == os.environ["HOST_IP"]


@pytest.mark.parametrize(
    "app_name, expected_e10s_flag",
    [["firefox", True], ["geckoview", True]],
)
def test_e10s_enabling(ConcretePerftest, options, app_name, expected_e10s_flag):
    options["app"] = app_name
    perftest = ConcretePerftest(profile_class="firefox", **options)
    assert perftest.config["e10s"] == expected_e10s_flag


def test_profile_was_provided_locally(ConcretePerftest, options):
    perftest = ConcretePerftest(**options)
    assert os.path.isdir(perftest.config["local_profile_dir"])


@pytest.mark.parametrize(
    "profile_class, app, expected_profile",
    [
        ["firefox", "firefox", "firefox"],
        [None, "firefox", "firefox"],
        ["firefox", None, "firefox"],
    ],
)
def test_profile_class_assignation(
    ConcretePerftest, options, profile_class, app, expected_profile
):
    options["app"] = app
    perftest = ConcretePerftest(profile_class=profile_class, **options)
    assert perftest.profile_class == expected_profile


def test_raptor_venv(ConcretePerftest, options):
    perftest = ConcretePerftest(**options)
    assert perftest.raptor_venv.endswith("raptor-venv")


@pytest.mark.parametrize(
    "run_local,"
    "debug_mode,"
    "post_startup_delay,"
    "expected_post_startup_delay,"
    "expected_debug_mode",
    [
        [True, True, 1234, 1234, True],
        [True, True, 12345, 3000, True],
        [False, False, 1234, 1234, False],
        [False, False, 12345, 12345, False],
        [True, False, 1234, 1234, False],
        [True, False, 12345, 12345, False],
        [False, True, 1234, 1234, False],
        [False, True, 12345, 12345, False],
    ],
)
def test_post_startup_delay(
    ConcretePerftest,
    options,
    run_local,
    debug_mode,
    post_startup_delay,
    expected_post_startup_delay,
    expected_debug_mode,
):
    perftest = ConcretePerftest(
        run_local=run_local,
        debug_mode=debug_mode,
        post_startup_delay=post_startup_delay,
        **options
    )
    assert perftest.post_startup_delay == expected_post_startup_delay
    assert perftest.debug_mode == expected_debug_mode


@pytest.mark.parametrize(
    "alert, expected_alert", [["test_to_alert_on", "test_to_alert_on"], [None, None]]
)
def test_perftest_run_test_setup(
    ConcretePerftest, options, mock_test, alert, expected_alert
):
    perftest = ConcretePerftest(**options)
    mock_test["alert_on"] = alert

    perftest.run_test_setup(mock_test)

    assert perftest.config["subtest_alert_on"] == expected_alert


# Browsertime tests
def test_cmd_arguments(ConcreteBrowsertime, browsertime_options, mock_test):
    expected_cmd = {
        browsertime_options["browsertime_node"],
        browsertime_options["browsertime_browsertimejs"],
        "--firefox.geckodriverPath",
        browsertime_options["browsertime_geckodriver"],
        "--browsertime.page_cycles",
        "1",
        "--browsertime.url",
        mock_test["test_url"],
        "--browsertime.secondary_url",
        mock_test["secondary_url"],
        "--browsertime.page_cycle_delay",
        "1000",
        "--browsertime.post_startup_delay",
        str(DEFAULT_TIMEOUT),
        "--firefox.profileTemplate",
        "--skipHar",
        "--video",
        "true",
        "--visualMetrics",
        "false",
        "--timeouts.pageLoad",
        str(DEFAULT_TIMEOUT),
        "--timeouts.script",
        str(DEFAULT_TIMEOUT),
        "--resultDir",
        "--iterations",
        "1",
    }
    if browsertime_options.get("app") in ["chrome", "chrome-m"]:
        expected_cmd.add(
            "--chrome.chromedriverPath", browsertime_options["browsertime_chromedriver"]
        )

    browsertime = ConcreteBrowsertime(
        post_startup_delay=DEFAULT_TIMEOUT, **browsertime_options
    )
    browsertime.run_test_setup(mock_test)
    cmd = browsertime._compose_cmd(mock_test, DEFAULT_TIMEOUT)

    assert expected_cmd.issubset(set(cmd))


def extract_arg_value(cmd, arg):
    param_index = cmd.index(arg) + 1
    return cmd[param_index]


@pytest.mark.parametrize(
    "arg_to_test, expected, test_patch, options_patch",
    [
        ["--iterations", "1", {}, {"browser_cycles": None}],
        ["--iterations", "123", {"browser_cycles": 123}, {}],
        ["--video", "false", {}, {"browsertime_video": None}],
        ["--video", "true", {}, {"browsertime_video": "dummy_value"}],
        ["--timeouts.script", str(DEFAULT_TIMEOUT), {}, {"page_cycles": None}],
        ["--timeouts.script", str(123 * DEFAULT_TIMEOUT), {"page_cycles": 123}, {}],
        ["--browsertime.page_cycles", "1", {}, {"page_cycles": None}],
        ["--browsertime.page_cycles", "123", {"page_cycles": 123}, {}],
    ],
)
def test_browsertime_arguments(
    ConcreteBrowsertime,
    browsertime_options,
    mock_test,
    arg_to_test,
    expected,
    test_patch,
    options_patch,
):
    mock_test.update(test_patch)
    browsertime_options.update(options_patch)
    browsertime = ConcreteBrowsertime(
        post_startup_delay=DEFAULT_TIMEOUT, **browsertime_options
    )
    browsertime.run_test_setup(mock_test)
    cmd = browsertime._compose_cmd(mock_test, DEFAULT_TIMEOUT)

    param_value = extract_arg_value(cmd, arg_to_test)
    assert expected == param_value


@pytest.mark.parametrize(
    "timeout, expected_timeout, test_patch, options_patch",
    [
        [0, 80, {}, {}],
        [0, 80, {}, {"gecko_profile": False}],
        [1000, 381, {}, {"gecko_profile": True}],
    ],
)
def test_compute_process_timeout(
    ConcreteBrowsertime,
    browsertime_options,
    mock_test,
    timeout,
    expected_timeout,
    test_patch,
    options_patch,
):
    mock_test.update(test_patch)
    browsertime_options.update(options_patch)
    browsertime = ConcreteBrowsertime(
        post_startup_delay=DEFAULT_TIMEOUT, **browsertime_options
    )
    bt_timeout = browsertime._compute_process_timeout(mock_test, timeout, [])
    assert bt_timeout == expected_timeout


if __name__ == "__main__":
    mozunit.main()
