# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import pathlib
import shutil
import tempfile
from unittest import mock

import mozunit
import pytest
from tryselect.selectors.perf import (
    MAX_PERF_TASKS,
    Apps,
    InvalidCategoryException,
    InvalidRegressionDetectorQuery,
    PerfParser,
    Platforms,
    Suites,
    Variants,
    run,
)
from tryselect.selectors.perf_preview import plain_display
from tryselect.selectors.perfselector.classification import (
    check_for_live_sites,
    check_for_profile,
)

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
    # Bug 1837058 - Switch this back to Variants.NO_FISSION when
    # the default flips to fission on android
    Variants.FISSION.value: {
        "query": "'nofis",
        "negation": "!nofis",
        "platforms": [Platforms.ANDROID.value],
        "apps": [Apps.FENIX.value, Apps.GECKOVIEW.value],
    },
    Variants.BYTECODE_CACHED.value: {
        "query": "'bytecode",
        "negation": "!bytecode",
        "platforms": [Platforms.DESKTOP.value],
        "apps": [Apps.FIREFOX.value],
    },
    Variants.LIVE_SITES.value: {
        "query": "'live",
        "negation": "!live",
        "restriction": check_for_live_sites,
        "platforms": [Platforms.DESKTOP.value, Platforms.ANDROID.value],
        "apps": list(PerfParser.apps.keys()),
    },
    Variants.PROFILING.value: {
        "query": "'profil",
        "negation": "!profil",
        "restriction": check_for_profile,
        "platforms": [Platforms.DESKTOP.value, Platforms.ANDROID.value],
        "apps": [Apps.FIREFOX.value, Apps.GECKOVIEW.value, Apps.FENIX.value],
    },
    Variants.SWR.value: {
        "query": "'swr",
        "negation": "!swr",
        "platforms": [Platforms.DESKTOP.value],
        "apps": [Apps.FIREFOX.value],
    },
}

