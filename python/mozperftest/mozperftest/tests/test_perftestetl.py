import json
import pathlib

import mozunit
import pytest

from mozperftest.metrics.notebook.constant import Constant
from mozperftest.metrics.notebook.transformer import Transformer
from mozperftest.metrics.notebook.transforms.single_json import SingleJsonRetriever


def test_init(ptetls):
    for ptetl in ptetls.values():
        assert isinstance(ptetl.fmt_data, dict)
        assert isinstance(ptetl.file_groups, dict)
        assert isinstance(ptetl.config, dict)
        assert isinstance(ptetl.sort_files, bool)
        assert isinstance(ptetl.const, Constant)
        assert isinstance(ptetl.transformer, Transformer)


def test_parse_file_grouping(ptetls):
    def _check_files_created(ptetl, expected_files):
        actual_files = set(ptetl.parse_file_grouping(expected_files))
        expected_files = set(expected_files)

        # Check all parsed files are regular files.
        assert all([pathlib.Path(file).is_file for file in actual_files])
        # Check parse_file_grouping function returns correct result.
        assert actual_files - expected_files == set()

    # If file_grouping is a list of files.
    ptetl = ptetls["ptetl_list"]
    expected_files = ptetl.file_groups["group_1"]
    _check_files_created(ptetl, expected_files)

    # If file_grouping is a directory string.
    ptetl = ptetls["ptetl_str"]
    expected_path = ptetl.file_groups["group_1"]
    expected_files = [
        f.resolve().as_posix() for f in pathlib.Path(expected_path).iterdir()
    ]
    _check_files_created(ptetl, expected_files)


def test_process(ptetls, files):
    # Temporary resource files.
    files, output = files["resources"], files["output"]
    file_1 = files["file_1"]
    file_2 = files["file_2"]

    # Create expected output.
    expected_output = [
        {
            "data": [
                {"value": 101, "xaxis": 1, "file": file_1},
                {"value": 102, "xaxis": 1, "file": file_1},
                {"value": 103, "xaxis": 1, "file": file_1},
                {"value": 201, "xaxis": 2, "file": file_2},
                {"value": 202, "xaxis": 2, "file": file_2},
                {"value": 203, "xaxis": 2, "file": file_2},
            ],
            "name": "group_1",
            "subtest": "browserScripts.timings.firstPaint",
        }
    ]

    ptetl = ptetls["ptetl_str"]

    # Set a custom transformer.
    ptetl.transformer = Transformer([], SingleJsonRetriever())

    # Create expected result.
    expected_result = {
        "data": expected_output,
        "file-output": output,
    }

    # Check return value.
    actual_result = ptetl.process()
    assert actual_result == expected_result

    # Check output file.
    with pathlib.Path(output).open() as f:
        actual_output = json.load(f)

    assert expected_output == actual_output


def test_process_fail_artifact_downloading(ptetls, files):
    ptetl = ptetls["ptetl_list"]
    ptetl.file_groups = {"group-name": {"artifact_downloader_setting": False}}

    # Set a custom transformer.
    ptetl.transformer = Transformer([], SingleJsonRetriever())
    with pytest.raises(Exception) as exc_info:
        ptetl.process()

    assert (
        str(exc_info.value)
        == "Artifact downloader tooling is disabled for the time being."
    )


if __name__ == "__main__":
    mozunit.main()
