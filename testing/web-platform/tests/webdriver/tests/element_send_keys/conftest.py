import pytest


@pytest.fixture
def create_file(tmpdir_factory):
    def inner(filename):
        fh = tmpdir_factory.mktemp("tmp").join(filename)
        fh.write(filename)

        return fh

    inner.__name__ = "create_file"
    return inner