TEST_CATEGORIES = {
    "Pageload": {
        "query": {
            Suites.RAPTOR.value: ["'browsertime 'tp6"],
        },
        "suites": [Suites.RAPTOR.value],
        "tasks": [],
        "description": "",
    },
    "Pageload (essential)": {
        "query": {
            Suites.RAPTOR.value: ["'browsertime 'tp6 'essential"],
        },
        "variant-restrictions": {Suites.RAPTOR.value: [Variants.FISSION.value]},
        "suites": [Suites.RAPTOR.value],
        "tasks": [],
        "description": "",
    },
    "Responsiveness": {
        "query": {
            Suites.RAPTOR.value: ["'browsertime 'responsive"],
        },
        "suites": [Suites.RAPTOR.value],
        "variant-restrictions": {Suites.RAPTOR.value: []},
        "tasks": [],
        "description": "",
    },
    "Benchmarks": {
        "query": {
            Suites.RAPTOR.value: ["'browsertime 'benchmark"],
        },
        "suites": [Suites.RAPTOR.value],
        "variant-restrictions": {Suites.RAPTOR.value: []},
        "tasks": [],
        "description": "",
    },
    "DAMP (Devtools)": {
        "query": {
            Suites.TALOS.value: ["'talos 'damp"],
        },
        "suites": [Suites.TALOS.value],
        "tasks": [],
        "description": "",
    },
    "Talos PerfTests": {
        "query": {
            Suites.TALOS.value: ["'talos"],
        },
        "suites": [Suites.TALOS.value],
        "tasks": [],
        "description": "",
    },
    "Resource Usage": {
        "query": {
            Suites.TALOS.value: ["'talos 'xperf | 'tp5"],
            Suites.RAPTOR.value: ["'power 'osx"],
            Suites.AWSY.value: ["'awsy"],
        },
        "suites": [Suites.TALOS.value, Suites.RAPTOR.value, Suites.AWSY.value],
        "platform-restrictions": [Platforms.DESKTOP.value],
        "variant-restrictions": {
            Suites.RAPTOR.value: [],
            Suites.TALOS.value: [],
        },
        "app-restrictions": {
            Suites.RAPTOR.value: [Apps.FIREFOX.value],
            Suites.TALOS.value: [Apps.FIREFOX.value],
        },
        "tasks": [],
        "description": "",
    },
    "Graphics, & Media Playback": {
        "query": {
            # XXX This might not be an exhaustive list for talos atm
            Suites.TALOS.value: ["'talos 'svgr | 'bcv | 'webgl"],
            Suites.RAPTOR.value: ["'browsertime 'youtube-playback"],
        },
        "suites": [Suites.TALOS.value, Suites.RAPTOR.value],
        "variant-restrictions": {Suites.RAPTOR.value: [Variants.FISSION.value]},
        "tasks": [],
        "description": "",
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
            58,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ]
                },
                "Pageload macosx": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'osx 'shippable",
                        "!bytecode",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ]
                },
                "Resource Usage desktop": {
                    "awsy": ["'awsy", "!android 'shippable !-32 !clang"],
                    "raptor": [
                        "'power 'osx",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ],
                    "talos": [
                        "'talos 'xperf | 'tp5",
                        "!android 'shippable !-32 !clang",
                        "!profil",
                        "!swr",
                    ],
                },
            },
            [
                "Responsiveness android-p2 geckoview",
                "Benchmarks desktop chromium",
            ],
        ),  # Default settings
        (
            {"live_sites": True},
            66,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ]
                },
                "Pageload macosx": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'osx 'shippable",
                        "!bytecode",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ]
                },
                "Pageload macosx live-sites": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'osx 'shippable",
                        "'live",
                        "!bytecode",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ],
                },
            },
            [
                "Responsiveness android-p2 geckoview",
                "Benchmarks desktop chromium",
                "Benchmarks desktop firefox profiling",
                "Talos desktop live-sites",
                "Talos desktop profiling+swr",
                "Benchmarks desktop firefox live-sites+profiling"
                "Benchmarks desktop firefox live-sites",
            ],
        ),
        (
            {"live_sites": True, "safari": True},
            72,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!profil",
                        "!chrom",
                        "!m-car",
                    ]
                },
                "Pageload macosx safari": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'osx 'shippable",
                        "'safari",
                        "!bytecode",
                        "!profil",
                    ]
                },
                "Pageload macosx safari live-sites": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'osx 'shippable",
                        "'safari",
                        "'live",
                        "!bytecode",
                        "!profil",
                    ],
                },
            },
            [
                "Pageload linux safari",
                "Pageload desktop safari",
            ],
        ),
        (
            {"live_sites": True, "chrome": True},
            114,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!profil",
                        "!safari",
                        "!m-car",
                    ]
                },
                "Pageload macosx live-sites": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'osx 'shippable",
                        "'live",
                        "!bytecode",
                        "!profil",
                        "!safari",
                        "!m-car",
                    ],
                },
                "Benchmarks desktop chromium": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "'chromium",
                        "!bytecode",
                        "!profil",
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
            88,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!live",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ],
                },
                "Responsiveness android-a51 geckoview": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "'android 'a51 'shippable 'aarch64",
                        "'geckoview",
                        "!nofis",
                        "!live",
                        "!profil",
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
            138,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!live",
                        "!profil",
                        "!safari",
                        "!m-car",
                    ],
                },
                "Responsiveness android-a51 chrome-m": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "'android 'a51 'shippable 'aarch64",
                        "'chrome-m",
                        "!nofis",
                        "!live",
                        "!profil",
                    ],
                },
            },
            ["Responsiveness android-p2 chrome-m", "Resource Usage android"],
        ),
        (
            {"android": True, "chrome": True, "profile": True},
            176,
            {
                "Benchmarks desktop": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!android 'shippable !-32 !clang",
                        "!bytecode",
                        "!live",
                        "!safari",
                        "!m-car",
                    ]
                },
                "Talos PerfTests desktop profiling": {
                    "talos": [
                        "'talos",
                        "!android 'shippable !-32 !clang",
                        "'profil",
                        "!swr",
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
            14,
            {
                "Benchmarks windows firefox": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "!-32 'windows 'shippable",
                        "!chrom !geckoview !fenix !safari !m-car",
                        "!bytecode",
                        "!live",
                        "!profil",
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
            10,
            {
                "Pageload android fenix": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "!nofis",
                        "!live",
                        "!profil",
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
            15,
            {
                "Benchmarks android geckoview": {
                    "raptor": [
                        "'browsertime 'benchmark",
                        "'android 'a51 'shippable 'aarch64",
                        "'geckoview",
                        "!nofis",
                        "!live",
                        "!profil",
                    ],
                },
                "Pageload android fenix": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "!nofis",
                        "!live",
                        "!profil",
                    ],
                },
            },
            [
                "Benchmarks desktop",
                "Pageload android no-fission",
                "Pageload android fenix live-sites",
            ],
        ),
        # Variants are inclusive, so we'll see the variant alongside the
        # base here for fenix
        (
            {
                "requested_variants": ["fission"],
                "requested_apps": ["fenix"],
                "android": True,
            },
            32,
            {
                "Pageload android-a51 fenix": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "!live",
                        "!profil",
                    ],
                },
                "Pageload android-a51 fenix fission": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "'nofis",
                        "!live",
                        "!profil",
                    ],
                },
                "Pageload (essential) android fenix fission": {
                    "raptor": [
                        "'browsertime 'tp6 'essential",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "'nofis",
                        "!live",
                        "!profil",
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
                "requested_variants": ["fission", "live-sites"],
                "requested_apps": ["fenix"],
                "android": True,
            },
            40,
            {
                "Pageload android-a51 fenix": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "!profil",
                    ],
                },
                "Pageload android-a51 fenix fission": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "'nofis",
                        "!live",
                        "!profil",
                    ],
                },
                "Pageload android-a51 fenix live-sites": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "'live",
                        "!nofis",
                        "!profil",
                    ],
                },
                "Pageload (essential) android fenix fission": {
                    "raptor": [
                        "'browsertime 'tp6 'essential",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "'nofis",
                        "!live",
                        "!profil",
                    ],
                },
                "Pageload android fenix fission+live-sites": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "'android 'a51 'shippable 'aarch64",
                        "'fenix",
                        "'nofis",
                        "'live",
                        "!profil",
                    ],
                },
            },
            [
                "Benchmarks desktop",
                "Pageload (live) android",
                "Pageload android-p2 fenix live-sites",
                "Pageload (essential) android fenix no-fission+live-sites",
            ],
        ),
        # Make sure that no no-fission tasks are selected when a variant cannot
        # run on a requested platform
        (
            {
                "requested_variants": ["no-fission"],
                "requested_platforms": ["windows"],
            },
            14,
            {
                "Responsiveness windows firefox": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "!-32 'windows 'shippable",
                        "!chrom !geckoview !fenix !safari !m-car",
                        "!bytecode",
                        "!live",
                        "!profil",
                    ],
                },
            },
            ["Benchmarks desktop", "Responsiveness windows firefox no-fisson"],
        ),
        # We should only see the base and the live-site variants here for windows
        (
            {
                "requested_variants": ["no-fission", "live-sites"],
                "requested_platforms": ["windows"],
                "android": True,
            },
            16,
            {
                "Responsiveness windows firefox": {
                    "raptor": [
                        "'browsertime 'responsive",
                        "!-32 'windows 'shippable",
                        "!chrom !geckoview !fenix !safari !m-car",
                        "!bytecode",
                        "!profil",
                    ],
                },
                "Pageload windows live-sites": {
                    "raptor": [
                        "'browsertime 'tp6",
                        "!-32 'windows 'shippable",
                        "'live",
                        "!bytecode",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ],
                },
                "Graphics, & Media Playback windows": {
                    "raptor": [
                        "'browsertime 'youtube-playback",
                        "!-32 'windows 'shippable",
                        "!bytecode",
                        "!profil",
                        "!chrom",
                        "!safari",
                        "!m-car",
                    ],
                    "talos": [
                        "'talos 'svgr | 'bcv | 'webgl",
                        "!-32 'windows 'shippable",
                        "!profil",
                        "!swr",
                    ],
                },
            },
            [
                "Benchmarks desktop",
                "Responsiveness windows firefox no-fisson",
                "Pageload (live) android",
                "Talos desktop live-sites",
                "Talos android",
                "Graphics, & Media Playback windows live-sites",
                "Graphics, & Media Playback android no-fission",
            ],
        ),
    ],
)
def test_category_expansion(
    category_options, expected_counts, unique_categories, missing
):
    # Set the categories, and variants to expand
    PerfParser.categories = TEST_CATEGORIES
    PerfParser.variants = TEST_VARIANTS

    # Expand the categories, then either check if the unique_categories,
    # exist or are missing from the categories
    expanded_cats = PerfParser.get_categories(**category_options)

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
            [9, 2, 2, 10, 2, 1],
            2,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=revision&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"query": "'Pageload 'linux 'firefox"},
            [9, 2, 2, 10, 2, 1],
            2,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=revision&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"cached_revision": "cached_base_revision"},
            [9, 1, 1, 10, 2, 0],
            2,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=cached_base_revision&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"dry_run": True},
            [9, 1, 1, 10, 2, 0],
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
            [1, 2, 2, 8, 2, 1],
            0,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=revision&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"show_all": True, "query": "'shippable !32 speedometer 'firefox"},
            [1, 2, 2, 8, 2, 1],
            0,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=revision&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"single_run": True},
            [9, 1, 1, 4, 2, 0],
            2,
            (
                "If you need any help, you can find us in the #perf-help Matrix channel:\n"
                "https://matrix.to/#/#perf-help:mozilla.org\n"
            ),
        ),
        (
            {"detect_changes": True},
            [10, 2, 2, 10, 2, 1],
            2,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://treeherder.mozilla.org/perfherder/compare?originalProject=try&original"
                "Revision=revision&newProject=try&newRevision=revision\n"
            ),
        ),
        (
            {"perfcompare_beta": True},
            [9, 2, 2, 10, 2, 1],
            2,
            (
                "\n!!!NOTE!!!\n You'll be able to find a performance comparison "
                "here once the tests are complete (ensure you select the right framework): "
                "https://beta--mozilla-perfcompare.netlify.app/#/compare-results?"
                "revs=revision,revision&repos=try,try\n"
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
        "tryselect.selectors.perf.PerfParser.check_cached_revision",
    ) as ccr, mock.patch(
        "tryselect.selectors.perf.PerfParser.save_revision_treeherder"
    ) as srt, mock.patch(
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
            ["", TASKS],
            ["", TASKS],
            ["", ["Perftest Change Detector"]],
        ]
        ccr.return_value = options.get("cached_revision", "")

        run(**options)

        assert fzf.call_count == call_counts[0]
        assert ptt.call_count == call_counts[1]
        assert logger.call_count == call_counts[2]
        assert perf_print.call_count == call_counts[3]
        assert ccr.call_count == call_counts[4]
        assert srt.call_count == call_counts[5]
        assert perf_print.call_args_list[log_ind][0][0] == expected_log_message


@pytest.mark.parametrize(
    "options, call_counts, log_ind, expected_log_message, expected_failure",
    [
        (
            {"detect_changes": True},
            [10, 0, 0, 2, 1],
            1,
            (
                "Executing raptor queries: 'browsertime 'benchmark, !clang 'linux "
                "'shippable, !bytecode, !live, !profil, !chrom, !safari, !m-car"
            ),
            InvalidRegressionDetectorQuery,
        ),
    ],
)
@pytest.mark.skipif(os.name == "nt", reason="fzf not installed on host")
def test_change_detection_task_injection_failure(
    options,
    call_counts,
    log_ind,
    expected_log_message,
    expected_failure,
):
    with mock.patch("tryselect.selectors.perf.push_to_try") as ptt, mock.patch(
        "tryselect.selectors.perf.run_fzf"
    ) as fzf, mock.patch(
        "tryselect.selectors.perf.get_repository_object", new=mock.MagicMock()
    ), mock.patch(
        "tryselect.selectors.perf.LogProcessor.revision",
        new_callable=mock.PropertyMock,
        return_value="revision",
    ) as logger, mock.patch(
        "tryselect.selectors.perf.PerfParser.check_cached_revision"
    ) as ccr, mock.patch(
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
            ["", TASKS],
            ["", TASKS],
            ["", TASKS],
        ]

        with pytest.raises(expected_failure):
            run(**options)

        assert fzf.call_count == call_counts[0]
        assert ptt.call_count == call_counts[1]
        assert logger.call_count == call_counts[2]
        assert perf_print.call_count == call_counts[3]
        assert ccr.call_count == call_counts[4]
        assert perf_print.call_args_list[log_ind][0][0] == expected_log_message


@pytest.mark.parametrize(
    "query, should_fail",
    [
        (
            {
                "query": {
                    # Raptor has all variants available so it
                    # should fail on this category
                    "raptor": ["browsertime 'live 'no-fission"],
                }
            },
            True,
        ),
        (
            {
                "query": {
                    # Awsy has no variants defined so it shouldn't fail
                    # on a query like this
                    "awsy": ["browsertime 'live 'no-fission"],
                }
            },
            False,
        ),
    ],
)
def test_category_rules(query, should_fail):
    # Set the categories, and variants to expand
    PerfParser.categories = {"test-live": query}
    PerfParser.variants = TEST_VARIANTS

    if should_fail:
        with pytest.raises(InvalidCategoryException):
            PerfParser.run_category_checks()
    else:
        assert PerfParser.run_category_checks()

    # Reset the categories, and variants to expand
    PerfParser.categories = TEST_CATEGORIES
    PerfParser.variants = TEST_VARIANTS


@pytest.mark.parametrize(
    "apk_name, apk_content, should_fail, failure_message",
    [
        (
            "real-file",
            "file-content",
            False,
            None,
        ),
        ("bad-file", None, True, "Path does not exist:"),
    ],
)
def test_apk_upload(apk_name, apk_content, should_fail, failure_message):
    with mock.patch("tryselect.selectors.perf.subprocess") as _, mock.patch(
        "tryselect.selectors.perf.shutil"
    ) as _:
        temp_dir = None
        try:
            temp_dir = tempfile.mkdtemp()
            sample_apk = pathlib.Path(temp_dir, apk_name)
            if apk_content is not None:
                with sample_apk.open("w") as f:
                    f.write(apk_content)

            if should_fail:
                with pytest.raises(Exception) as exc_info:
                    PerfParser.setup_apk_upload("browsertime", str(sample_apk))
                assert failure_message in str(exc_info)
            else:
                PerfParser.setup_apk_upload("browsertime", str(sample_apk))
        finally:
            if temp_dir is not None:
                shutil.rmtree(temp_dir)


@pytest.mark.parametrize(
    "args, load_data, return_value, call_counts, exists_cache_file",
    [
        (
            (
                [],
                "base_commit",
            ),
            {
                "base_commit": [
                    {
                        "base_revision_treeherder": "2b04563b5",
                        "date": "2023-03-31",
                        "tasks": [],
                    },
                ],
            },
            "2b04563b5",
            [1, 0],
            True,
        ),
        (
            (
                ["task-a"],
                "subset_base_commit",
            ),
            {
                "subset_base_commit": [
                    {
                        "base_revision_treeherder": "2b04563b5",
                        "date": "2023-03-31",
                        "tasks": ["task-a", "task-b"],
                    },
                ],
            },
            "2b04563b5",
            [1, 0],
            True,
        ),
        (
            ([], "not_exist_cached_base_commit"),
            {
                "base_commit": [
                    {
                        "base_revision_treeherder": "2b04563b5",
                        "date": "2023-03-31",
                        "tasks": [],
                    },
                ],
            },
            None,
            [1, 0],
            True,
        ),
        (
            (
                ["task-a", "task-b"],
                "superset_base_commit",
            ),
            {
                "superset_base_commit": [
                    {
                        "base_revision_treeherder": "2b04563b5",
                        "date": "2023-03-31",
                        "tasks": ["task-a"],
                    },
                ],
            },
            None,
            [1, 0],
            True,
        ),
        (
            ([], None),
            {},
            None,
            [1, 1],
            True,
        ),
        (
            ([], None),
            {},
            None,
            [0, 0],
            False,
        ),
    ],
)
def test_check_cached_revision(
    args, load_data, return_value, call_counts, exists_cache_file
):
    with mock.patch("tryselect.selectors.perf.json.load") as load, mock.patch(
        "tryselect.selectors.perf.json.dump"
    ) as dump, mock.patch(
        "tryselect.selectors.perf.pathlib.Path.is_file"
    ) as is_file, mock.patch(
        "tryselect.selectors.perf.pathlib.Path.open"
    ):
        load.return_value = load_data
        is_file.return_value = exists_cache_file
        result = PerfParser.check_cached_revision(*args)

        assert load.call_count == call_counts[0]
        assert dump.call_count == call_counts[1]
        assert result == return_value


@pytest.mark.parametrize(
    "args, call_counts, exists_cache_file",
    [
        (
            ["base_commit", "base_revision_treeherder"],
            [0, 1],
            False,
        ),
        (
            ["base_commit", "base_revision_treeherder"],
            [1, 1],
            True,
        ),
    ],
)
def test_save_revision_treeherder(args, call_counts, exists_cache_file):
    with mock.patch("tryselect.selectors.perf.json.load") as load, mock.patch(
        "tryselect.selectors.perf.json.dump"
    ) as dump, mock.patch(
        "tryselect.selectors.perf.pathlib.Path.is_file"
    ) as is_file, mock.patch(
        "tryselect.selectors.perf.pathlib.Path.open"
    ):
        is_file.return_value = exists_cache_file
        PerfParser.save_revision_treeherder(TASKS, args[0], args[1])

        assert load.call_count == call_counts[0]
        assert dump.call_count == call_counts[1]


@pytest.mark.parametrize(
    "total_tasks, options, call_counts, expected_log_message, expected_failure",
    [
        (
            MAX_PERF_TASKS + 1,
            {},
            [1, 0, 0, 1],
            (
                "\n\n----------------------------------------------------------------------------------------------\n"
                f"You have selected {MAX_PERF_TASKS+1} total test runs! (selected tasks({MAX_PERF_TASKS+1}) * rebuild"
                f" count(1) \nThese tests won't be triggered as the current maximum for a single ./mach try "
                f"perf run is {MAX_PERF_TASKS}. \nIf this was unexpected, please file a bug in Testing :: Performance."
                "\n----------------------------------------------------------------------------------------------\n\n"
            ),
            True,
        ),
        (
            MAX_PERF_TASKS,
            {"show_all": True},
            [9, 0, 0, 8],
            (
                "For more information on the performance tests, see our "
                "PerfDocs here:\nhttps://firefox-source-docs.mozilla.org/testing/perfdocs/"
            ),
            False,
        ),
        (
            int((MAX_PERF_TASKS + 2) / 2),
            {"show_all": True, "try_config": {"rebuild": 2}},
            [1, 0, 0, 1],
            (
                "\n\n----------------------------------------------------------------------------------------------\n"
                f"You have selected {int((MAX_PERF_TASKS + 2) / 2) * 2} total test runs! (selected tasks("
                f"{int((MAX_PERF_TASKS + 2) / 2)}) * rebuild"
                f" count(2) \nThese tests won't be triggered as the current maximum for a single ./mach try "
                f"perf run is {MAX_PERF_TASKS}. \nIf this was unexpected, please file a bug in Testing :: Performance."
                "\n----------------------------------------------------------------------------------------------\n\n"
            ),
            True,
        ),
        (0, {}, [1, 0, 0, 1], ("No tasks selected"), True),
    ],
)
def test_max_perf_tasks(
    total_tasks,
    options,
    call_counts,
    expected_log_message,
    expected_failure,
):
    # Set the categories, and variants to expand
    PerfParser.categories = TEST_CATEGORIES
    PerfParser.variants = TEST_VARIANTS

    with mock.patch("tryselect.selectors.perf.push_to_try") as ptt, mock.patch(
        "tryselect.selectors.perf.print",
    ) as perf_print, mock.patch(
        "tryselect.selectors.perf.LogProcessor.revision",
        new_callable=mock.PropertyMock,
        return_value="revision",
    ), mock.patch(
        "tryselect.selectors.perf.PerfParser.perf_push_to_try",
        new_callable=mock.MagicMock,
        return_value=("revision1", "revision2"),
    ) as perf_push_to_try_mock, mock.patch(
        "tryselect.selectors.perf.PerfParser.get_perf_tasks"
    ) as get_perf_tasks_mock, mock.patch(
        "tryselect.selectors.perf.PerfParser.get_tasks"
    ) as get_tasks_mock, mock.patch(
        "tryselect.selectors.perf.run_fzf"
    ) as fzf, mock.patch(
        "tryselect.selectors.perf.fzf_bootstrap", return_value=mock.MagicMock()
    ):
        tasks = ["a-task"] * total_tasks
        get_tasks_mock.return_value = tasks
        get_perf_tasks_mock.return_value = tasks, [], []

        run(**options)

        assert perf_push_to_try_mock.call_count == 0 if expected_failure else 1
        assert ptt.call_count == call_counts[1]
        assert perf_print.call_count == call_counts[3]
        assert fzf.call_count == 0
        assert perf_print.call_args_list[-1][0][0] == expected_log_message


@pytest.mark.parametrize(
    "try_config, selected_tasks, expected_try_config",
    [
        (
            {"use-artifact-builds": True},
            ["some-android-task"],
            {"use-artifact-builds": False},
        ),
        (
            {"use-artifact-builds": True},
            ["some-desktop-task"],
            {"use-artifact-builds": True},
        ),
        (
            {"use-artifact-builds": False},
            ["some-android-task"],
            {"use-artifact-builds": False},
        ),
        (
            {"use-artifact-builds": True},
            ["some-desktop-task", "some-android-task"],
            {"use-artifact-builds": False},
        ),
    ],
)
def test_artifact_mode_autodisable(try_config, selected_tasks, expected_try_config):
    PerfParser.setup_try_config(try_config, [], selected_tasks)
    assert (
        try_config["use-artifact-builds"] == expected_try_config["use-artifact-builds"]
    )


def test_build_category_description():
    base_cmd = ["--preview", '-t "{+f}"']

    with mock.patch("tryselect.selectors.perf.json.dump") as dump:
        PerfParser.build_category_description(base_cmd, "")

        assert dump.call_count == 1
        assert str(base_cmd).count("-d") == 1
        assert str(base_cmd).count("-l") == 1


@pytest.mark.parametrize(
    "options, call_count",
    [
        ({}, [1, 1, 2]),
        ({"show_all": True}, [0, 0, 1]),
    ],
)
def test_preview_description(options, call_count):
    with mock.patch("tryselect.selectors.perf.PerfParser.perf_push_to_try"), mock.patch(
        "tryselect.selectors.perf.fzf_bootstrap"
    ), mock.patch(
        "tryselect.selectors.perf.PerfParser.get_perf_tasks"
    ) as get_perf_tasks, mock.patch(
        "tryselect.selectors.perf.PerfParser.get_tasks"
    ), mock.patch(
        "tryselect.selectors.perf.PerfParser.build_category_description"
    ) as bcd:
        get_perf_tasks.return_value = [], [], []

        run(**options)

        assert bcd.call_count == call_count[0]

    base_cmd = ["--preview", '-t "{+f}"']
    option = base_cmd[base_cmd.index("--preview") + 1].split(" ")
    description, line = None, None
    if call_count[0] == 1:
        PerfParser.build_category_description(base_cmd, "")
        option = base_cmd[base_cmd.index("--preview") + 1].split(" ")
        description = option[option.index("-d") + 1]
        line = "Current line"

    taskfile = option[option.index("-t") + 1]

    with mock.patch("tryselect.selectors.perf_preview.open"), mock.patch(
        "tryselect.selectors.perf_preview.pathlib.Path.open"
    ), mock.patch("tryselect.selectors.perf_preview.json.load") as load, mock.patch(
        "tryselect.selectors.perf_preview.print"
    ) as preview_print:
        load.return_value = {line: "test description"}

        plain_display(taskfile, description, line)

        assert load.call_count == call_count[1]
        assert preview_print.call_count == call_count[2]


if __name__ == "__main__":
    mozunit.main()
