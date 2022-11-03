# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from unittest import mock

import mozunit
import pytest

import tryselect.selectors.perf as ps

TASKS = [
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-animometer",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-optimizing",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-webaudio",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-speedometer",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-jetstream2",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-ares6",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-optimizing",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-sunspider",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-matrix-react-bench",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot-baseline",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-twitch-animation",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-assorted-dom",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-stylebench",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-misc-baseline",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-motionmark-htmlsuite",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-firefox-unity-webgl",
    "test-linux1804-64-shippable-qr/opt-browsertime-benchmark-wasm-firefox-wasm-godot",
]


@pytest.mark.parametrize(
    "category_options, expected_counts, unique_categories, missing",
    [
        # Default should show the premade live category, but no chrome or android
        # The benchmark desktop category should be visible in all configurations
        # except for when there are requested apps/variants/platforms
        (
            {},
            48,
            {
                "Benchmarks desktop": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!live",
                    "!profil",
                    "!chrom",
                ],
                "Pageload (live) macosx": [
                    "'browsertime 'tp6 'live",
                    "'osx 'shippable",
                    "!live",
                    "!profil",
                    "!chrom",
                ],
            },
            ["Responsiveness android-p2 geckoview", "Benchmarks desktop chromium"],
        ),  # Default settings
        (
            {"live_sites": True},
            240,
            {
                "Benchmarks desktop": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!profil",
                    "!chrom",
                ],
                "Pageload (live) macosx": [
                    "'browsertime 'tp6 'live",
                    "'osx 'shippable",
                    "!profil",
                    "!chrom",
                ],
                "Benchmarks desktop firefox live-sites+profiling": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!chrom",
                    "!chrom !geckoview !fenix",
                    "'live",
                    "'profil",
                ],
            },
            [
                "Responsiveness android-p2 geckoview",
                "Benchmarks desktop chromium",
                "Benchmarks desktop firefox profiling",
            ],
        ),
        (
            {"live_sites": True, "chrome": True},
            480,
            {
                "Benchmarks desktop": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!profil",
                ],
                "Pageload (live) macosx": [
                    "'browsertime 'tp6 'live",
                    "'osx 'shippable",
                    "!profil",
                ],
                "Benchmarks desktop chromium": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!profil",
                    "'chromium",
                ],
            },
            ["Responsiveness android-p2 geckoview"],
        ),
        (
            {"android": True},
            420,
            {
                "Benchmarks desktop": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!live",
                    "!profil",
                    "!chrom",
                ],
                "Responsiveness android-a51 geckoview": [
                    "'browsertime 'responsive",
                    "'android 'a51 'shippable 'aarch64",
                    "!live",
                    "!profil",
                    "!chrom",
                    "'geckoview",
                ],
            },
            ["Responsiveness android-a51 chrome-m"],
        ),
        (
            {"android": True, "chrome": True},
            720,
            {
                "Benchmarks desktop": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!live",
                    "!profil",
                ],
                "Responsiveness android-a51 chrome-m": [
                    "'browsertime 'responsive",
                    "'android 'a51 'shippable 'aarch64",
                    "!live",
                    "!profil",
                    "'chrome-m",
                ],
            },
            ["Responsiveness android-p2 chrome-m"],
        ),
        (
            {"android": True, "chrome": True, "profile": True},
            1008,
            {
                "Benchmarks desktop": [
                    "'browsertime 'benchmark",
                    "!android 'shippable !-32 !clang",
                    "!live",
                ]
            },
            [],
        ),
        # Show all available windows tests, no other platform should exist
        # including the desktop catgeory
        (
            {"requested_platforms": ["windows"]},
            84,
            {
                "Benchmarks windows firefox bytecode-cached+profiling": [
                    "'browsertime 'benchmark",
                    "!-32 'windows 'shippable",
                    "!live",
                    "!chrom",
                    "!chrom !geckoview !fenix",
                    "'bytecode",
                    "'profil",
                ]
            },
            [
                "Benchmarks desktop",
                "Benchmarks linux firefox bytecode-cached+profiling",
            ],
        ),
        # Can't have fenix on the windows platform
        (
            {"requested_platforms": ["windows"], "requested_apps": ["fenix"]},
            0,
            {},
            ["Benchmarks desktop"],
        ),
        # Android flag also needs to be supplied
        (
            {"requested_platforms": ["android"], "requested_apps": ["fenix"]},
            0,
            {},
            ["Benchmarks desktop"],
        ),
        # There should be no global categories available, only fenix
        (
            {
                "requested_platforms": ["android"],
                "requested_apps": ["fenix"],
                "android": True,
            },
            42,
            {
                "Bytecode Cached android fenix no-fission+live-sites+profiling": [
                    "'browsertime 'bytecode",
                    "'android 'a51 'shippable 'aarch64",
                    "!chrom",
                    "'fenix",
                    "'nofis",
                    "'live",
                    "'profil",
                ],
            },
            ["Benchmarks desktop", "Pageload (live) android"],
        ),
        # Test with multiple apps
        (
            {
                "requested_platforms": ["android"],
                "requested_apps": ["fenix", "geckoview"],
                "android": True,
            },
            84,
            {
                "Benchmarks android geckoview live-sites+profiling": [
                    "'browsertime 'benchmark",
                    "'android 'a51 'shippable 'aarch64",
                    "!chrom",
                    "'geckoview",
                    "'live",
                    "'profil",
                ],
                "Bytecode Cached android fenix no-fission+live-sites+profiling": [
                    "'browsertime 'bytecode",
                    "'android 'a51 'shippable 'aarch64",
                    "!chrom",
                    "'fenix",
                    "'nofis",
                    "'live",
                    "'profil",
                ],
            },
            [
                "Benchmarks desktop",
                "Pageload (live) android",
                "Pageload android-p2 fenix live-sites",
            ],
        ),
        # Variants are inclusive, so we'll see the variant alongside the
        # base here for fenix
        (
            {
                "requested_variants": ["no-fission"],
                "requested_apps": ["fenix"],
                "android": True,
            },
            60,
            {
                "Pageload android-a51 fenix": [
                    "'browsertime 'tp6",
                    "'android 'a51 'shippable 'aarch64",
                    "!live",
                    "!profil",
                    "!chrom",
                    "'fenix",
                ],
                "Pageload android-a51 fenix no-fission": [
                    "'browsertime 'tp6",
                    "'android 'a51 'shippable 'aarch64",
                    "!live",
                    "!profil",
                    "!chrom",
                    "'fenix",
                    "'nofis",
                ],
                "Pageload (essential) android fenix no-fission+live-sites": [
                    "'browsertime 'tp6 'essential",
                    "'android 'a51 'shippable 'aarch64",
                    "!profil",
                    "!chrom",
                    "'fenix",
                    "'nofis",
                    "'live",
                ],
            },
            [
                "Benchmarks desktop",
                "Pageload (live) android",
                "Pageload android-p2 fenix live-sites",
            ],
        ),
        # With multiple variants, we'll see the base variant (with no combinations)
        # for each of them
        (
            {
                "requested_variants": ["no-fission", "live-sites"],
                "requested_apps": ["fenix"],
                "android": True,
            },
            84,
            {
                "Pageload android-a51 fenix": [
                    "'browsertime 'tp6",
                    "'android 'a51 'shippable 'aarch64",
                    "!live",
                    "!profil",
                    "!chrom",
                    "'fenix",
                ],
                "Pageload android-a51 fenix no-fission": [
                    "'browsertime 'tp6",
                    "'android 'a51 'shippable 'aarch64",
                    "!live",
                    "!profil",
                    "!chrom",
                    "'fenix",
                    "'nofis",
                ],
                "Pageload android-a51 fenix live-sites": [
                    "'browsertime 'tp6",
                    "'android 'a51 'shippable 'aarch64",
                    "!profil",
                    "!chrom",
                    "'fenix",
                    "'live",
                ],
                "Pageload (essential) android fenix no-fission+live-sites": [
                    "'browsertime 'tp6 'essential",
                    "'android 'a51 'shippable 'aarch64",
                    "!profil",
                    "!chrom",
                    "'fenix",
                    "'nofis",
                    "'live",
                ],
            },
            [
                "Benchmarks desktop",
                "Pageload (live) android",
                "Pageload android-p2 fenix live-sites",
            ],
        ),
        # Make sure that no no-fission tasks are selected when a variant cannot
        # run on a requested platform
        (
            {
                "requested_variants": ["no-fission"],
                "requested_platforms": ["windows"],
            },
            12,
            {
                "Responsiveness windows firefox": [
                    "'browsertime 'responsive",
                    "!-32 'windows 'shippable",
                    "!live",
                    "!profil",
                    "!chrom",
                    "!chrom !geckoview !fenix",
                ],
            },
            ["Benchmarks desktop", "Responsiveness windows firefox no-fisson"],
        ),
        # We should only see the base and the live-site variants here
        (
            {
                "requested_variants": ["no-fission", "live-sites"],
                "requested_platforms": ["windows"],
                "android": True,
            },
            60,
            {
                "Responsiveness windows firefox": [
                    "'browsertime 'responsive",
                    "!-32 'windows 'shippable",
                    "!live",
                    "!profil",
                    "!chrom",
                    "!chrom !geckoview !fenix",
                ],
                "Responsiveness windows firefox live-sites": [
                    "'browsertime 'responsive",
                    "!-32 'windows 'shippable",
                    "!profil",
                    "!chrom",
                    "!chrom !geckoview !fenix",
                    "'live",
                ],
            },
            [
                "Benchmarks desktop",
                "Responsiveness windows firefox no-fisson",
                "Pageload (live) android",
            ],
        ),
    ],
)
def test_category_expansion(
    category_options, expected_counts, unique_categories, missing
):
    # Expand the categories, then either check if the unique_categories,
    # exist or are missing from the categories
    expanded_cats = ps.PerfParser.expand_categories(**category_options)

    assert len(expanded_cats) == expected_counts
    assert not any([expanded_cats.get(ucat, None) is not None for ucat in missing])
    assert all(
        [expanded_cats.get(ucat, None) is not None for ucat in unique_categories.keys()]
    )

    # Ensure that the queries are as expected
    for cat_name, cat_query in unique_categories.items():
        # Don't use get here because these fields should always exist
        assert cat_query == expanded_cats[cat_name]["queries"]


