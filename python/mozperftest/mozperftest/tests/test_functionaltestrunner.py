import sys
from unittest import mock

from mozperftest.test.functionaltestrunner import (
    FunctionalTestProcessor,
    FunctionalTestRunner,
)


def test_functionaltestrunner_pass():
    with mock.patch("moztest.resolve.TestResolver") as test_resolver_mock, mock.patch(
        "mozperftest.test.functionaltestrunner.load_class_from_path"
    ) as load_class_path_mock, mock.patch(
        "mozperftest.test.functionaltestrunner.FunctionalTestProcessor"
    ), mock.patch(
        "mozperftest.test.functionaltestrunner.mozlog"
    ):
        test_mock = mock.MagicMock()
        test_mock.test.return_value = 0
        load_class_path_mock.return_value = test_mock

        mach_cmd = mock.MagicMock()
        test_resolver_mock.resolve_metadata.return_value = (1, 1)
        mach_cmd._spawn.return_value = test_resolver_mock

        status, _ = FunctionalTestRunner.test(mach_cmd, [], [])

    assert status == 0


def test_functionaltestrunner_missing_test_failure():
    with mock.patch("moztest.resolve.TestResolver") as test_resolver_mock:
        mach_cmd = mock.MagicMock()
        test_resolver_mock.resolve_metadata.return_value = (0, 0)
        mach_cmd._spawn.return_value = test_resolver_mock
        status, _ = FunctionalTestRunner.test(mach_cmd, [], [])
    assert status == 1


def test_functionaltestrunner_perfmetrics_parsing():
    formatter_mock = mock.MagicMock()
    formatter_mock.return_value = "perfMetrics | fake-data"

    log_processor = FunctionalTestProcessor(stream=sys.stdout, formatter=formatter_mock)
    log_processor("")

    assert len(log_processor.match) == 1


def test_functionaltestrunner_perfmetrics_missing():
    formatter_mock = mock.MagicMock()
    formatter_mock.return_value = "perfmetrics | fake-data"

    log_processor = FunctionalTestProcessor(stream=sys.stdout, formatter=formatter_mock)
    log_processor("")

    assert len(log_processor.match) == 0
