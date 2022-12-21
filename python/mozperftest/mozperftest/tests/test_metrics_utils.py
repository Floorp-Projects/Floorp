#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json

import mozunit
import pytest

from mozperftest.metrics.utils import metric_fields, open_file
from mozperftest.tests.support import temp_file


def test_open_file():
    data = json.dumps({"1": 2})

    with temp_file(name="data.json", content=data) as f:
        res = open_file(f)
        assert res == {"1": 2}

    with temp_file(name="data.txt", content="yeah") as f:
        assert open_file(f) == "yeah"


def test_metric_fields_old_format():
    assert metric_fields("firstPaint") == {"name": "firstPaint"}


@pytest.mark.parametrize(
    "metrics, expected",
    [
        [
            "name:foo,extraOptions:bar",
            {"name": "foo", "extraOptions": "bar"},
        ],
        ["name:foo", {"name": "foo"}],
    ],
)
def test_metric_fields_simple(metrics, expected):
    assert metric_fields(metrics) == expected


@pytest.mark.parametrize(
    "metrics, expected",
    [
        [
            "name:foo,extraOptions:['1', '2', '3', 2]",
            {"name": "foo", "extraOptions": ["1", "2", "3", 2]},
        ],
        [
            """name:foo,extraOptions:['1', '2', '3', 2, "3", "hello,world"] """,
            {"name": "foo", "extraOptions": ["1", "2", "3", 2, "3", "hello,world"]},
        ],
        [
            """name:foo,extraOptions:['1', '2', '3', 2, "3", "hello,world"],"""
            """alertThreshold:['1',2,"hello"] """,
            {
                "name": "foo",
                "extraOptions": ["1", "2", "3", 2, "3", "hello,world"],
                "alertThreshold": ["1", 2, "hello"],
            },
        ],
        [
            """name:foo,extraOptions:['1', '2', '3', 2, "3", "hello,world"],"""
            """value:foo,alertThreshold:['1',2,"hello"],framework:99 """,
            {
                "name": "foo",
                "extraOptions": ["1", "2", "3", 2, "3", "hello,world"],
                "alertThreshold": ["1", 2, "hello"],
                "value": "foo",
                "framework": 99,
            },
        ],
    ],
)
def test_metric_fields_complex(metrics, expected):
    assert metric_fields(metrics) == expected


@pytest.mark.parametrize(
    "metrics",
    [
        """name:foo,extraOptions:['1', '2', '3', 2, "3", "hello,world"],"""
        """value:foo,alertThreshold:['1',2,"hello"],framework:99,"""
        """shouldAlert:[99,100,["hello", "world"],0] """,
        """name:foo,extraOptions:['1', '2', '3', 2, "3", "hello,world"],"""
        """value:foo,alertThreshold:['1',2,"hello"],framework:99,"""
        """shouldAlert:[99,100,["hello:", "world:"],0] """,
    ],
)
def test_metric_fields_complex_failures(metrics):
    with pytest.raises(Exception):
        metric_fields(metrics)


if __name__ == "__main__":
    mozunit.main()
