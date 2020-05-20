import mozunit
import pytest
from mozperftest.metrics.notebook.transformer import Transformer
from mozperftest.metrics.exceptions import NotebookInvalidTransformError


def test_init_failure(files):
    class TempClass(object):
        def temp_fun():
            return 1

    with pytest.raises(NotebookInvalidTransformError):
        Transformer(custom_transformer=TempClass())


def test_files_getter(files):
    files, _, _ = files
    assert files == Transformer(files).files


def test_files_setter(files):
    files, _, _ = files
    files = list(files.values())
    tfm = Transformer()
    tfm.files = files
    assert files == tfm.files


def test_files_setter_failure():
    tfm = Transformer()
    tfm.files = "fail"
    assert not tfm.files


def test_open_data(data, files):
    tfm = Transformer()

    files, _, _ = files
    json_1 = files["file_1"]
    json_2 = files["file_2"]
    txt_3 = files["file_3"]

    # If a json file is open.
    assert data["data_1"] == tfm.open_data(json_1)
    assert data["data_2"] == tfm.open_data(json_2)
    # If an other type file is open.
    assert [str(data["data_3"])] == tfm.open_data(txt_3)

    # Test failure
    with pytest.raises(Exception):
        tfm.open_data("fail")


if __name__ == "__main__":
    mozunit.main()
