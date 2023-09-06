#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from datetime import date

import mozunit
import pytest

from mozperftest.argparser import (
    Options,
    PerftestArgumentParser,
    PerftestToolsArgumentParser,
)


def test_argparser():
    parser = PerftestArgumentParser()
    args = ["test_one.js"]
    res = parser.parse_args(args)
    assert res.tests == ["test_one.js"]


def test_argparser_defaults():
    parser = PerftestArgumentParser()
    args = ["test_one.js"]
    res = parser.parse_args(args)
    assert res.console_simplify_exclude == ["statistics"]


def test_options():
    assert Options.args["--proxy"]["help"] == "Activates the proxy layer"
    assert Options.args["--no-browsertime"]["help"] == (
        "Deactivates the " "browsertime layer"
    )


def test_layer_option():
    parser = PerftestArgumentParser()
    assert parser.parse_args(["--notebook-metrics"]) == parser.parse_args(
        ["--notebook-metrics", "--notebook"]
    )
    assert parser.parse_known_args(["--notebook-metrics"]) == parser.parse_known_args(
        ["--notebook-metrics", "--notebook"]
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


def test_perfherder_metrics():
    parser = PerftestArgumentParser()
    args = [
        "test_one.js",
        "--perfherder-metrics",
        "name:foo,unit:ms,alertThreshold:2",
        "name:baz,unit:count,alertThreshold:2,lowerIsBetter:false",
    ]

    res = parser.parse_args(args)
    assert res.perfherder_metrics[0]["name"] == "foo"
    assert res.perfherder_metrics[1]["alertThreshold"] == 2

    args = [
        "test_one.js",
        "--perfherder-metrics",
        "name:foo,unit:ms,alertThreshold:2",
        "name:baz,UNKNOWN:count,alertThreshold:2,lowerIsBetter:false",
    ]

    with pytest.raises(SystemExit):
        parser.parse_args(args)

    args = [
        "test_one.js",
        "--perfherder-metrics",
        "name:foo,unit:ms,alertThreshold:2",
        "namemalformedbaz,alertThreshold:2,lowerIsBetter:false",
    ]

    with pytest.raises(SystemExit):
        parser.parse_args(args)

    # missing the name!
    args = [
        "test_one.js",
        "--perfherder-metrics",
        "name:foo,unit:ms,alertThreshold:2",
        "alertThreshold:2,lowerIsBetter:false",
    ]

    with pytest.raises(SystemExit):
        parser.parse_args(args)

    # still supporting just plain names
    args = [
        "test_one.js",
        "--perfherder-metrics",
        "name:foo,unit:euros,alertThreshold:2",
        "baz",
    ]

    res = parser.parse_args(args)
    assert res.perfherder_metrics[1]["name"] == "baz"
    assert res.perfherder_metrics[0]["name"] == "foo"
    assert res.perfherder_metrics[0]["unit"] == "euros"


def test_tools_argparser_bad_tool():
    with pytest.raises(SystemExit):
        PerftestToolsArgumentParser()


def test_tools_bad_argparser():
    PerftestToolsArgumentParser.tool = "side-by-side"
    parser = PerftestToolsArgumentParser()
    args = [
        "-t",
        "browsertime-first-install-firefox-welcome",
        "--base-platform",
        "test-linux1804-64-shippable-qr",
    ]
    with pytest.raises(SystemExit):
        parser.parse_args(args)


def test_tools_argparser():
    PerftestToolsArgumentParser.tool = "side-by-side"
    parser = PerftestToolsArgumentParser()
    args = [
        "-t",
        "browsertime-first-install-firefox-welcome",
        "--base-platform",
        "test-linux1804-64-shippable-qr",
        "--base-revision",
        "438092d03ac4b9c36b52ba455da446afc7e14213",
        "--new-revision",
        "29943068938aa9e94955dbe13c2e4c254553e4ce",
    ]
    res = parser.parse_args(args)
    assert res.test_name == "browsertime-first-install-firefox-welcome"
    assert res.platform == "test-linux1804-64-shippable-qr"
    assert res.base_revision == "438092d03ac4b9c36b52ba455da446afc7e14213"
    assert res.new_revision == "29943068938aa9e94955dbe13c2e4c254553e4ce"


if __name__ == "__main__":
    mozunit.main()
