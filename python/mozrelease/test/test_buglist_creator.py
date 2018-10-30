# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
from pathlib2 import Path

import mozunit
from pkg_resources import parse_version
from mozrelease.buglist_creator import (
    is_excluded_change, create_bugs_url, is_backout_bug, get_previous_tag_version,
    get_bugs_in_changeset, dot_version_to_tag_version, tag_version_to_dot_version_parse,
)


DATA_PATH = Path(__file__).with_name("data")


def test_beta_1_release():
    release_object_54_0b1 = {
        'branch': 'releases/mozilla-beta',
        'product': 'firefox',
        'version': '54.0b1',
        'mozillaRevision': 'cf76e00dcd6f',
    }
    buglist_str_54_0b1 = create_bugs_url(release_object_54_0b1)
    assert buglist_str_54_0b1 == '', 'There should be no bugs to compare for beta 1.'


def test_is_excluded_change():
    excluded_changesets = [
        {'desc': 'something something something a=test-only something something something'},
        {'desc': 'this is a a=release change!'},
    ]
    assert all(is_excluded_change(excluded) for excluded in excluded_changesets), \
        'is_excluded_change failed to exclude a changeset.'


def test_is_backout_bug():
    backout_bugs_descs = [
        'I backed out this bug because',
        'Backing out this bug due to',
        'Backout bug xyz',
        'Back out bug xyz',
    ]

    not_backout_bugs = [
        'this is a regular bug description',
    ]

    assert all(is_backout_bug(backout_desc.lower()) for backout_desc in backout_bugs_descs)
    assert all(not is_backout_bug(regular_desc.lower()) for regular_desc in not_backout_bugs)


def test_dot_version_to_tag_version():
    test_tuples = [
        (['firefox', '53.0b10'], 'FIREFOX_53_0b10_RELEASE'),
        (['firefox', '52.0'], 'FIREFOX_52_0_RELEASE'),
        (['fennec', '52.0.2'], 'FENNEC_52_0_2_RELEASE'),
    ]

    assert all(dot_version_to_tag_version(*args) == results for args, results in test_tuples)


def test_tag_version_to_dot_version_parse():
    test_tuples = [
        ('FIREFOX_53_0b10_RELEASE', parse_version('53.0b10')),
        ('FIREFOX_52_0_RELEASE', parse_version('52.0')),
        ('FENNEC_52_0_2_RELEASE', parse_version('52.0.2')),
    ]

    assert all(tag_version_to_dot_version_parse(tag) == expected for tag, expected in test_tuples)


def test_get_previous_tag_version():
    product = 'firefox'
    ff_48_tags = [
        u'FIREFOX_BETA_48_END',
        u'FIREFOX_RELEASE_48_END',
        u'FIREFOX_48_0_2_RELEASE',
        u'FIREFOX_48_0_2_BUILD1',
        u'FIREFOX_48_0_1_RELEASE',
        u'FIREFOX_48_0_1_BUILD3',
        u'FIREFOX_48_0_RELEASE',
        u'FIREFOX_48_0_BUILD2',
        u'FIREFOX_RELEASE_48_BASE',
        u'FIREFOX_48_0b10_RELEASE',
        u'FIREFOX_48_0b10_BUILD1',
        u'FIREFOX_48_0b9_RELEASE',
        u'FIREFOX_48_0b9_BUILD1',
        u'FIREFOX_48_0b7_RELEASE',
        u'FIREFOX_48_0b7_BUILD1',
        u'FIREFOX_48_0b6_RELEASE',
        u'FIREFOX_48_0b6_BUILD1',
        u'FIREFOX_48_0b5_RELEASE',
        u'FIREFOX_48_0b5_BUILD1',
        u'FIREFOX_48_0b4_RELEASE',
        u'FIREFOX_48_0b4_BUILD1',
        u'FIREFOX_48_0b3_RELEASE',
        u'FIREFOX_48_0b3_BUILD1',
        u'FIREFOX_48_0b2_RELEASE',
        u'FIREFOX_48_0b2_BUILD2',
        u'FIREFOX_48_0b1_RELEASE',
        u'FIREFOX_48_0b1_BUILD2',
        u'FIREFOX_AURORA_48_END',
        u'FIREFOX_BETA_48_BASE',
        u'FIREFOX_AURORA_48_BASE',
    ]

    mock_hg_json = {
        'tags': [
            {'tag': tag} for tag in ff_48_tags
        ],
    }

    test_tuples = [
        ('48.0b4', 'FIREFOX_48_0b4_RELEASE', 'FIREFOX_48_0b3_RELEASE'),
        ('48.0b9', 'FIREFOX_48_0b9_RELEASE', 'FIREFOX_48_0b7_RELEASE'),
        ('48.0.2', 'FIREFOX_48_0_2_RELEASE', 'FIREFOX_48_0_1_RELEASE'),
        ('48.0.1', 'FIREFOX_48_0_1_RELEASE', 'FIREFOX_48_0_RELEASE'),
    ]

    assert all(
        get_previous_tag_version(product, dot_version, tag_version, mock_hg_json) == expected
        for dot_version, tag_version, expected in test_tuples)


def test_get_bugs_in_changeset():
    with DATA_PATH.joinpath("buglist_changesets.json").open("r") as fp:
        changeset_data = json.load(fp)
    bugs, backouts = get_bugs_in_changeset(changeset_data)

    assert bugs == {u'1356563', u'1348409', u'1341190', u'1360626', u'1332731', u'1328762',
                    u'1355870', u'1358089', u'1354911', u'1354038'}
    assert backouts == {u'1337861', u'1320072'}


if __name__ == '__main__':
    mozunit.main()
