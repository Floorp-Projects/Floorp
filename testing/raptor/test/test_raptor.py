from __future__ import absolute_import, unicode_literals

import os
import sys
import threading
import time
import traceback
import mock
import pytest
from mock import Mock
from six import reraise

import mozunit
from mozprofile import BaseProfile
from mozrunner.errors import RunnerNotStartedError

# need this so the raptor unit tests can find output & filter classes
here = os.path.abspath(os.path.dirname(__file__))
raptor_dir = os.path.join(os.path.dirname(here), "raptor")
sys.path.insert(0, raptor_dir)


from browsertime import BrowsertimeDesktop, BrowsertimeAndroid
from webextension import (
    WebExtensionFirefox,
    WebExtensionDesktopChrome,
    WebExtensionAndroid,
)


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
        [WebExtensionFirefox, "firefox"],
        [WebExtensionDesktopChrome, "chrome"],
        [WebExtensionDesktopChrome, "chromium"],
        [WebExtensionAndroid, "fennec"],
        [WebExtensionAndroid, "geckoview"],
        [BrowsertimeDesktop, "firefox"],
        [BrowsertimeDesktop, "chrome"],
        [BrowsertimeDesktop, "chromium"],
        [BrowsertimeAndroid, "fennec"],
        [BrowsertimeAndroid, "geckoview"],
    ],
)
def test_build_profile(options, perftest_class, app_name, get_prefs):
    options["app"] = app_name
    perftest_instance = perftest_class(**options)
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
    [["firefox", True], ["fennec", False], ["geckoview", True]],
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
        ["firefox", "fennec", "firefox"],
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


# WebExtension tests
@pytest.mark.parametrize(
    "app", ["firefox", pytest.mark.xfail("chrome"), pytest.mark.xfail("chromium")]
)
def test_start_browser(get_binary, app):
    binary = get_binary(app)
    assert binary

    raptor = WebExtensionFirefox(app, binary, post_startup_delay=0)

    tests = [{"name": "raptor-{}-tp6".format(app), "page_timeout": 1000}]
    test_names = [test["name"] for test in tests]

    thread = TestBrowserThread(raptor, tests, test_names)
    thread.start()

    timeout = time.time() + 5  # seconds
    while time.time() < timeout:
        try:
            is_running = raptor.runner.is_running()
            assert is_running
            break
        except RunnerNotStartedError:
            time.sleep(0.1)
    else:
        # browser didn't start
        # if the thread had an error, display it here
        thread.print_error()
        assert False

    raptor.clean_up()
    thread.join(5)

    if thread.exc is not None:
        exc, value, tb = thread.exc
        reraise(exc, value, tb)

    assert not raptor.runner.is_running()
    assert raptor.runner.returncode is not None


# Browsertime tests
def test_cmd_arguments(ConcreteBrowsertime, browsertime_options, mock_test):
    expected_cmd = {
        browsertime_options["browsertime_node"],
        browsertime_options["browsertime_browsertimejs"],
        "--firefox.geckodriverPath", browsertime_options["browsertime_geckodriver"],
        "--browsertime.page_cycles", "1",
        "--browsertime.url", mock_test["test_url"],
        "--browsertime.page_cycle_delay", "1000",
        "--browsertime.post_startup_delay", str(DEFAULT_TIMEOUT),
        "--firefox.profileTemplate",
        "--skipHar",
        "--video", "true",
        "--visualMetrics", "false",
        "--timeouts.pageLoad", str(DEFAULT_TIMEOUT),
        "--timeouts.script", str(DEFAULT_TIMEOUT),
        "--resultDir",
        "-n", "1",
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
        ["-n", "1", {}, {"browser_cycles": None}],
        ["-n", "123", {"browser_cycles": 123}, {}],
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
        [0, 20, {}, {}],
        [0, 20, {}, {"gecko_profile": False}],
        [1000, 321, {}, {"gecko_profile": True}],
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
    bt_timeout = browsertime._compute_process_timeout(mock_test, timeout)
    assert bt_timeout == expected_timeout


@pytest.mark.parametrize(
    "host, playback, benchmark",
    [["127.0.0.1", True, False], ["localhost", False, True]],
)
def test_android_reverse_ports(host, playback, benchmark):
    raptor = WebExtensionAndroid(
        "geckoview", "org.mozilla.geckoview_example", host=host
    )
    if benchmark:
        benchmark_mock = mock.patch("raptor.raptor.benchmark.Benchmark")
        raptor.benchmark = benchmark_mock
        raptor.benchmark.port = 1234

    if playback:
        playback_mock = mock.patch(
            "mozbase.mozproxy.mozproxy.backends.mitm.mitm.MitmproxyAndroid"
        )
        playback_mock.port = 4321
        raptor.playback = playback_mock

    raptor.set_reverse_port = Mock()
    raptor.set_reverse_ports()

    raptor.set_reverse_port.assert_any_call(raptor.control_server.port)
    if benchmark:
        raptor.set_reverse_port.assert_any_call(1234)

    if playback:
        raptor.set_reverse_port.assert_any_call(4321)


def test_android_reverse_ports_non_local_host():
    raptor = WebExtensionAndroid(
        "geckoview", "org.mozilla.geckoview_example", host="192.168.100.10"
    )

    raptor.set_reverse_port = Mock()
    raptor.set_reverse_ports()

    raptor.set_reverse_port.assert_not_called()


if __name__ == "__main__":
    mozunit.main()
