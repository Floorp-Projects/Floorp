# Any copyright is dedicated to the Public Domain.
# https://creativecommons.org/publicdomain/zero/1.0/
"""
Tests for the 'tests.py' transforms
"""

import hashlib
import json
from functools import partial
from pprint import pprint

import mozunit
import pytest

from gecko_taskgraph.transforms import test as test_transforms


@pytest.fixture
def make_test_task():
    """Create a test task definition with required default values."""

    def inner(**extra):
        task = {
            "attributes": {},
            "build-platform": "linux64",
            "mozharness": {"extra-options": []},
            "test-platform": "linux64",
            "treeherder-symbol": "g(t)",
            "try-name": "task",
        }
        task.update(extra)
        return task

    return inner


def test_split_variants(monkeypatch, run_full_config_transform, make_test_task):
    # mock out variant definitions
    monkeypatch.setattr(
        test_transforms.variant,
        "TEST_VARIANTS",
        {
            "foo": {
                "description": "foo variant",
                "suffix": "foo",
                "mozinfo": "foo",
                "component": "foo bar",
                "expiration": "never",
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
                "mozinfo": "bar",
                "component": "foo bar",
                "expiration": "never",
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

    run_split_variants = partial(
        run_full_config_transform, test_transforms.variant.split_variants
    )

    # test no variants
    input_task = make_test_task(
        **{
            "run-without-variant": True,
        }
    )
    tasks = list(run_split_variants(input_task))
    assert len(tasks) == 1
    assert tasks[0] == input_task

    # test variants are split into expected tasks
    input_task = make_test_task(
        **{
            "run-without-variant": True,
            "variants": ["foo", "bar"],
        }
    )
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
            "run-without-variant": True,
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
            "run-without-variant": True,
            # this should cause task to be filtered out of 'bar' and 'foo+bar' variants
            "test-platform": "windows",
            "variants": ["foo", "bar", "foo+bar"],
        }
    )
    tasks = list(run_split_variants(input_task))
    assert len(tasks) == 2
    assert "unittest_variant" not in tasks[0]["attributes"]
    assert tasks[1]["attributes"]["unittest_variant"] == "foo"

    # test 'run-without-variants=False'
    input_task = make_test_task(
        **{
            "run-without-variant": False,
            "variants": ["foo"],
        }
    )
    tasks = list(run_split_variants(input_task))
    assert len(tasks) == 1
    assert tasks[0]["attributes"]["unittest_variant"] == "foo"


@pytest.mark.parametrize(
    "task,expected",
    (
        pytest.param(
            {
                "attributes": {"unittest_variant": "webrender-sw+1proc"},
                "test-platform": "linux1804-64-clang-trunk-qr/opt",
            },
            {
                "platform": {
                    "arch": "64",
                    "os": {
                        "name": "linux",
                        "version": "1804",
                    },
                },
                "build": {
                    "type": "opt",
                    "clang-trunk": True,
                },
                "runtime": {
                    "1proc": True,
                    "webrender-sw": True,
                },
            },
            id="linux",
        ),
        pytest.param(
            {
                "attributes": {},
                "test-platform": "linux2204-64-wayland-shippable/opt",
            },
            {
                "platform": {
                    "arch": "64",
                    "display": "wayland",
                    "os": {
                        "name": "linux",
                        "version": "2204",
                    },
                },
                "build": {
                    "type": "opt",
                    "shippable": True,
                },
                "runtime": {},
            },
            id="linux wayland shippable",
        ),
        pytest.param(
            {
                "attributes": {},
                "test-platform": "android-hw-a51-11-0-arm7-shippable-qr/opt",
            },
            {
                "platform": {
                    "arch": "arm7",
                    "device": "a51",
                    "os": {
                        "name": "android",
                        "version": "11.0",
                    },
                },
                "build": {
                    "type": "opt",
                    "shippable": True,
                },
                "runtime": {},
            },
            id="android",
        ),
        pytest.param(
            {
                "attributes": {},
                "test-platform": "windows10-64-2004-ref-hw-2017-ccov/debug",
            },
            {
                "platform": {
                    "arch": "64",
                    "machine": "ref-hw-2017",
                    "os": {
                        "build": "2004",
                        "name": "windows",
                        "version": "10",
                    },
                },
                "build": {
                    "type": "debug",
                    "ccov": True,
                },
                "runtime": {},
            },
            id="windows",
        ),
    ),
)
def test_set_test_setting(run_transform, task, expected):
    # add hash to 'expected'
    expected["_hash"] = hashlib.sha256(
        json.dumps(expected, sort_keys=True).encode("utf-8")
    ).hexdigest()[:12]

    task = list(run_transform(test_transforms.other.set_test_setting, task))[0]
    assert "test-setting" in task
    assert task["test-setting"] == expected


def assert_spi_not_disabled(task):
    extra_options = task["mozharness"]["extra-options"]
    # The pref to enable this gets set outside of this transform, so only
    # bother asserting that the pref to disable does not exist.
    assert (
        "--setpref=media.peerconnection.mtransport_process=false" not in extra_options
    )
    assert "--setpref=network.process.enabled=false" not in extra_options


def assert_spi_disabled(task):
    extra_options = task["mozharness"]["extra-options"]
    assert "--setpref=media.peerconnection.mtransport_process=false" in extra_options
    assert "--setpref=media.peerconnection.mtransport_process=true" not in extra_options
    assert "--setpref=network.process.enabled=false" in extra_options
    assert "--setpref=network.process.enabled=true" not in extra_options


@pytest.mark.parametrize(
    "task,callback",
    (
        pytest.param(
            {"attributes": {"unittest_variant": "socketprocess"}},
            assert_spi_not_disabled,
            id="socketprocess",
        ),
        pytest.param(
            {
                "attributes": {"unittest_variant": "socketprocess_networking"},
            },
            assert_spi_not_disabled,
            id="socketprocess_networking",
        ),
        pytest.param({}, assert_spi_disabled, id="no variant"),
        pytest.param(
            {"suite": "cppunit", "attributes": {"unittest_variant": "socketprocess"}},
            assert_spi_not_disabled,
            id="excluded suite",
        ),
        pytest.param(
            {"attributes": {"unittest_variant": "no-fission+socketprocess"}},
            assert_spi_not_disabled,
            id="composite variant",
        ),
    ),
)
def test_ensure_spi_disabled_on_all_but_spi(
    make_test_task, run_transform, task, callback
):
    task.setdefault("suite", "mochitest-plain")
    task = make_test_task(**task)
    task = list(
        run_transform(test_transforms.other.ensure_spi_disabled_on_all_but_spi, task)
    )[0]
    pprint(task)
    callback(task)


if __name__ == "__main__":
    mozunit.main()
