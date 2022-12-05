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

# The TEST_VARIANTS, and TEST_CATEGORIES are used to force
# a particular set of categories to show up in testing. Otherwise,
# every time someone adds a category, or a variant, we'll need
# to redo all the category counts. The platforms, and apps are
# not forced because they change infrequently.
TEST_VARIANTS = {
    "no-fission": {
        "query": "'nofis",
        "negation": "!nofis",
        "platforms": ["android"],
        "apps": ["fenix", "geckoview"],
    },
    "bytecode-cached": {
        "query": "'bytecode",
        "negation": "!bytecode",
        "platforms": ["desktop"],
        "apps": ["firefox"],
    },
    "live-sites": {
        "query": "'live",
        "negation": "!live",
        "platforms": ["desktop", "android"],
        "apps": list(ps.PerfParser.apps.keys()),
    },
    "profiling": {
        "query": "'profil",
        "negation": "!profil",
        "platforms": ["desktop", "android"],
        "apps": ["firefox", "geckoview", "fenix"],
    },
    "swr": {
        "query": "'swr",
        "negation": "!swr",
        "platforms": ["desktop"],
        "apps": ["firefox"],
    },
}

TEST_CATEGORIES = {
    "Pageload": {
        "query": {
            "raptor": ["'browsertime 'tp6"],
        },
        "suites": ["raptor"],
        "tasks": [],
    },
    "Pageload (essential)": {
        "query": {
            "raptor": ["'browsertime 'tp6 'essential"],
        },
        "suites": ["raptor"],
        "tasks": [],
    },
    "Pageload (live)": {
        "query": {
            "raptor": ["'browsertime 'tp6 'live"],
        },
        "suites": ["raptor"],
        "tasks": [],
    },
    "Bytecode Cached": {
        "query": {
            "raptor": ["'browsertime 'bytecode"],
        },
        "suites": ["raptor"],
        "tasks": [],
    },
    "Responsiveness": {
        "query": {
            "raptor": ["'browsertime 'responsive"],
        },
        "suites": ["raptor"],
        "tasks": [],
    },
    "Benchmarks": {
        "query": {
            "raptor": ["'browsertime 'benchmark"],
        },
        "suites": ["raptor"],
        "tasks": [],
    },
    "DAMP (Devtools)": {
        "query": {
            "talos": ["'talos 'damp"],
        },
        "suites": ["talos"],
        "tasks": [],
    },
    "Talos PerfTests": {
        "query": {
            "talos": ["'talos"],
        },
        "suites": ["talos"],
        "tasks": [],
    },
    "Resource Usage": {
        "query": {
            "talos": ["'talos 'xperf | 'tp5"],
            "raptor": ["'power 'osx"],
            "awsy": ["'awsy"],
        },
        "suites": ["talos", "raptor", "awsy"],
        "platform-restrictions": ["desktop"],
        "variant-restrictions": {
            "raptor": [],
            "talos": [],
        },
        "app-restrictions": {
            "raptor": ["firefox"],
            "talos": ["firefox"],
        },
        "tasks": [],
    },
    "Graphics, & Media Playback": {
        "query": {
            # XXX This might not be an exhaustive list for talos atm
            "talos": ["'talos 'svgr | 'bcv | 'webgl"],
            "raptor": ["'browsertime 'youtube-playback"],
        },
        "suites": ["talos", "raptor"],
        "tasks": [],
    },
}


