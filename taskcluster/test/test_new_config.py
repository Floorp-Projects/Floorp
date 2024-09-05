# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import pytest
from mozunit import main
from tryselect.selectors.auto import TRY_AUTO_PARAMETERS

pytestmark = pytest.mark.slow

PARAMS = TRY_AUTO_PARAMETERS.copy()
PARAMS.update(
    {
        "head_repository": "https://hg.mozilla.org/try",
        "project": "try",
        "target_kind": "test",
        # These ensure this isn't considered a backstop. The pushdate must
        # be slightly higher than the one in data/pushes.json, and
        # pushlog_id must not be a multiple of 10.
        "pushdate": 1593029536,
        "pushlog_id": "2",
    }
)

PARAMS_NEW_CONFIG = TRY_AUTO_PARAMETERS.copy()
PARAMS_NEW_CONFIG.update(
    {
        "head_repository": "https://hg.mozilla.org/try",
        "project": "try",
        "target_kind": "test",
        # These ensure this isn't considered a backstop. The pushdate must
        # be slightly higher than the one in data/pushes.json, and
        # pushlog_id must not be a multiple of 10.
        "pushdate": 1593029536,
        "pushlog_id": "2",
        "try_task_config": {"new-test-config": True},
        "try_mode": "try_task_config",
        "target_tasks_method": "try_tasks",
        "test_manifest_loader": "default",
    }
)


@pytest.mark.parametrize(
    "func,min_expected",
    (
        pytest.param(
            lambda t: (
                t.kind == "test"
                and t.attributes["unittest_suite"] == "mochitest-browser-chrome"
                and t.attributes["test_platform"] == "linux1804-64-qr/opt"
                and ("spi-nw" not in t.label and "a11y-checks" not in t.label)
            ),
            32,
            id="mochitest-browser-chrome",
        ),
    ),
)
def test_tasks_new_config_false(full_task_graph, filter_tasks, func, min_expected):
    """Ensure when using new-test-config that we have -cf tasks and they are half the total tasks."""
    tasks = [t.label for t in filter_tasks(full_task_graph, func)]
    assert len(tasks) == min_expected

    cf_tasks = [
        t.label for t in filter_tasks(full_task_graph, func) if t.label.endswith("-cf")
    ]
    assert len(cf_tasks) == min_expected / 2


@pytest.mark.parametrize(
    "func,min_expected",
    (
        pytest.param(
            lambda t: (
                t.kind == "test"
                and t.attributes["unittest_suite"] == "mochitest-browser-chrome"
                and t.attributes["test_platform"] == "linux1804-64-qr/opt"
                and ("spi-nw" not in t.label and "a11y-checks" not in t.label)
            ),
            32,
            id="mochitest-browser-chrome",
        ),
    ),
)
def test_tasks_new_config_true(
    full_task_graph_new_config, filter_tasks, func, min_expected
):
    """Ensure when using new-test-config that no -cf tasks are scheduled and we have 2x the default and NO -cf."""
    tasks = [t.label for t in filter_tasks(full_task_graph_new_config, func)]
    assert len(tasks) == min_expected

    cf_tasks = [
        t.label
        for t in filter_tasks(full_task_graph_new_config, func)
        if t.label.endswith("-cf")
    ]
    assert len(cf_tasks) == 0


if __name__ == "__main__":
    main()
