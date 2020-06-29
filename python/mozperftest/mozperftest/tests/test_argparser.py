#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import pytest
from datetime import date

from mozperftest.argparser import PerftestArgumentParser, Options


def test_argparser():
    parser = PerftestArgumentParser()
    args = ["test_one.js"]
    res = parser.parse_args(args)
    assert res.tests == ["test_one.js"]


def test_options():
    assert Options.args["--proxy"]["help"] == "Activates the proxy layer"
    assert Options.args["--no-browsertime"]["help"] == (
        "Deactivates the " "browsertime layer"
    )


def test_bad_test_date():
    parser = PerftestArgumentParser()
    args = ["test_one.js", "--test-date", "bleh"]
    with pytest.raises(SystemExit):
        parser.parse_args(args)


def test_test_date_today():
    parser = PerftestArgumentParser()
    args = ["test_one.js", "--test-date", "today"]
    res = parser.parse_args(args)
    assert res.test_date == date.today().strftime("%Y.%m.%d")


if __name__ == "__main__":
    mozunit.main()
