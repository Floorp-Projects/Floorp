# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function, unicode_literals

import pytest
from mozunit import main
from tryselect.selectors.auto import TRY_AUTO_PARAMETERS


pytestmark = pytest.mark.slow


@pytest.fixture(scope="module")
def tgg(create_tgg):
    params = TRY_AUTO_PARAMETERS.copy()
    params.update(
        {"head_repository": "https://hg.mozilla.org/try", "project": "try"}
    )
    tgg = create_tgg(overrides=params, target_kind="test")
    return tgg


@pytest.fixture(scope="module")
def full_task_graph(tgg):
    return tgg.full_task_graph


@pytest.fixture(scope="module")
def optimized_task_graph(full_task_graph, tgg):
    return tgg.optimized_task_graph


def test_generate_graph(optimized_task_graph):
    """Simply tests that generating the graph does not fail."""
    assert len(optimized_task_graph.tasks) > 0


if __name__ == "__main__":
    main()
