import mozunit
import pytest
from jsonschema import ValidationError

from mozperftest.metrics.exceptions import (
    NotebookDuplicateTransformsError,
    NotebookInvalidPathError,
    NotebookInvalidTransformError,
)
from mozperftest.metrics.notebook.transformer import (
    Transformer,
    get_transformer,
    get_transformers,
)
from mozperftest.tests.data.perftestetl_plugin import (
    test_transformer_perftestetl_plugin_1,
    test_transformer_perftestetl_plugin_2,
)
from mozperftest.tests.support import HERE, get_running_env

_, metadata, _ = get_running_env()
prefix = "PerftestNotebook"


def test_init_failure():
    class TempClass(object):
        def temp_fun():
            return 1

    with pytest.raises(NotebookInvalidTransformError):
        Transformer(custom_transformer=TempClass())


def test_files_getter(files):
    files = files["resources"]
    assert files == Transformer(files, logger=metadata, prefix=prefix).files


def test_files_setter(files):
    files = files["resources"]
    files = list(files.values())
    tfm = Transformer(logger=metadata, prefix=prefix)
    tfm.files = files
    assert files == tfm.files


def test_files_setter_failure():
    tfm = Transformer(logger=metadata, prefix=prefix)
    tfm.files = "fail"
    assert not tfm.files


def test_open_data(data, files):
    tfm = Transformer(logger=metadata, prefix=prefix)

    files = files["resources"]
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


def test_jsonschema_valitate_failure(files):
    class BadTransformer:
        def transform(self, data):
            return {"bad data": "bad data"}

        def merge(self, sde):
            return {"bad data": "bad data"}

    files = files["resources"]
    file_1 = files["file_1"]
    file_2 = files["file_2"]

    tfm = Transformer([], BadTransformer(), logger=metadata, prefix=prefix)
    tfm.files = [file_1, file_2]
    with pytest.raises(ValidationError):
        tfm.process("name")


def test_get_transformer():
    path_1 = (
        HERE
        / "data"
        / "perftestetl_plugin"
        / "test_transformer_perftestetl_plugin_1.py"
    )
    assert (
        get_transformer(path_1.as_posix()).__name__
        == test_transformer_perftestetl_plugin_1.TestTransformer1.__name__
    )

    path_2 = (
        "mozperftest.tests.data.perftestetl_plugin."
        + "test_transformer_perftestetl_plugin_2:TestTransformer2"
    )
    assert (
        get_transformer(path_2).__name__
        == test_transformer_perftestetl_plugin_2.TestTransformer2.__name__
    )


def test_get_transformer_failure():
    path_1 = HERE / "data" / "does-not-exist.py"
    with pytest.raises(NotebookInvalidPathError):
        get_transformer(path_1.as_posix())

    path_2 = HERE / "data" / "does-not-exist"
    with pytest.raises(ImportError):
        get_transformer(path_2.as_posix())

    path_3 = (
        "mozperftest.tests.data.perftestetl_plugin."
        + "test_transformer_perftestetl_plugin_2:TestTransformer3"
    )
    with pytest.raises(ImportError):
        get_transformer(path_3)

    path_4 = (
        "mozperftest.tests.data.perftestetl_plugin."
        + "test_transformer_perftestetl_plugin_3:TestTransformer3"
    )
    with pytest.raises(ImportError):
        get_transformer(path_4)

    with pytest.raises(NotebookInvalidTransformError):
        get_transformer(__file__)


def test_get_transformers():
    dirpath = HERE / "data" / "perftestetl_plugin"
    tfms = get_transformers(dirpath)
    assert test_transformer_perftestetl_plugin_1.TestTransformer1.__name__ in tfms
    assert test_transformer_perftestetl_plugin_2.TestTransformer2.__name__ in tfms


def test_get_transformers_failure():
    dirpath = HERE / "data" / "does-not-exist"
    with pytest.raises(NotebookInvalidPathError):
        get_transformers(dirpath)

    dirpath = HERE / "data" / "perftestetl_plugin" / "test_transformer_1.py"
    with pytest.raises(NotebookInvalidPathError):
        get_transformers(dirpath)

    dirpath = HERE / "data" / "multiple_transforms_error"
    with pytest.raises(NotebookDuplicateTransformsError):
        get_transformers(dirpath)


if __name__ == "__main__":
    mozunit.main()
