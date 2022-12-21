#!/usr/bin/env python
import mozunit
import pytest

from mozperftest.metrics.exceptions import (
    NotebookTransformError,
    NotebookTransformOptionsError,
)
from mozperftest.metrics.notebook.transforms.logcattime import LogCatTimeTransformer
from mozperftest.tests.support import HERE


@pytest.fixture(scope="session", autouse=True)
def tfm():
    yield LogCatTimeTransformer()


@pytest.fixture(scope="session", autouse=True)
def logcat_data(tfm):
    data = tfm.open_data(str(HERE / "data" / "home_activity.txt"))
    assert data
    yield data


def test_logcat_transform_two_regex(tfm, logcat_data):
    restart = r".*Activity.*Manager.*START.*org\.mozilla\.fennec_aurora/org\.mozilla\.fenix\.HomeActivity.*"  # noqa
    reend = r".*Displayed.*org\.mozilla\.fennec_aurora.*"
    opts = {
        "first-timestamp": restart,
        "second-timestamp": reend,
        "transform-subtest-name": "HANOOBish",
    }

    actual_result = tfm.transform(logcat_data, **opts)
    expected_result = [
        {
            "data": [
                {"value": 1782.0, "xaxis": 0},
                {"value": 1375.0, "xaxis": 1},
                {"value": 1497.0, "xaxis": 2},
            ],
            "subtest": "HANOOBish",
        }
    ]
    assert actual_result == expected_result

    # We should get the same results back from merge
    # since we are working with only one file
    merged = tfm.merge(actual_result)
    assert merged == expected_result


def test_logcat_transform_one_regex(tfm, logcat_data):
    def processor(groups):
        """Parses the time from a displayed time string into milliseconds."""
        return (float(groups[0]) * 1000) + float(groups[1])

    re_w_group = r".*Displayed.*org\.mozilla\.fennec_aurora.*\+([\d]+)s([\d]+)ms.*"
    opts = {
        "first-timestamp": re_w_group,
        "processor": processor,
        "transform-subtest-name": "TimeToDisplayed",
    }

    actual_result = tfm.transform(logcat_data, **opts)
    expected_result = [
        {
            "data": [
                {"value": 1743.0, "xaxis": 0},
                {"value": 1325.0, "xaxis": 1},
                {"value": 1462.0, "xaxis": 2},
            ],
            "subtest": "TimeToDisplayed",
        }
    ]
    assert actual_result == expected_result


def test_logcat_transform_no_processor(tfm, logcat_data):
    re_w_group = r".*Displayed.*org\.mozilla\.fennec_aurora.*\+([\d]+)s([\d]+)ms.*"
    opts = {
        "first-timestamp": re_w_group,
        "transform-subtest-name": "TimeToDisplayed",
    }

    actual_result = tfm.transform(logcat_data, **opts)
    expected_result = [
        {
            "data": [
                {"value": 1.0, "xaxis": 0},
                {"value": 1.0, "xaxis": 1},
                {"value": 1.0, "xaxis": 2},
            ],
            "subtest": "TimeToDisplayed",
        }
    ]
    assert actual_result == expected_result


def test_logcat_transform_no_groups(tfm, logcat_data):
    re_w_group = r".*Displayed.*org\.mozilla\.fennec_aurora.*"
    opts = {
        "first-timestamp": re_w_group,
        "transform-subtest-name": "TimeToDisplayed",
    }

    with pytest.raises(NotebookTransformOptionsError):
        tfm.transform(logcat_data, **opts)


def test_logcat_transform_too_many_groups(tfm, logcat_data):
    restart = r".*Activity.*Manager.*START.*org\.mozilla\.fennec_aurora/org\.mozilla\.fenix\.HomeActivity.*"  # noqa
    reend = r".*Displayed.*org\.mozilla\.fennec_aurora.*\+([\d]+)s([\d]+)ms.*"
    opts = {
        "first-timestamp": restart,
        "second-timestamp": reend,
        "transform-subtest-name": "HANOOBish",
    }

    with pytest.raises(NotebookTransformError):
        tfm.transform(logcat_data, **opts)


if __name__ == "__main__":
    mozunit.main()
