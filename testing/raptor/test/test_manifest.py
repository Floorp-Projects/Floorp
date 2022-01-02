from __future__ import absolute_import, unicode_literals

import mozinfo
import mozunit
import os
import pytest
import sys

from six.moves.urllib.parse import parse_qs, urlsplit

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))

raptor_dir = os.path.join(os.path.dirname(here), "raptor")
sys.path.insert(0, raptor_dir)

from manifest import (
    add_test_url_params,
    get_browser_test_list,
    get_raptor_test_list,
    validate_test_ini,
)

# some test details (test INIs)
VALID_MANIFESTS = [
    {
        # page load test with local playback
        "alert_on": "fcp",
        "alert_threshold": 2.0,
        "apps": "firefox",
        "lower_is_better": True,
        "manifest": "valid_details_0",
        "measure": ["fnbpaint", "fcp"],
        "page_cycles": 25,
        "playback": "mitmproxy",
        "playback_pageset_manifest": "pageset.manifest",
        "test_url": "http://www.test-url/goes/here",
        "type": "pageload",
        "unit": "ms",
    },
    {
        # test optional settings with None
        "alert_threshold": 2.0,
        "apps": "firefox",
        "lower_is_better": True,
        "manifest": "valid_details_1",
        "measure": "fnbpaint, fcb",
        "page_cycles": 25,
        "test_url": "http://www.test-url/goes/here",
        "type": "pageload",
        "unit": "ms",
        "alert_change_type": None,
        "alert_on": None,
        "playback": None,
    },
    {
        # page load test for geckoview
        "alert_threshold": 2.0,
        "apps": "geckoview",
        "browser_cycles": 10,
        "lower_is_better": False,
        "manifest": "valid_details_2",
        "measure": "fcp",
        "page_cycles": 1,
        "test_url": "http://www.test-url/goes/here",
        "type": "pageload",
        "unit": "score",
    },
    {
        # benchmark test for chrome
        "alert_threshold": 2.0,
        "apps": "chrome",
        "lower_is_better": False,
        "manifest": "valid_details_1",
        "measure": "fcp",
        "page_cycles": 5,
        "test_url": "http://www.test-url/goes/here",
        "type": "benchmark",
        "unit": "score",
    },
    {
        # benchmark test for chromium
        "alert_threshold": 2.0,
        "apps": "chromium",
        "lower_is_better": False,
        "manifest": "valid_details_1",
        "measure": "fcp",
        "page_cycles": 5,
        "test_url": "http://www.test-url/goes/here",
        "type": "benchmark",
        "unit": "score",
    },
]

INVALID_MANIFESTS = [
    {
        "alert_threshold": 2.0,
        "apps": "firefox",
        "lower_is_better": True,
        "manifest": "invalid_details_0",
        "page_cycles": 25,
        "playback": "mitmproxy",
        "playback_pageset_manifest": "pageset.manifest",
        "test_url": "http://www.test-url/goes/here",
        "type": "pageload",
        "unit": "ms",
    },
    {
        "alert_threshold": 2.0,
        "apps": "chrome",
        "lower_is_better": True,
        "manifest": "invalid_details_1",
        "measure": "fnbpaint, fcp",
        "page_cycles": 25,
        "playback": "mitmproxy",
        "test_url": "http://www.test-url/goes/here",
        "type": "pageload",
        "unit": "ms",
    },
    {
        "alert_threshold": 2.0,
        "apps": "chromium",
        "lower_is_better": True,
        "manifest": "invalid_details_1",
        "measure": "fnbpaint, fcp",
        "page_cycles": 25,
        "playback": "mitmproxy",
        "test_url": "http://www.test-url/goes/here",
        "type": "pageload",
        "unit": "ms",
    },
    {
        "alert_on": "nope",
        "alert_threshold": 2.0,
        "apps": "firefox",
        "lower_is_better": True,
        "manifest": "invalid_details_2",
        "measure": "fnbpaint, fcp",
        "page_cycles": 25,
        "playback": "mitmproxy",
        "playback_pageset_manifest": "pageset.manifest",
        "test_url": "http://www.test-url/goes/here",
        "type": "pageload",
        "unit": "ms",
    },
]


