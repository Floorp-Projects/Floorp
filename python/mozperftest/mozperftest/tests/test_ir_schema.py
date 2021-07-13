#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import pytest
from jsonschema.exceptions import ValidationError
from unittest import mock

from mozperftest.metrics.utils import validate_intermediate_results, metric_fields

from mozperftest.utils import silence
from mozperftest.tests.support import get_running_env, temp_file
from mozperftest.environment import METRICS


def mocked_filtered_metrics(*args, **kwargs):
    return (
        {
            "Example": [
                {
                    "data": [
                        {"file": "browsertime.json", "value": 0, "xaxis": 0},
                        {"file": "browsertime.json", "value": 0, "xaxis": 0},
                    ],
                    "name": "Example",
                    "subtest": "example-subtest",
                }
            ]
        },
        {"Example": {"binary": "example-path/firefox", "name": "Example"}},
    )


def setup_env(options):
    mach_cmd, metadata, env = get_running_env(**options)

    metrics = env.layers[METRICS]

    metadata.add_result(
        {
            "results": "path-to-results",
            "name": "browsertime",
            "binary": "example-path/firefox",
        }
    )
    return metrics, metadata, env


def test_results_with_directory():
    test_result = {
        "results": "path-to-results",
        "name": "the-name",
        "binary": "example-path/firefox",
    }
    validate_intermediate_results(test_result)


def test_results_with_measurements():
    test_result = {
        "results": [
            {"name": "metric-1", "values": [0, 1, 1, 0]},
            {"name": "metric-2", "values": [0, 1, 1, 0]},
        ],
        "name": "the-name",
        "binary": "example-path/firefox",
    }
    validate_intermediate_results(test_result)


def test_results_with_suite_perfherder_options():
    test_result = {
        "results": [
            {"name": "metric-1", "values": [0, 1, 1, 0]},
            {"name": "metric-2", "values": [0, 1, 1, 0]},
        ],
        "name": "the-name",
        "binary": "example-path/firefox",
        "extraOptions": ["an-extra-option"],
        "value": 9000,
    }
    validate_intermediate_results(test_result)


def test_results_with_subtest_perfherder_options():
    test_result = {
        "results": [
            {"name": "metric-1", "shouldAlert": True, "values": [0, 1, 1, 0]},
            {"name": "metric-2", "alertThreshold": 1.0, "values": [0, 1, 1, 0]},
        ],
        "name": "the-name",
        "binary": "example-path/firefox",
        "extraOptions": ["an-extra-option"],
        "value": 9000,
    }
    validate_intermediate_results(test_result)


def test_results_with_bad_suite_property():
    test_result = {
        "results": "path-to-results",
        "name": "the-name",
        "binary": "example-path/firefox",
        "I'll cause a failure,": "an expected failure",
    }
    with pytest.raises(ValidationError):
        validate_intermediate_results(test_result)


def test_results_with_bad_subtest_property():
    test_result = {
        "results": [
            # Error is in "shouldalert", it should be "shouldAlert"
            {"name": "metric-1", "shouldalert": True, "values": [0, 1, 1, 0]},
            {"name": "metric-2", "alertThreshold": 1.0, "values": [0, 1, 1, 0]},
        ],
        "name": "the-name",
        "binary": "example-path/firefox",
        "extraOptions": ["an-extra-option"],
        "value": 9000,
    }
    with pytest.raises(ValidationError):
        validate_intermediate_results(test_result)


def test_results_with_missing_suite_property():
    test_result = {
        # Missing "results"
        "name": "the-name",
        "binary": "example-path/firefox",
    }
    with pytest.raises(ValidationError):
        validate_intermediate_results(test_result)


def test_results_with_missing_subtest_property():
    test_result = {
        "results": [
            # Missing "values"
            {"name": "metric-2", "alertThreshold": 1.0}
        ],
        "name": "the-name",
        "binary": "example-path/firefox",
        "extraOptions": ["an-extra-option"],
        "value": 9000,
    }
    with pytest.raises(ValidationError):
        validate_intermediate_results(test_result)


def test_results_with_missing_binary_property():
    test_result = {"results": "path-to-results", "name": "the-name"}
    # This will raise an error because the required binary field is missing
    with pytest.raises(ValidationError, match="'binary' is a required property"):
        validate_intermediate_results(test_result)


@mock.patch(
    "mozperftest.metrics.perfherder.filtered_metrics", new=mocked_filtered_metrics
)
def test_perfherder_get_browser_meta_invalid_binary(*mocked):
    options = {
        "perfherder": True,
        "perfherder-stats": True,
        "perfherder-prefix": "",
        "perfherder-metrics": [metric_fields("firstPaint")],
    }

    metrics, metadata, env = setup_env(options)

    # This will raise an error because the binary path is invalid
    with pytest.raises(ValidationError, match="None is not one of"):
        with temp_file() as output:
            env.set_arg("output", output)
            with metrics as m, silence():
                m(metadata)


if __name__ == "__main__":
    mozunit.main()
