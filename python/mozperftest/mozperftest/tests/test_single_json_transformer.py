#!/usr/bin/env python
import mozunit

from mozperftest.metrics.notebook.transformer import Transformer
from mozperftest.metrics.notebook.transforms.single_json import SingleJsonRetriever


def test_transform(data):
    tfm = SingleJsonRetriever()

    expected_result = [
        {
            "data": [
                {"value": 101, "xaxis": 1},
                {"value": 102, "xaxis": 1},
                {"value": 103, "xaxis": 1},
            ],
            "subtest": "browserScripts.timings.firstPaint",
        }
    ]

    actual_result = tfm.transform(data["data_1"])

    assert actual_result == expected_result


def test_merge(data):
    tfm = SingleJsonRetriever()
    sde = tfm.transform(data["data_1"])
    sde.extend(tfm.transform(data["data_2"]))

    expected_result = [
        {
            "data": [
                {"value": 101, "xaxis": 1},
                {"value": 102, "xaxis": 1},
                {"value": 103, "xaxis": 1},
                {"value": 201, "xaxis": 2},
                {"value": 202, "xaxis": 2},
                {"value": 203, "xaxis": 2},
            ],
            "subtest": "browserScripts.timings.firstPaint",
        }
    ]

    actual_result = tfm.merge(sde)

    assert actual_result == expected_result


def test_process(files):
    files = files["resources"]
    file_1 = files["file_1"]
    file_2 = files["file_2"]

    tfm = Transformer([], SingleJsonRetriever())
    tfm.files = [file_1, file_2]

    expected_result = [
        {
            "data": [
                {"value": 101, "xaxis": 1, "file": file_1},
                {"value": 102, "xaxis": 1, "file": file_1},
                {"value": 103, "xaxis": 1, "file": file_1},
                {"value": 201, "xaxis": 2, "file": file_2},
                {"value": 202, "xaxis": 2, "file": file_2},
                {"value": 203, "xaxis": 2, "file": file_2},
            ],
            "subtest": "browserScripts.timings.firstPaint",
            "name": "group_1",
        }
    ]

    actual_result = tfm.process("group_1")

    assert actual_result == expected_result


if __name__ == "__main__":
    mozunit.main()
