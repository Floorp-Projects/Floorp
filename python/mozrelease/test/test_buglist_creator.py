# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
from pathlib import Path

import mozunit
import pytest
from mozilla_version.gecko import GeckoVersion

from mozrelease.buglist_creator import (
    create_bugs_url,
    get_bugs_in_changeset,
    get_previous_tag_version,
    is_backout_bug,
    is_excluded_change,
    parse_tag_version,
    tag_version,
)

DATA_PATH = Path(__file__).with_name("data")


def test_beta_1_release():
    buglist_str_54_0b1 = create_bugs_url(
        product="firefox",
        current_version=GeckoVersion.parse("54.0b1"),
        current_revision="cf76e00dcd6f",
    )
    assert buglist_str_54_0b1 == "", "There should be no bugs to compare for beta 1."


@pytest.mark.parametrize(
    "description,is_excluded",
    (
        (
            "something something something a=test-only something something something",
            True,
        ),
        ("this is a a=release change!", True),
    ),
)
def test_is_excluded_change(description, is_excluded):
    assert is_excluded_change({"desc": description}) == is_excluded


@pytest.mark.parametrize(
    "description,is_backout",
    (
        ("I backed out this bug because", True),
        ("Backing out this bug due to", True),
        ("Backout bug xyz", True),
        ("Back out bug xyz", True),
        ("this is a regular bug description", False),
    ),
)
def test_is_backout_bug(description, is_backout):
    assert is_backout_bug(description) == is_backout


@pytest.mark.parametrize(
    "product,version,tag",
    (
        ("firefox", GeckoVersion.parse("53.0b10"), "FIREFOX_53_0b10_RELEASE"),
        ("firefox", GeckoVersion.parse("52.0"), "FIREFOX_52_0_RELEASE"),
        ("fennec", GeckoVersion.parse("52.0.2"), "FENNEC_52_0_2_RELEASE"),
    ),
)
def test_tag_version(product, version, tag):
    assert tag_version(product, version) == tag


@pytest.mark.parametrize(
    "tag,version",
    (
        ("FIREFOX_53_0b10_RELEASE", GeckoVersion.parse("53.0b10")),
        ("FIREFOX_52_0_RELEASE", GeckoVersion.parse("52.0")),
        ("FENNEC_52_0_2_RELEASE", GeckoVersion.parse("52.0.2")),
    ),
)
def test_parse_tag_version(tag, version):
    assert parse_tag_version(tag) == version


@pytest.mark.parametrize(
    "version,tag,previous_tag",
    (
        (
            GeckoVersion.parse("48.0b4"),
            "FIREFOX_48_0b4_RELEASE",
            "FIREFOX_48_0b3_RELEASE",
        ),
        (
            GeckoVersion.parse("48.0b9"),
            "FIREFOX_48_0b9_RELEASE",
            "FIREFOX_48_0b7_RELEASE",
        ),
        (
            GeckoVersion.parse("48.0.2"),
            "FIREFOX_48_0_2_RELEASE",
            "FIREFOX_48_0_1_RELEASE",
        ),
        (
            GeckoVersion.parse("48.0.1"),
            "FIREFOX_48_0_1_RELEASE",
            "FIREFOX_48_0_RELEASE",
        ),
    ),
)
def test_get_previous_tag_version(version, tag, previous_tag):
    product = "firefox"
    ff_48_tags = [
        "FIREFOX_BETA_48_END",
        "FIREFOX_RELEASE_48_END",
        "FIREFOX_48_0_2_RELEASE",
        "FIREFOX_48_0_2_BUILD1",
        "FIREFOX_48_0_1_RELEASE",
        "FIREFOX_48_0_1_BUILD3",
        "FIREFOX_48_0_RELEASE",
        "FIREFOX_48_0_BUILD2",
        "FIREFOX_RELEASE_48_BASE",
        "FIREFOX_48_0b10_RELEASE",
        "FIREFOX_48_0b10_BUILD1",
        "FIREFOX_48_0b9_RELEASE",
        "FIREFOX_48_0b9_BUILD1",
        "FIREFOX_48_0b7_RELEASE",
        "FIREFOX_48_0b7_BUILD1",
        "FIREFOX_48_0b6_RELEASE",
        "FIREFOX_48_0b6_BUILD1",
        "FIREFOX_48_0b5_RELEASE",
        "FIREFOX_48_0b5_BUILD1",
        "FIREFOX_48_0b4_RELEASE",
        "FIREFOX_48_0b4_BUILD1",
        "FIREFOX_48_0b3_RELEASE",
        "FIREFOX_48_0b3_BUILD1",
        "FIREFOX_48_0b2_RELEASE",
        "FIREFOX_48_0b2_BUILD2",
        "FIREFOX_48_0b1_RELEASE",
        "FIREFOX_48_0b1_BUILD2",
        "FIREFOX_AURORA_48_END",
        "FIREFOX_BETA_48_BASE",
        "FIREFOX_AURORA_48_BASE",
    ]

    mock_hg_json = {"tags": [{"tag": ff_48_tag} for ff_48_tag in ff_48_tags]}

    assert get_previous_tag_version(product, version, tag, mock_hg_json) == previous_tag


def test_get_bugs_in_changeset():
    with DATA_PATH.joinpath("buglist_changesets.json").open("r") as fp:
        changeset_data = json.load(fp)
    bugs, backouts = get_bugs_in_changeset(changeset_data)

    assert bugs == {
        "1356563",
        "1348409",
        "1341190",
        "1360626",
        "1332731",
        "1328762",
        "1355870",
        "1358089",
        "1354911",
        "1354038",
    }
    assert backouts == {"1337861", "1320072"}


if __name__ == "__main__":
    mozunit.main()