@pytest.mark.parametrize(
    "app", ["firefox", "chrome", "chromium", "geckoview", "refbrow", "fenix"]
)
def test_get_browser_test_list(app):
    test_list = get_browser_test_list(app, run_local=True)
    assert len(test_list) > 0


@pytest.mark.parametrize("test_details", VALID_MANIFESTS)
def test_validate_test_ini_valid(test_details):
    assert validate_test_ini(test_details)


@pytest.mark.parametrize("test_details", INVALID_MANIFESTS)
def test_validate_test_ini_invalid(test_details):
    assert not (validate_test_ini(test_details))


def test_get_raptor_test_list_firefox(create_args):
    args = create_args(browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 4

    subtests = [
        "raptor-tp6-unittest-google-firefox",
        "raptor-tp6-unittest-amazon-firefox",
        "raptor-tp6-unittest-facebook-firefox",
        "raptor-tp6-unittest-youtube-firefox",
    ]

    for next_subtest in test_list:
        assert next_subtest["name"] in subtests


def test_get_raptor_test_list_chrome(create_args):
    args = create_args(app="chrome", test="speedometer", browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "speedometer"


def test_get_raptor_test_list_geckoview(create_args):
    args = create_args(app="geckoview", test="unity-webgl", browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "unity-webgl"


def test_get_raptor_test_list_gecko_profiling_enabled(create_args):
    args = create_args(test="amazon", gecko_profile=True, browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0]["gecko_profile"] is True
    assert test_list[0].get("gecko_profile_entries") == "14000000"
    assert test_list[0].get("gecko_profile_interval") == "1"
    assert test_list[0].get("gecko_profile_threads") is None
    assert test_list[0].get("gecko_profile_features") is None


def test_get_raptor_test_list_gecko_profiling_enabled_args_override(create_args):
    args = create_args(
        test="amazon",
        gecko_profile=True,
        gecko_profile_entries=42,
        gecko_profile_interval=100,
        gecko_profile_threads="Foo",
        gecko_profile_features="Mood,UserNetWorth",
        browser_cycles=1,
    )

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0]["gecko_profile"] is True
    assert test_list[0]["gecko_profile_entries"] == "42"
    assert test_list[0]["gecko_profile_interval"] == "100"
    assert test_list[0]["gecko_profile_threads"] == "Foo"
    assert test_list[0]["gecko_profile_features"] == "Mood,UserNetWorth"


def test_get_raptor_test_list_gecko_profiling_enabled_extra_args_override(create_args):
    args = create_args(
        test="amazon",
        gecko_profile=True,
        gecko_profile_entries=42,
        gecko_profile_interval=100,
        gecko_profile_extra_threads=["Foo", "Oof"],
        gecko_profile_threads="String,Rope",
        browser_cycles=1,
    )

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0]["gecko_profile"] is True
    assert test_list[0]["gecko_profile_entries"] == "42"
    assert test_list[0]["gecko_profile_interval"] == "100"
    assert test_list[0]["gecko_profile_threads"] == "String,Rope,Foo,Oof"


def test_get_raptor_test_list_gecko_profiling_disabled(create_args):
    args = create_args(
        test="amazon",
        gecko_profile=False,
        gecko_profile_entries=42,
        gecko_profile_interval=100,
        gecko_profile_threads=["Foo"],
        gecko_profile_features=["Temperature"],
        browser_cycles=1,
    )

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0].get("gecko_profile") is None
    assert test_list[0].get("gecko_profile_entries") is None
    assert test_list[0].get("gecko_profile_interval") is None
    assert test_list[0].get("gecko_profile_threads") is None
    assert test_list[0].get("gecko_profile_features") is None


