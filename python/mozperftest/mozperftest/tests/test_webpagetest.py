import json
from mozperftest.tests.support import (
    get_running_env,
    EXAMPLE_WPT_TEST,
)
import requests
from unittest import mock
import random
import mozperftest.test.webpagetest as webpagetest
import pytest
from mozperftest.test.webpagetest import (
    WPTTimeOutError,
    WPTBrowserSelectionError,
    WPTInvalidURLError,
    WPTLocationSelectionError,
    WPTInvalidConnectionSelection,
    ACCEPTED_STATISTICS,
    WPTInvalidStatisticsError,
    WPTDataProcessingError,
)

WPT_METRICS = [
    "firstContentfulPaint",
    "timeToContentfulPaint",
    "visualComplete90",
    "firstPaint",
    "visualComplete99",
    "visualComplete",
    "SpeedIndex",
    "bytesIn",
    "bytesOut",
    "TTFB",
    "fullyLoadedCPUms",
    "fullyLoadedCPUpct",
    "domElements",
    "domContentLoadedEventStart",
    "domContentLoadedEventEnd",
    "loadEventStart",
    "loadEventEnd",
]


class WPTTests:
    def __init__(self, log):
        self.log = log

    def runTests(self, args):
        return True


def running_env(**kw):
    return get_running_env(flavor="webpagetest", **kw)


def init_placeholder_wpt_data(fvonly=False, invalid_results=False):
    views = {"firstView": {}}
    if not fvonly:
        views["repeatView"] = {}
    placeholder_data = {
        "data": {
            "summary": "websitelink.com",
            "location": "ec2-us-east-1:Firefox",
            "testRuns": 3,
            "successfulFVRuns": 3,
            "successfulRVRuns": 3,
            "fvonly": fvonly,
            "average": views,
            "standardDeviation": views,
            "median": views,
            "runs": {"1": {"firstView": {"browserVersion": 101.1}}},
            "url": "testurl.ca",
        },
        "webPagetestVersion": 21.0,
    }
    exclude_metrics = 0 if not invalid_results else 2
    for metric in WPT_METRICS[exclude_metrics:]:
        for view in views:
            for stat in ACCEPTED_STATISTICS:
                placeholder_data["data"][stat][view][metric] = random.randint(0, 10000)
                placeholder_data["data"][stat][view][metric] = random.randint(0, 10000)
                placeholder_data["data"][stat][view][metric] = random.randint(0, 10000)
    return placeholder_data


def init_mocked_request(status_code, **kwargs):
    mock_data = {
        "data": {
            "ec2-us-east-1": {"PendingTests": {"Queued": 3}, "Label": "California"},
            "jsonUrl": "mock_test.com",
            "summary": "Just a pageload test",
            "url": "testurl.ca",
        },
        "statusCode": status_code,
    }
    for key, value in kwargs.items():
        mock_data["data"][key] = value
    mock_request = requests.Response()
    mock_request._content = json.dumps(mock_data).encode("utf-8")
    return mock_request


@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.get_WPT_results",
    return_value=init_placeholder_wpt_data(),
)
@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_no_issues_mocked_results(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    test = webpagetest.WebPageTest(env, mach_cmd)
    metadata.script["options"]["test_parameters"]["wait_between_requests"] = 1
    metadata.script["options"]["test_parameters"]["first_view_only"] = 0
    metadata.script["options"]["test_parameters"]["test_list"] = ["google.ca"]
    test.run(metadata)


@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_test_invalid_browser(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    metadata.script["options"]["test_parameters"]["browser"] = "Invalid Browser"
    test = webpagetest.WebPageTest(env, mach_cmd)
    with pytest.raises(WPTBrowserSelectionError):
        test.run(metadata)


@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_test_invalid_connection(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    test = webpagetest.WebPageTest(env, mach_cmd)
    metadata.script["options"]["test_parameters"]["connection"] = "Invalid Connection"
    with pytest.raises(WPTInvalidConnectionSelection):
        test.run(metadata)


@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_test_invalid_url(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    test = webpagetest.WebPageTest(env, mach_cmd)
    metadata.script["options"]["test_list"] = ["InvalidUrl"]
    with pytest.raises(WPTInvalidURLError):
        test.run(metadata)


@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_test_invalid_statistic(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    test = webpagetest.WebPageTest(env, mach_cmd)
    metadata.script["options"]["test_parameters"]["statistics"] = ["Invalid Statistic"]
    with pytest.raises(WPTInvalidStatisticsError):
        test.run(metadata)
    assert True


@mock.patch("requests.get", return_value=init_mocked_request(200))
@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.request_with_timeout",
    return_value={"data": {}},
)
def test_webpagetest_test_invalid_location(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    test = webpagetest.WebPageTest(env, mach_cmd)
    metadata.script["options"]["test_parameters"]["location"] = "Invalid Location"
    with pytest.raises(WPTLocationSelectionError):
        test.run(metadata)


@mock.patch("requests.get", return_value=init_mocked_request(400))
@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_test_timeout(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    test = webpagetest.WebPageTest(env, mach_cmd)
    metadata.script["options"]["test_parameters"]["timeout_limit"] = 2
    metadata.script["options"]["test_parameters"]["wait_between_requests"] = 1
    with pytest.raises(WPTTimeOutError):
        test.run(metadata)
    assert True


@mock.patch(
    "requests.get",
    return_value=init_mocked_request(
        200, testRuns=3, successfulFVRuns=3, fvonly=True, location="BadLocation"
    ),
)
@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_test_wrong_browserlocation(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    metadata.script["options"]["test_list"] = ["google.ca"]
    metadata.script["options"]["test_parameters"]["wait_between_requests"] = 1
    test = webpagetest.WebPageTest(env, mach_cmd)
    with pytest.raises(WPTBrowserSelectionError):
        test.run(metadata)


@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.get_WPT_results",
    return_value=init_placeholder_wpt_data(invalid_results=True),
)
@mock.patch("mozperftest.utils.get_tc_secret", return_value={"wpt_key": "fake_key"})
@mock.patch(
    "mozperftest.test.webpagetest.WebPageTest.location_queue", return_value=None
)
def test_webpagetest_test_metric_not_found(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_WPT_TEST)])
    metadata.script["options"]["test_list"] = ["google.ca"]
    metadata.script["options"]["test_parameters"]["wait_between_requests"] = 1
    test = webpagetest.WebPageTest(env, mach_cmd)
    with pytest.raises(WPTDataProcessingError):
        test.run(metadata)
