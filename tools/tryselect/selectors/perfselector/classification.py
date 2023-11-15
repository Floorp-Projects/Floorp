# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import enum


class ClassificationEnum(enum.Enum):
    """This class provides the ability to use Enums as array indices."""

    @property
    def value(self):
        return self._value_["value"]

    def __index__(self):
        return self._value_["index"]

    def __int__(self):
        return self._value_["index"]


class Platforms(ClassificationEnum):
    ANDROID_A51 = {"value": "android-a51", "index": 0}
    ANDROID = {"value": "android", "index": 1}
    WINDOWS = {"value": "windows", "index": 2}
    LINUX = {"value": "linux", "index": 3}
    MACOSX = {"value": "macosx", "index": 4}
    DESKTOP = {"value": "desktop", "index": 5}


class Apps(ClassificationEnum):
    FIREFOX = {"value": "firefox", "index": 0}
    CHROME = {"value": "chrome", "index": 1}
    CHROMIUM = {"value": "chromium", "index": 2}
    GECKOVIEW = {"value": "geckoview", "index": 3}
    FENIX = {"value": "fenix", "index": 4}
    CHROME_M = {"value": "chrome-m", "index": 5}
    SAFARI = {"value": "safari", "index": 6}
    CHROMIUM_RELEASE = {"value": "custom-car", "index": 7}
    CHROMIUM_RELEASE_M = {"value": "cstm-car-m", "index": 8}


class Suites(ClassificationEnum):
    RAPTOR = {"value": "raptor", "index": 0}
    TALOS = {"value": "talos", "index": 1}
    AWSY = {"value": "awsy", "index": 2}


class Variants(ClassificationEnum):
    FISSION = {"value": "fission", "index": 0}
    BYTECODE_CACHED = {"value": "bytecode-cached", "index": 1}
    LIVE_SITES = {"value": "live-sites", "index": 2}
    PROFILING = {"value": "profiling", "index": 3}
    SWR = {"value": "swr", "index": 4}


"""
The following methods and constants are used for restricting
certain platforms and applications such as chrome, safari, and
android tests. These all require a flag such as --android to
enable (see build_category_matrix for more info).
"""


def check_for_android(android=False, **kwargs):
    return android


def check_for_chrome(chrome=False, **kwargs):
    return chrome


def check_for_custom_car(custom_car=False, **kwargs):
    return custom_car


def check_for_safari(safari=False, **kwargs):
    return safari


def check_for_live_sites(live_sites=False, **kwargs):
    return live_sites


def check_for_profile(profile=False, **kwargs):
    return profile


