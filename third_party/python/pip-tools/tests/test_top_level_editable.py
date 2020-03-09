import os

import pytest

from .constants import PACKAGES_PATH

from piptools.repositories import PyPIRepository


class MockedPyPIRepository(PyPIRepository):
    def get_dependencies(self, ireq):
        # "mock" everything but editable reqs to avoid disk and network I/O
        # when possible
        if not ireq.editable:
            return set()

        return super(MockedPyPIRepository, self).get_dependencies(ireq)


@pytest.fixture
def mocked_repository(tmpdir):
    return MockedPyPIRepository(["--no-index"], cache_dir=str(tmpdir / "pypi-repo"))


def test_editable_top_level_deps_preserved(
    base_resolver, mocked_repository, from_editable
):
    package_path = os.path.join(PACKAGES_PATH, "small_fake_with_deps")
    ireqs = [from_editable(package_path)]
    output = base_resolver(
        ireqs, prereleases=False, repository=mocked_repository
    ).resolve()

    output = {p.name for p in output}

    # sanity check that we're expecting something
    assert output != set()
    assert "small-fake-a" in output
