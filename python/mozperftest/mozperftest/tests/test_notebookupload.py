#!/usr/bin/env python
import pytest

import mock
import mozunit
from mozperftest.environment import METRICS
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

    notebook.assert_has_calls(mock.call().post_to_iodide(["scatterplot"]))


if __name__ == "__main__":
    mozunit.main()
