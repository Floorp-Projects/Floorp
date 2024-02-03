import mozunit
import pytest
from tryselect import push


@pytest.mark.parametrize(
    "method,labels,params,routes,expected",
    (
        pytest.param(
            "fuzzy",
            ["task-foo", "task-bar"],
            None,
            None,
            {
                "parameters": {
                    "optimize_target_tasks": False,
                    "try_task_config": {
                        "env": {"TRY_SELECTOR": "fuzzy"},
                        "tasks": ["task-bar", "task-foo"],
                    },
                },
                "version": 2,
            },
            id="basic",
        ),
        pytest.param(
            "fuzzy",
            ["task-foo"],
            {"existing_tasks": {"task-foo": "123", "task-bar": "abc"}},
            None,
            {
                "parameters": {
                    "existing_tasks": {"task-bar": "abc"},
                    "optimize_target_tasks": False,
                    "try_task_config": {
                        "env": {"TRY_SELECTOR": "fuzzy"},
                        "tasks": ["task-foo"],
                    },
                },
                "version": 2,
            },
            id="existing_tasks",
        ),
    ),
)
def test_generate_try_task_config(method, labels, params, routes, expected):
    assert (
        push.generate_try_task_config(method, labels, params=params, routes=routes)
        == expected
    )


if __name__ == "__main__":
    mozunit.main()
