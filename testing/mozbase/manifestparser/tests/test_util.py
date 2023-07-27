#!/usr/bin/env python

"""
Test how our utility functions are working.
"""

from io import StringIO
from textwrap import dedent

import mozunit
import pytest
from manifestparser import read_ini
from manifestparser.util import evaluate_list_from_string


@pytest.fixture(scope="module")
def parse_manifest():
    def inner(string, **kwargs):
        buf = StringIO()
        buf.write(dedent(string))
        buf.seek(0)
        return read_ini(buf, **kwargs)[0]

    return inner


@pytest.mark.parametrize(
    "test_manifest, expected_list",
    [
        [
            """
            [test_felinicity.py]
            kittens = true
            cats =
                "I",
                "Am",
                "A",
                "Cat",
            """,
            ["I", "Am", "A", "Cat"],
        ],
        [
            """
            [test_felinicity.py]
            kittens = true
            cats =
                ["I", 1],
                ["Am", 2],
                ["A", 3],
                ["Cat", 4],
            """,
            [
                ["I", 1],
                ["Am", 2],
                ["A", 3],
                ["Cat", 4],
            ],
        ],
    ],
)
def test_string_to_list_conversion(test_manifest, expected_list, parse_manifest):
    parsed_tests = parse_manifest(test_manifest)
    assert evaluate_list_from_string(parsed_tests[0][1]["cats"]) == expected_list


@pytest.mark.parametrize(
    "test_manifest, failure",
    [
        [
            """
            # This will fail since the elements are not enlosed in quotes
            [test_felinicity.py]
            kittens = true
            cats =
                I,
                Am,
                A,
                Cat,
            """,
            ValueError,
        ],
        [
            """
            # This will fail since the syntax is incorrect
            [test_felinicity.py]
            kittens = true
            cats =
                ["I", 1,
                ["Am", 2,
                ["A", 3],
                ["Cat", 4],
            """,
            SyntaxError,
        ],
    ],
)
def test_string_to_list_conversion_failures(test_manifest, failure, parse_manifest):
    parsed_tests = parse_manifest(test_manifest)
    with pytest.raises(failure):
        evaluate_list_from_string(parsed_tests[0][1]["cats"])


if __name__ == "__main__":
    mozunit.main()
