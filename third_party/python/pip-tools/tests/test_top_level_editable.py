import os
import pytest

from piptools.repositories import PyPIRepository
from piptools.scripts.compile import get_pip_command


class MockedPyPIRepository(PyPIRepository):
    def get_dependencies(self, ireq):
        # "mock" everything but editable reqs to avoid disk and network I/O
        # when possible
        if not ireq.editable:
            return set()

        return super(MockedPyPIRepository, self).get_dependencies(ireq)


def _get_repository():
    pip_command = get_pip_command()
    pip_args = []
    pip_options, _ = pip_command.parse_args(pip_args)
    session = pip_command._build_session(pip_options)
    repository = MockedPyPIRepository(pip_options, session)
    return repository


@pytest.mark.parametrize(
    ('input', 'expected'),

    ((tup) for tup in [
        ([os.path.join(os.path.dirname(__file__), 'test_data', 'small_fake_package')],
         ['six']),
    ])
)
def test_editable_top_level_deps_preserved(base_resolver, repository, from_editable, input, expected):
    input = [from_editable(line) for line in input]
    repository = _get_repository()
    output = base_resolver(input, prereleases=False, repository=repository).resolve()

    output = {p.name for p in output}

    # sanity check that we're expecting something
    assert output != set()

    for package_name in expected:
        assert package_name in output