@pytest.mark.parametrize(
    "category_options, expected_counts, unique_categories, missing",
    [
        # Default should show the premade live category, but no chrome or android
        # The benchmark desktop category should be visible in all configurations
        # except for when there are requested apps/variants/platforms
        (
            {},
            76,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                    ]
                },
                "Pageload (live) macosx": {
                    "raptor": [
                        "'browsertime 'tp6 'live",
                        "'osx 'shippable",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                    ]
                },
                "Resource Usage linux": {
                    "awsy": ["'awsy", "!clang 'linux 'shippable"],
                    "raptor": [
                        "'power 'osx",
                        "!clang 'linux 'shippable",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                    ],
                    "talos": ["'talos 'xperf | 'tp5", "!clang 'linux 'shippable"],
                },
            },
            [
                "Responsiveness android-p2 geckoview",
                "Benchmarks desktop chromium",
            ],
        ),  # Default settings
        (
            {"live_sites": True},
            332,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!profil",
                        "!chrom",
                        "!safari",
                    ],
                },
                "Pageload (live) macosx": {
                    "raptor": [
                        "'browsertime 'tp6 'live",
                        "'osx 'shippable",
                        "!profil",
                        "!chrom",
                        "!safari",
                    ],
                },
                "Benchmarks desktop firefox live-sites+profiling": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!chrom",
                        "!safari",
                        "!chrom !geckoview !fenix !safari",
                        "'live",
                        "'profil",
                    ],
                },
                "Graphics, & Media Playback desktop live-sites+profiling+swr": {
                    "raptor": [
                        "'browsertime 'youtube-playback",
                        "!android 'shippable !-32 !clang",
                        "!chrom",
                        "!safari",
                        "'live",
                        "'profil",
                    ],
                    "talos": [
                        "'talos 'svgr | 'bcv | 'webgl",
                        "!android 'shippable !-32 !clang",
                        "'profil",
                        "'swr",
                    ],
                },
            },
            [
                "Responsiveness android-p2 geckoview",
                "Benchmarks desktop chromium",
                "Benchmarks desktop firefox profiling",
                "Talos desktop live-sites",
                "Talos desktop profiling+swr",
            ],
        ),
        (
            {"live_sites": True, "chrome": True},
            644,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!profil",
                        "!safari",
                    ],
                },
                "Pageload (live) macosx": {
                    "raptor": [
                        "'browsertime 'tp6 'live",
                        "'osx 'shippable",
                        "!profil",
                        "!safari",
                    ],
                },
                "Benchmarks desktop chromium": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!profil",
                        "!safari",
                        "'chromium",
                    ],
                },
            },
            [
                "Responsiveness android-p2 geckoview",
                "Firefox Pageload linux chrome",
                "Talos PerfTests desktop swr",
            ],
        ),
        (
            {"android": True},
            542,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                    ],
                },
                "Responsiveness android-a51 geckoview": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "'android 'a51 'shippable 'aarch64",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'geckoview",
                    ],
                },
            },
            [
                "Responsiveness android-a51 chrome-m",
                "Firefox Pageload android",
            ],
        ),
        (
            {"android": True, "chrome": True},
            924,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!live",
                        "!profil",
                        "!safari",
                    ],
                },
                "Responsiveness android-a51 chrome-m": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "'android 'a51 'shippable 'aarch64",
                        "!live",
                        "!profil",
                        "!safari",
                        "'chrome-m",
                    ],
                },
            },
            ["Responsiveness android-p2 chrome-m", "Resource Usage android"],
        ),
        (
            {"android": True, "chrome": True, "profile": True},
            1324,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!live",
                        "!safari",
                    ]
                },
                "Talos PerfTests desktop profiling+swr": {
                    "talos": [
                        "'talos",
                        "!android 'shippable !-32 !clang",
                        "'profil",
                        "'swr",
                    ]
                },
            },
            [
                "Resource Usage desktop profiling",
                "DAMP (Devtools) desktop chrome",
                "Resource Usage android",
                "Resource Usage windows chromium",
            ],
        ),
        # Show all available windows tests, no other platform should exist
        # including the desktop catgeory
        (
            {"requested_platforms": ["windows"]},
            123,
            {
                "Benchmarks windows firefox bytecode-cached+profiling": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!-32 'windows 'shippable",
                        "!live",
                        "!chrom",
                        "!safari",
                        "!chrom !geckoview !fenix !safari",
                        "'bytecode",
                        "'profil",
                    ]
                },
            },
            [
                "Resource Usage desktop",
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
            49,
            {
                "Bytecode Cached android fenix no-fission+live-sites+profiling": {
                    "raptor": [
                        "'browsertime 'bytecode",
                        "'android 'a51 'shippable 'aarch64",
                        "!chrom",
                        "!safari",
                        "'fenix",
                        "'nofis",
                        "'live",
                        "'profil",
                    ],
                }
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
            98,
            {
                "Benchmarks android geckoview live-sites+profiling": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "'android 'a51 'shippable 'aarch64",
                        "!chrom",
                        "!safari",
                        "'geckoview",
                        "'live",
                        "'profil",
                    ],
                },
                "Bytecode Cached android fenix no-fission+live-sites+profiling": {
                    "raptor": [
                        "'browsertime 'bytecode",
                        "'android 'a51 'shippable 'aarch64",
                        "!chrom",
                        "!safari",
                        "'fenix",
                        "'nofis",
                        "'live",
                        "'profil",
                    ],
                },
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
            70,
            {
                "Pageload android-a51 fenix": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'fenix",
                    ],
                },
                "Pageload android-a51 fenix no-fission": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'fenix",
                        "'nofis",
                    ],
                },
                "Pageload (essential) android fenix no-fission+live-sites": {
                    "raptor": [
                        "'browsertime 'tp6 'essential",
                        "'android 'a51 'shippable 'aarch64",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'fenix",
                        "'nofis",
                        "'live",
                    ],
                },
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
            98,
            {
                "Pageload android-a51 fenix": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'fenix",
                    ],
                },
                "Pageload android-a51 fenix no-fission": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'fenix",
                        "'nofis",
                    ],
                },
                "Pageload android-a51 fenix live-sites": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'fenix",
                        "'live",
                    ],
                },
                "Pageload (essential) android fenix no-fission+live-sites": {
                    "raptor": [
                        "'browsertime 'tp6 'essential",
                        "'android 'a51 'shippable 'aarch64",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "'fenix",
                        "'nofis",
                        "'live",
                    ],
                },
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
            19,
            {
                "Responsiveness windows firefox": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "!-32 'windows 'shippable",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!chrom !geckoview !fenix !safari",
                    ],
                },
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
            83,
            {
                "Responsiveness windows firefox": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "!-32 'windows 'shippable",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!chrom !geckoview !fenix !safari",
                    ],
                },
                "Responsiveness windows firefox live-sites": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "!-32 'windows 'shippable",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!chrom !geckoview !fenix !safari",
                        "'live",
                    ],
                },
                "Graphics, & Media Playback windows live-sites+profiling+swr": {
                    "raptor": [
                        "'browsertime 'youtube-playback",
                        "!-32 'windows 'shippable",
                        "!chrom",
                        "!safari",
                        "'live",
                        "'profil",
                    ],
                    "talos": [
                        "'talos 'svgr | 'bcv | 'webgl",
                        "!-32 'windows 'shippable",
                        "'profil",
                        "'swr",
                    ],
                },
            },
            [
                "Benchmarks desktop",
                "Responsiveness windows firefox no-fisson",
                "Pageload (live) android",
                "Talos desktop live-sites",
                "Talos android",
            ],
        ),
    ],
)
def test_category_expansion(
    category_options, expected_counts, unique_categories, missing
):
    # Set the categories, and variants to expand
    ps.PerfParser.categories = TEST_CATEGORIES
    ps.PerfParser.variants = TEST_VARIANTS

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
            [7, 2, 2, 5],
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
            [7, 1, 1, 5],
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