@pytest.mark.parametrize(
    "options, call_counts, log_ind, expected_log_message",
    [
        (
            {},
            [6, 2, 2, 5],
            2,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=revision&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"dry_run": True},
            [6, 1, 1, 5],
            2,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"show_all": True},
            [1, 2, 2, 3],
            0,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=revision&newProject=try&newRevision=revision\n"
            ),
        ),
    ],
)
@pytest.mark.skipif(os.name == "nt", reason="fzf not installed on host")
def test_full_run(options, call_counts, log_ind, expected_log_message):
    with mock.patch("tryselect.selectors.perf.push_to_try") as ptt, mock.patch(
        "tryselect.selectors.perf.run_fzf"
    ) as fzf, mock.patch(
        "tryselect.selectors.perf.get_repository_object", new=mock.MagicMock()
    ), mock.patch(
        "tryselect.selectors.perf.LogProcessor.revision",
        new_callable=mock.PropertyMock,
        return_value="revision",
    ) as logger, mock.patch(
        "tryselect.selectors.perf.print",
    ) as perf_print:
        fzf.side_effect = [
            ["", ["Benchmarks linux"]],
            ["", TASKS],
            ["", TASKS],
            ["", TASKS],
            ["", TASKS],
            ["", TASKS],
        ]

        ps.run(**options)

        assert fzf.call_count == call_counts[0]
        assert ptt.call_count == call_counts[1]
        assert logger.call_count == call_counts[2]
        assert perf_print.call_count == call_counts[3]
        assert perf_print.call_args_list[log_ind][0][0] == expected_log_message


if __name__ == "__main__":
    mozunit.main()
