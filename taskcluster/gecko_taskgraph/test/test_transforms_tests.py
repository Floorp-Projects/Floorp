# Any copyright is dedicated to the Public Domain.
# https://creativecommons.org/publicdomain/zero/1.0/
"""
Tests for the 'tests.py' transforms
"""

from functools import partial

import mozunit
import pytest

from gecko_taskgraph.transforms import tests as test_transforms


@pytest.fixture
def make_test_task():
    """Create a test task definition with required default values."""

    def inner(**extra):
        task = {
            "attributes": {},
            "build-platform": "linux64",
            "mozharness": {},
            "test-platform": "linux64",
            "treeherder-symbol": "g(t)",
            "try-name": "task",
        }
        task.update(extra)
        return task

    return inner


def test_split_variants(monkeypatch, run_transform, make_test_task):
    # mock out variant definitions
    monkeypatch.setattr(
        test_transforms,
        "TEST_VARIANTS",
        {
            "foo": {
                "description": "foo variant",
                "suffix": "foo",
                "merge": {
                    "mozharness": {
                        "extra-options": [
                            "--setpref=foo=1",
                        ],
                    },
                },
            },
            "bar": {
                "description": "bar variant",
                "suffix": "bar",
                "when": {
                    "$eval": "task['test-platform'][:5] == 'linux'",
                },
                "merge": {
                    "mozharness": {
                        "extra-options": [
                            "--setpref=bar=1",
                        ],
                    },
                },
                "replace": {"tier": 2},
            },
        },
    )

    def make_expected(variant):
        """Helper to generate expected tasks."""
        return make_test_task(
            **{
                "attributes": {"unittest_variant": variant},
                "description": f"{variant} variant",
                "mozharness": {
                    "extra-options": [f"--setpref={variant}=1"],
                },
                "treeherder-symbol": f"g-{variant}(t)",
                "variant-suffix": f"-{variant}",
            }
        )

    run_split_variants = partial(run_transform, test_transforms.split_variants)

    # test no variants
    input_task = make_test_task()
    tasks = list(run_split_variants(input_task))
    assert len(tasks) == 1
    assert tasks[0] == input_task

    # test variants are split into expected tasks
    input_task["variants"] = ["foo", "bar"]
    tasks = list(run_split_variants(input_task))
    assert len(tasks) == 3
    assert tasks[0] == make_test_task()
    assert tasks[1] == make_expected("foo")

    expected = make_expected("bar")
    expected["tier"] = 2
    assert tasks[2] == expected

    # test composite variants
    input_task = make_test_task(
        **{
            "variants": ["foo+bar"],
        }
    )
    tasks = list(run_split_variants(input_task))
    assert len(tasks) == 2
    assert tasks[1]["attributes"]["unittest_variant"] == "foo+bar"
    assert tasks[1]["mozharness"]["extra-options"] == [
        "--setpref=foo=1",
        "--setpref=bar=1",
    ]
    assert tasks[1]["treeherder-symbol"] == "g-foo-bar(t)"

    # test 'when' filter
    input_task = make_test_task(
        **{
            "variants": ["foo", "bar", "foo+bar"],
            # this should cause task to be filtered out of 'bar' and 'foo+bar' variants
            "test-platform": "windows",
        }
    )
    tasks = list(run_split_variants(input_task))
    assert len(tasks) == 2
    assert "unittest_variant" not in tasks[0]["attributes"]
    assert tasks[1]["attributes"]["unittest_variant"] == "foo"


if __name__ == "__main__":
    mozunit.main()