class ClassificationProvider:
    @property
    def platforms(self):
        return {
            Platforms.ANDROID_A51.value: {
                "query": "'android 'a51 'shippable 'aarch64",
                "restriction": check_for_android,
                "platform": Platforms.ANDROID.value,
            },
            Platforms.ANDROID.value: {
                # The android, and android-a51 queries are expected to be the same,
                # we don't want to run the tests on other mobile platforms.
                "query": "'android 'a51 'shippable 'aarch64",
                "restriction": check_for_android,
                "platform": Platforms.ANDROID.value,
            },
            Platforms.WINDOWS.value: {
                "query": "!-32 'windows 'shippable",
                "platform": Platforms.DESKTOP.value,
            },
            Platforms.LINUX.value: {
                "query": "!clang 'linux 'shippable",
                "platform": Platforms.DESKTOP.value,
            },
            Platforms.MACOSX.value: {
                "query": "'osx 'shippable",
                "platform": Platforms.DESKTOP.value,
            },
            Platforms.DESKTOP.value: {
                "query": "!android 'shippable !-32 !clang",
                "platform": Platforms.DESKTOP.value,
            },
        }

    @property
    def apps(self):
        return {
            Apps.FIREFOX.value: {
                "query": "!chrom !geckoview !fenix !safari !m-car",
                "platforms": [Platforms.DESKTOP.value],
            },
            Apps.CHROME.value: {
                "query": "'chrome",
                "negation": "!chrom",
                "restriction": check_for_chrome,
                "platforms": [Platforms.DESKTOP.value],
            },
            Apps.CHROMIUM.value: {
                "query": "'chromium",
                "negation": "!chrom",
                "restriction": check_for_chrome,
                "platforms": [Platforms.DESKTOP.value],
            },
            Apps.GECKOVIEW.value: {
                "query": "'geckoview",
                "platforms": [Platforms.ANDROID.value],
            },
            Apps.FENIX.value: {
                "query": "'fenix",
                "platforms": [Platforms.ANDROID.value],
            },
            Apps.CHROME_M.value: {
                "query": "'chrome-m",
                "negation": "!chrom",
                "restriction": check_for_chrome,
                "platforms": [Platforms.ANDROID.value],
            },
            Apps.SAFARI.value: {
                "query": "'safari",
                "negation": "!safari",
                "restriction": check_for_safari,
                "platforms": [Platforms.MACOSX.value],
            },
            Apps.CHROMIUM_RELEASE.value: {
                "query": "'m-car",
                "negation": "!m-car",
                "restriction": check_for_custom_car,
                "platforms": [
                    Platforms.LINUX.value,
                    Platforms.WINDOWS.value,
                    Platforms.MACOSX.value,
                ],
            },
            Apps.CHROMIUM_RELEASE_M.value: {
                "query": "'m-car",
                "negation": "!m-car",
                "restriction": check_for_custom_car,
                "platforms": [Platforms.ANDROID.value],
            },
        }

    @property
    def variants(self):
        return {
            Variants.FISSION.value: {
                "query": "!nofis",
                "negation": "'nofis",
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
                "apps": [  # XXX No live CaR tests
                    Apps.FIREFOX.value,
                    Apps.CHROME.value,
                    Apps.CHROMIUM.value,
                    Apps.FENIX.value,
                    Apps.GECKOVIEW.value,
                    Apps.SAFARI.value,
                ],
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

    @property
    def suites(self):
        return {
            Suites.RAPTOR.value: {
                "apps": list(self.apps.keys()),
                "platforms": list(self.platforms.keys()),
                "variants": [
                    Variants.FISSION.value,
                    Variants.LIVE_SITES.value,
                    Variants.PROFILING.value,
                    Variants.BYTECODE_CACHED.value,
                ],
            },
            Suites.TALOS.value: {
                "apps": [Apps.FIREFOX.value],
                "platforms": [Platforms.DESKTOP.value],
                "variants": [
                    Variants.PROFILING.value,
                    Variants.SWR.value,
                ],
            },
            Suites.AWSY.value: {
                "apps": [Apps.FIREFOX.value],
                "platforms": [Platforms.DESKTOP.value],
                "variants": [],
            },
        }

    """
    Here you can find the base categories that are defined for the perf
    selector. The following fields are available:
        * query: Set the queries to use for each suite you need.
        * suites: The suites that are needed for this category.
        * tasks: A hard-coded list of tasks to select.
        * platforms: The platforms that it can run on.
        * app-restrictions: A list of apps that the category can run.
        * variant-restrictions: A list of variants available for each suite.

    Note that setting the App/Variant-Restriction fields should be used to
    restrict the available apps and variants, not expand them.
    """

    @property
    def categories(self):
        return {
            "Pageload": {
                "query": {
                    Suites.RAPTOR.value: ["'browsertime 'tp6"],
                },
                "suites": [Suites.RAPTOR.value],
                "tasks": [],
                "description": "Run all the pageload tests in warm, and cold. Used to determine "
                "if your patch has a direct impact on pageload performance.",
            },
            "Speedometer 3": {
                "query": {
                    Suites.RAPTOR.value: ["'browsertime 'speedometer3"],
                },
                "variant-restrictions": {Suites.RAPTOR.value: [Variants.FISSION.value]},
                "suites": [Suites.RAPTOR.value],
                "app-restrictions": {},
                "tasks": [],
                "description": "",
            },
            "Responsiveness": {
                "query": {
                    Suites.RAPTOR.value: ["'browsertime 'responsive"],
                },
                "suites": [Suites.RAPTOR.value],
                "variant-restrictions": {Suites.RAPTOR.value: []},
                "app-restrictions": {
                    Suites.RAPTOR.value: [
                        Apps.FIREFOX.value,
                        Apps.CHROME.value,
                        Apps.CHROMIUM.value,
                        Apps.FENIX.value,
                        Apps.GECKOVIEW.value,
                    ],
                },
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
                "app-restrictions": {
                    Suites.RAPTOR.value: [
                        Apps.FIREFOX.value,
                        Apps.CHROME.value,
                        Apps.CHROMIUM.value,
                        Apps.FENIX.value,
                        Apps.GECKOVIEW.value,
                    ],
                },
                "tasks": [],
                "description": "",
            },
        }