def test_get_raptor_test_list_gecko_profiling_disabled_args_override(create_args):
    args = create_args(
        test="amazon",
        gecko_profile=False,
        gecko_profile_entries=42,
        gecko_profile_interval=100,
        gecko_profile_threads=["Foo"],
        gecko_profile_features=["Temperature"],
        browser_cycles=1,
    )

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0].get("gecko_profile") is None
    assert test_list[0].get("gecko_profile_entries") is None
    assert test_list[0].get("gecko_profile_interval") is None
    assert test_list[0].get("gecko_profile_threads") is None
    assert test_list[0].get("gecko_profile_features") is None


def test_get_raptor_test_list_debug_mode(create_args):
    args = create_args(test="amazon", debug_mode=True, browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0]["debug_mode"] is True
    assert test_list[0]["page_cycles"] == 2


def test_get_raptor_test_list_using_live_sites(create_args):
    args = create_args(test="amazon", live_sites=True, browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0]["use_live_sites"] == "true"
    assert test_list[0]["playback"] is None


def test_get_raptor_test_list_override_page_cycles(create_args):
    args = create_args(test="amazon", page_cycles=99, browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0]["page_cycles"] == 99


def test_get_raptor_test_list_override_page_timeout(create_args):
    args = create_args(test="amazon", page_timeout=9999, browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    assert test_list[0]["page_timeout"] == 9999


def test_get_raptor_test_list_add_test_url_params(create_args):
    args = create_args(test="amazon", test_url_params="c=4", browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "amazon"
    query_params = parse_qs(urlsplit(test_list[0]["test_url"]).query)
    assert query_params.get("c") == ["4"]


def test_get_raptor_test_list_refbrow(create_args):
    args = create_args(app="refbrow", test="speedometer", browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "speedometer"


def test_get_raptor_test_list_fenix(create_args):
    args = create_args(app="fenix", test="speedometer", browser_cycles=1)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]["name"] == "speedometer"


def test_add_test_url_params_with_single_extra_param():
    initial_test_url = "http://test.com?a=1&b=2"
    extra_params = "c=3"

    result = add_test_url_params(initial_test_url, extra_params)

    expected_params = {"a": ["1"], "b": ["2"], "c": ["3"]}
    actual_params = parse_qs(urlsplit(result).query)
    assert actual_params == expected_params


def test_add_test_url_params_with_multiple_extra_param():
    initial_test_url = "http://test.com?a=1&b=2"
    extra_params = "c=3&d=4"

    result = add_test_url_params(initial_test_url, extra_params)

    expected_params = {"a": ["1"], "b": ["2"], "c": ["3"], "d": ["4"]}
    actual_params = parse_qs(urlsplit(result).query)
    assert actual_params == expected_params


def test_add_test_url_params_without_params_in_url():
    initial_test_url = "http://test.com"
    extra_params = "c=3"

    result = add_test_url_params(initial_test_url, extra_params)

    expected_params = {"c": ["3"]}
    actual_params = parse_qs(urlsplit(result).query)
    assert actual_params == expected_params


def test_add_test_url_params_overwrites_single_param():
    initial_test_url = "http://test.com?a=1&b=2"
    extra_params = "b=3"

    result = add_test_url_params(initial_test_url, extra_params)

    expected_params = {"a": ["1"], "b": ["3"]}
    actual_params = parse_qs(urlsplit(result).query)
    assert actual_params == expected_params


def test_add_test_url_params_overwrites_multiple_param():
    initial_test_url = "http://test.com?a=1&b=2&c=3"
    extra_params = "c=4&b=5"

    result = add_test_url_params(initial_test_url, extra_params)

    expected_params = {"a": ["1"], "b": ["5"], "c": ["4"]}
    actual_params = parse_qs(urlsplit(result).query)
    assert actual_params == expected_params


if __name__ == "__main__":
    mozunit.main()
