#!/usr/bin/env python
from unittest import mock

import mozunit
import pytest

from mozperftest.environment import METRICS
from mozperftest.metrics.utils import metric_fields
from mozperftest.tests.support import BT_DATA, EXAMPLE_TEST, get_running_env, temp_file
from mozperftest.utils import silence


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


@pytest.mark.parametrize("no_filter", [True, False])
@mock.patch("mozperftest.metrics.notebookupload.PerftestNotebook")
@mock.patch("mozperftest.test.BrowsertimeRunner", new=mock.MagicMock())
def test_notebookupload_with_filter(notebook, no_filter):
    options = {
        "notebook-metrics": [],
        "notebook-prefix": "",
        "notebook": True,
        "notebook-analysis": ["scatterplot"],
        "notebook-analyze-strings": no_filter,
    }

    metrics, metadata, env = setup_env(options)

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m, silence():
            m(metadata)

    if no_filter:
        args, kwargs = notebook.call_args_list[0]
        assert type(kwargs["data"][0]["data"][0]["value"]) == str
    else:
        for call in notebook.call_args_list:
            args, kwargs = call
            for a in args:
                for data_dict in a:
                    for data in data_dict["data"]:
                        assert type(data["value"]) in (int, float)

    notebook.assert_has_calls(
        [mock.call().post_to_iodide(["scatterplot"], start_local_server=True)]
    )


@pytest.mark.parametrize("stats", [False, True])
@mock.patch("mozperftest.metrics.notebookupload.PerftestNotebook")
@mock.patch("mozperftest.test.BrowsertimeRunner", new=mock.MagicMock())
def test_compare_to_success(notebook, stats):
    options = {
        "notebook-metrics": [metric_fields("firstPaint")],
        "notebook-prefix": "",
        "notebook-analysis": [],
        "notebook": True,
        "notebook-compare-to": [str(BT_DATA.parent)],
        "notebook-stats": stats,
    }

    metrics, metadata, env = setup_env(options)

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m, silence():
            m(metadata)

    args, kwargs = notebook.call_args_list[0]

    if not stats:
        assert len(kwargs["data"]) == 2
        assert kwargs["data"][0]["name"] == "browsertime- newest run"
        assert kwargs["data"][1]["name"] == "browsertime-results"
    else:
        assert any("statistics" in element["subtest"] for element in kwargs["data"])

    notebook.assert_has_calls(
        [mock.call().post_to_iodide(["compare"], start_local_server=True)]
    )


@pytest.mark.parametrize("filepath", ["invalidPath", str(BT_DATA)])
@mock.patch("mozperftest.metrics.notebookupload.PerftestNotebook")
@mock.patch("mozperftest.test.BrowsertimeRunner", new=mock.MagicMock())
def test_compare_to_invalid_parameter(notebook, filepath):
    options = {
        "notebook-metrics": [metric_fields("firstPaint")],
        "notebook-prefix": "",
        "notebook-analysis": [],
        "notebook": True,
        "notebook-compare-to": [filepath],
    }

    metrics, metadata, env = setup_env(options)

    with pytest.raises(Exception) as einfo:
        with temp_file() as output:
            env.set_arg("output", output)
            with metrics as m, silence():
                m(metadata)

    if filepath == "invalidPath":
        assert "does not exist" in str(einfo.value)
    else:
        assert "not a directory" in str(einfo.value)


if __name__ == "__main__":
    mozunit.main()
