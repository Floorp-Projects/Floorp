#!/usr/bin/env python
import json
import jsonschema
import pathlib
import pytest
import mozunit

from mozperftest.metrics.exceptions import PerfherderValidDataError
from mozperftest.tests.support import (
    get_running_env,
    temp_file,
    temp_dir,
    EXAMPLE_TEST,
    BT_DATA,
    HERE,
)
from mozperftest.environment import METRICS
from mozperftest.utils import silence
from mozperftest.metrics.utils import metric_fields


def setup_env(options):
    mach_cmd, metadata, env = get_running_env(**options)
    runs = []

    def _run_process(*args, **kw):
        runs.append((args, kw))

    mach_cmd.run_process = _run_process
    metrics = env.layers[METRICS]
    env.set_arg("tests", [EXAMPLE_TEST])
    metadata.add_result({"results": str(BT_DATA), "name": "browsertime"})
    return metrics, metadata, env


def test_perfherder():
    options = {
        "perfherder": True,
        "perfherder-stats": True,
        "perfherder-prefix": "",
        "perfherder-metrics": [metric_fields("firstPaint")],
    }

    metrics, metadata, env = setup_env(options)

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m, silence():
            m(metadata)
        output_file = metadata.get_output()
        with open(output_file) as f:
            output = json.loads(f.read())

    # Check some metadata
    assert output["application"]["name"] == "firefox"
    assert output["framework"]["name"] == "browsertime"

    # Check some numbers in our data
    assert len(output["suites"]) == 1
    assert len(output["suites"][0]["subtests"]) == 10
    assert output["suites"][0]["value"] > 0

    # Check if only firstPaint metrics were obtained
    for subtest in output["suites"][0]["subtests"]:
        assert "firstPaint" in subtest["name"]


def test_perfherder_logcat():
    options = {
        "perfherder": True,
        "perfherder-prefix": "",
        "perfherder-metrics": [metric_fields("TimeToDisplayed")],
    }

    metrics, metadata, env = setup_env(options)
    metadata.clear_results()

    def processor(groups):
        """Parses the time from a displayed time string into milliseconds."""
        return (float(groups[0]) * 1000) + float(groups[1])

    re_w_group = r".*Displayed.*org\.mozilla\.fennec_aurora.*\+([\d]+)s([\d]+)ms.*"
    metadata.add_result(
        {
            "results": str(HERE / "data" / "home_activity.txt"),
            "transformer": "LogCatTimeTransformer",
            "transformer-options": {
                "first-timestamp": re_w_group,
                "processor": processor,
                "transform-subtest-name": "TimeToDisplayed",
            },
            "name": "LogCat",
        }
    )

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m:  # , silence():
            m(metadata)
        output_file = metadata.get_output()
        with open(output_file) as f:
            output = json.loads(f.read())

    # Check some metadata
    assert output["application"]["name"] == "firefox"
    assert output["framework"]["name"] == "browsertime"

    # Check some numbers in our data
    assert len(output["suites"]) == 1
    assert len(output["suites"][0]["subtests"]) == 1
    assert output["suites"][0]["value"] > 0

    # Check if only the TimeToDisplayd metric was obtained
    for subtest in output["suites"][0]["subtests"]:
        assert "TimeToDisplayed" in subtest["name"]


def test_perfherder_validation_failure():
    options = {"perfherder": True, "perfherder-prefix": ""}

    metrics, metadata, env = setup_env(options)

    # Perfherder schema has limits on min/max data values. Having
    # no metrics in the options will cause a failure because of the
    # timestamps that are picked up from browsertime.
    with pytest.raises(jsonschema.ValidationError):
        with temp_dir() as output:
            env.set_arg("output", output)
            with metrics as m, silence():
                m(metadata)


def test_perfherder_missing_data_failure():
    options = {"perfherder": True, "perfherder-prefix": ""}

    metrics, metadata, env = setup_env(options)
    metadata.clear_results()

    with temp_dir() as tmpdir:
        nodatajson = pathlib.Path(tmpdir, "baddata.json")
        with nodatajson.open("w") as f:
            json.dump({"bad data": "here"}, f)

        metadata.add_result({"results": str(nodatajson), "name": "browsertime"})

        with pytest.raises(PerfherderValidDataError):
            with temp_file() as output:
                env.set_arg("output", output)
                with metrics as m, silence():
                    m(metadata)


def test_perfherder_metrics_filtering():
    options = {
        "perfherder": True,
        "perfherder-prefix": "",
        "perfherder-metrics": [metric_fields("I shouldn't match a metric")],
    }

    metrics, metadata, env = setup_env(options)
    metadata.clear_results()

    with temp_dir() as tmpdir:
        nodatajson = pathlib.Path(tmpdir, "nodata.json")
        with nodatajson.open("w") as f:
            json.dump({}, f)

        metadata.add_result({"results": str(nodatajson), "name": "browsertime"})

        with temp_dir() as output:
            env.set_arg("output", output)
            with metrics as m, silence():
                m(metadata)

            assert not pathlib.Path(output, "perfherder-data.json").exists()


def test_perfherder_exlude_stats():
    options = {
        "perfherder": True,
        "perfherder-prefix": "",
        "perfherder-metrics": [metric_fields("firstPaint")],
    }

    metrics, metadata, env = setup_env(options)

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m, silence():
            m(metadata)
        output_file = metadata.get_output()
        with open(output_file) as f:
            output = json.loads(f.read())

    # Check some numbers in our data
    assert len(output["suites"]) == 1
    assert len(output["suites"][0]["subtests"]) == 1
    assert output["suites"][0]["value"] > 0

    # Check if only one firstPaint metric was obtained
    assert (
        "browserScripts.timings.firstPaint"
        == output["suites"][0]["subtests"][0]["name"]
    )


def test_perfherder_app_name():
    options = {
        "perfherder": True,
        "perfherder-prefix": "",
        "perfherder-app": "fenix",
        "perfherder-metrics": [metric_fields("firstPaint")],
    }

    metrics, metadata, env = setup_env(options)

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m, silence():
            m(metadata)
        output_file = metadata.get_output()
        with open(output_file) as f:
            output = json.loads(f.read())

    # Make sure that application setting is correct
    assert output["application"]["name"] == "fenix"
    assert "version" not in output["application"]


def test_perfherder_bad_app_name():
    options = {
        "perfherder": True,
        "perfherder-prefix": "",
        "perfherder-app": "this is not an app",
        "perfherder-metrics": [metric_fields("firstPaint")],
    }

    metrics, metadata, env = setup_env(options)

    # This will raise an error because the options method
    # we use in tests skips the `choices` checks.
    with pytest.raises(jsonschema.ValidationError):
        with temp_file() as output:
            env.set_arg("output", output)
            with metrics as m, silence():
                m(metadata)


if __name__ == "__main__":
    mozunit.main()
