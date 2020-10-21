# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest

from tryselect.selectors.auto import AutoParser


def test_strategy_validation():
    parser = AutoParser()
    args = parser.parse_args(["--strategy", "relevant_tests"])
    assert args.strategy == "taskgraph.optimize:tryselect.relevant_tests"

    args = parser.parse_args(
        ["--strategy", "taskgraph.optimize:experimental.relevant_tests"]
    )
    assert args.strategy == "taskgraph.optimize:experimental.relevant_tests"

    with pytest.raises(SystemExit):
        parser.parse_args(["--strategy", "taskgraph.optimize:tryselect"])

    with pytest.raises(SystemExit):
        parser.parse_args(["--strategy", "foo"])

    with pytest.raises(SystemExit):
        parser.parse_args(["--strategy", "foo:bar"])


if __name__ == "__main__":
    mozunit.main()
