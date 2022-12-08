# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import enum
import itertools
import os
import re
import sys
from contextlib import redirect_stdout

from mozbuild.base import MozbuildObject
from mozversioncontrol import get_repository_object

from ..push import generate_try_task_config, push_to_try
from ..util.fzf import (
    FZF_NOT_FOUND,
    build_base_cmd,
    fzf_bootstrap,
    run_fzf,
    setup_tasks_for_fzf,
)
from .compare import CompareParser

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)

PERFHERDER_BASE_URL = (
    "https://treeherder.mozilla.org/perfherder/"
    "compare?originalProject=try&originalRevision=%s&newProject=try&newRevision=%s"
)

# Prevent users from running more than 300 tests at once. It's possible, but
# it's more likely that a query is broken and is selecting far too much.
MAX_PERF_TASKS = 300
REVISION_MATCHER = re.compile(r"remote:.*/try/rev/([\w]*)[ \t]*$")

# Name of the base category with no variants applied to it
BASE_CATEGORY_NAME = "base"


class InvalidCategoryException(Exception):
    """Thrown when a category is found to be invalid.

    See the `PerfParser.run_category_checks()` method for more info.
    """

    pass


class LogProcessor:
    def __init__(self):
        self.buf = ""
        self.stdout = sys.__stdout__
        self._revision = None

    @property
    def revision(self):
        return self._revision

    def write(self, buf):
        while buf:
            try:
                newline_index = buf.index("\n")
            except ValueError:
                # No newline, wait for next call
                self.buf += buf
                break

            # Get data up to next newline and combine with previously buffered data
            data = self.buf + buf[: newline_index + 1]
            buf = buf[newline_index + 1 :]

            # Reset buffer then output line
            self.buf = ""
            if data.strip() == "":
                continue
            self.stdout.write(data.strip("\n") + "\n")

            # Check if a temporary commit wa created
            match = REVISION_MATCHER.match(data)
            if match:
                # Last line found is the revision we want
                self._revision = match.group(1)


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


class Suites(ClassificationEnum):
    RAPTOR = {"value": "raptor", "index": 0}
    TALOS = {"value": "talos", "index": 1}
    AWSY = {"value": "awsy", "index": 2}


class Variants(ClassificationEnum):
    NO_FISSION = {"value": "no-fission", "index": 0}
    BYTECODE_CACHED = {"value": "bytecode-cached", "index": 1}
    LIVE_SITES = {"value": "live-sites", "index": 2}
    PROFILING = {"value": "profiling", "index": 3}
    SWR = {"value": "swr", "index": 4}


class PerfParser(CompareParser):
    name = "perf"
    common_groups = ["push", "task"]
    task_configs = [
        "artifact",
        "browsertime",
        "disable-pgo",
        "env",
        "gecko-profile",
        "path",
        "rebuild",
    ]

    platforms = {
        "android-a51": {
            "query": "'android 'a51 'shippable 'aarch64",
            "platform": "android",
        },
        "android": {
            # The android, and android-a51 queries are expected to be the same,
            # we don't want to run the tests on other mobile platforms.
            "query": "'android 'a51 'shippable 'aarch64",
            "platform": "android",
        },
        "windows": {
            "query": "!-32 'windows 'shippable",
            "platform": "desktop",
        },
        "linux": {
            "query": "!clang 'linux 'shippable",
            "platform": "desktop",
        },
        "macosx": {
            "query": "'osx 'shippable",
            "platform": "desktop",
        },
        "desktop": {
            "query": "!android 'shippable !-32 !clang",
            "platform": "desktop",
        },
    }

    apps = {
        "firefox": {
            "query": "!chrom !geckoview !fenix !safari",
            "platforms": ["desktop"],
        },
        "chrome": {
            "query": "'chrome",
            "platforms": ["desktop"],
        },
        "chromium": {
            "query": "'chromium",
            "platforms": ["desktop"],
        },
        "safari": {
            "query": "'safari",
            "platforms": ["macosx"],
        },
        "geckoview": {
            "query": "'geckoview",
            "platforms": ["android"],
        },
        "fenix": {
            "query": "'fenix",
            "platforms": ["android"],
        },
        "chrome-m": {
            "query": "'chrome-m",
            "platforms": ["android"],
        },
    }

    variants = {
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
            "apps": list(apps.keys()),
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
    categories = {
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

    suites = {
        "raptor": {
            "apps": list(apps.keys()),
            "platforms": list(platforms.keys()),
            "variants": [
                "no-fission",
                "live-sites",
                "profiling",
                "bytecode-cached",
            ],
        },
        "talos": {
            "apps": ["firefox"],
            "platforms": ["desktop"],
            "variants": [
                "profiling",
                "swr",
            ],
        },
        "awsy": {
            "apps": ["firefox"],
            "platforms": ["desktop"],
            "variants": [],
        },
    }

    arguments = [
        [
            ["--show-all"],
            {
                "action": "store_true",
                "default": False,
                "help": "Show all available tasks.",
            },
        ],
        [
            ["--android"],
            {
                "action": "store_true",
                "default": False,
                "help": "Show android test categories (disabled by default).",
            },
        ],
        [
            ["--chrome"],
            {
                "action": "store_true",
                "default": False,
                "help": "Show tests available for Chrome-based browsers "
                "(disabled by default).",
            },
        ],
        [
            ["--safari"],
            {
                "action": "store_true",
                "default": False,
                "help": "Show tests available for Safari (disabled by default).",
            },
        ],
        [
            ["--live-sites"],
            {
                "action": "store_true",
                "default": False,
                "help": "Run tasks with live sites (if possible). "
                "You can also use the `live-sites` variant.",
            },
        ],
        [
            ["--profile"],
            {
                "action": "store_true",
                "default": False,
                "help": "Run tasks with profiling (if possible). "
                "You can also use the `profiling` variant.",
            },
        ],
        [
            ["--variants"],
            {
                "nargs": "*",
                "type": str,
                "default": [],
                "dest": "requested_variants",
                "choices": list(variants.keys()),
                "help": "Select variants to display in the selector from: "
                + ", ".join(list(variants.keys())),
                "metavar": "",
            },
        ],
        [
            ["--platforms"],
            {
                "nargs": "*",
                "type": str,
                "default": [],
                "dest": "requested_platforms",
                "choices": list(platforms.keys()),
                "help": "Select specific platforms to target. Android only "
                "available with --android. Available platforms: "
                + ", ".join(list(platforms.keys())),
                "metavar": "",
            },
        ],
        [
            ["--apps"],
            {
                "nargs": "*",
                "type": str,
                "default": [],
                "dest": "requested_apps",
                "choices": list(apps.keys()),
                "help": "Select specific applications to target from: "
                + ", ".join(list(apps.keys())),
                "metavar": "",
            },
        ],
    ]

    def get_tasks(base_cmd, queries, query_arg=None, candidate_tasks=None):
        cmd = base_cmd[:]
        if query_arg:
            cmd.extend(["-f", query_arg])

        query_str, tasks = run_fzf(cmd, sorted(candidate_tasks))
        queries.append(query_str)
        return set(tasks)

    def get_perf_tasks(base_cmd, all_tg_tasks, perf_categories):
        # Convert the categories to tasks
        selected_tasks = set()
        queries = []

        selected_categories = PerfParser.get_tasks(
            base_cmd, queries, None, perf_categories
        )

        for category, category_info in perf_categories.items():
            if category not in selected_categories:
                continue
            print("Gathering tasks for %s category" % category)

            category_tasks = set()
            for suite in PerfParser.suites:
                # Either perform a query to get the tasks (recommended), or
                # use a hardcoded task list
                suite_queries = category_info["queries"].get(suite)

                category_suite_tasks = set()
                if suite_queries:
                    print(
                        "Executing %s queries: %s" % (suite, ", ".join(suite_queries))
                    )

                    for perf_query in suite_queries:
                        if not category_suite_tasks:
                            # Get all tasks selected with the first query
                            category_suite_tasks |= PerfParser.get_tasks(
                                base_cmd, queries, perf_query, all_tg_tasks
                            )
                        else:
                            # Keep only those tasks that matched in all previous queries
                            category_suite_tasks &= PerfParser.get_tasks(
                                base_cmd, queries, perf_query, category_suite_tasks
                            )

                        if len(category_suite_tasks) == 0:
                            print("Failed to find any tasks for query: %s" % perf_query)
                            break

                    if category_suite_tasks:
                        category_tasks |= category_suite_tasks

            if category_info["tasks"]:
                category_tasks = set(category_info["tasks"]) & all_tg_tasks
                if category_tasks != set(category_info["tasks"]):
                    print(
                        "Some expected tasks could not be found: %s"
                        % ", ".join(category_info["tasks"] - category_tasks)
                    )

            if not category_tasks:
                print("Could not find any tasks for category %s" % category)
            else:
                # Add the new tasks to the currently selected ones
                selected_tasks |= category_tasks

        if len(selected_tasks) > MAX_PERF_TASKS:
            print(
                "That's a lot of tests selected (%s)!\n"
                "These tests won't be triggered. If this was unexpected, "
                "please file a bug in Testing :: Performance." % MAX_PERF_TASKS
            )
            return [], [], []

        return selected_tasks, selected_categories, queries

    def _check_app(app, target):
        """Checks if the app exists in the target."""
        if app.value in target:
            return True
        return False

    def _check_platform(platform, target):
        """Checks if the platform, or it's type exists in the target."""
        if (
            platform.value in target
            or PerfParser.platforms[platform.value]["platform"] in target
        ):
            return True
        return False

    def _build_initial_decision_matrix():
        # Build first stage of matrix APPS X PLATFORMS
        initial_decision_matrix = []
        for platform in Platforms:
            platform_row = []
            for app in Apps:
                if PerfParser._check_platform(
                    platform, PerfParser.apps[app.value]["platforms"]
                ):
                    # This app can run on this platform
                    platform_row.append(True)
                else:
                    platform_row.append(False)
            initial_decision_matrix.append(platform_row)
        return initial_decision_matrix

    def _build_intermediate_decision_matrix():
        # Second stage of matrix building applies the 2D matrix found above
        # to each suite
        initial_decision_matrix = PerfParser._build_initial_decision_matrix()

        intermediate_decision_matrix = []
        for suite in Suites:
            suite_matrix = copy.deepcopy(initial_decision_matrix)
            suite_info = PerfParser.suites[suite.value]

            # Restric the platforms for this suite now
            for platform in Platforms:
                for app in Apps:
                    runnable = False
                    if PerfParser._check_app(
                        app, suite_info["apps"]
                    ) and PerfParser._check_platform(platform, suite_info["platforms"]):
                        runnable = True
                    suite_matrix[platform][app] = (
                        runnable and suite_matrix[platform][app]
                    )

            intermediate_decision_matrix.append(suite_matrix)
        return intermediate_decision_matrix

    def _build_variants_matrix():
        # Third stage is expanding the intermediate matrix
        # across all the variants (non-expanded). Start with the
        # intermediate matrix in the list since it provides our
        # base case with no variants
        intermediate_decision_matrix = PerfParser._build_intermediate_decision_matrix()

        variants_matrix = []
        for variant in Variants:
            variant_matrix = copy.deepcopy(intermediate_decision_matrix)

            for suite in Suites:
                if variant.value in PerfParser.suites[suite.value]["variants"]:
                    # Allow the variant through and set it's platforms and apps
                    # based on how it sets it -> only restrict, don't make allowances
                    # here
                    for platform in Platforms:
                        for app in Apps:
                            if not (
                                PerfParser._check_platform(
                                    platform,
                                    PerfParser.variants[variant.value]["platforms"],
                                )
                                and PerfParser._check_app(
                                    app, PerfParser.variants[variant.value]["apps"]
                                )
                            ):
                                variant_matrix[suite][platform][app] = False
                else:
                    # This variant matrix needs to be completely False
                    variant_matrix[suite] = [
                        [False] * len(platform_row)
                        for platform_row in variant_matrix[suite]
                    ]

            variants_matrix.append(variant_matrix)

        return variants_matrix, intermediate_decision_matrix

    def _build_decision_matrix():
        """Build the decision matrix.

        This method builds the decision matrix that is used
        to determine what categories will be shown to the user.
        This matrix has the following form (as lists):
            - Variants
                - Suites
                    - Platforms
                        - Apps

        Each element in the 4D Matrix is either True or False and tells us
        whether the particular combination is "runnable" according to
        the given specifications. This does not mean that the combination
        exists, just that it's fully configured in this selector.

        The ("base",) variant combination found in the matrix has
        no variants applied to it. At this stage, it's a catch-all for those
        categories. The query it uses is reduced further in later stages.
        """
        # Get the variants matrix (see methods above) and the intermediate decision
        # matrix to act as the base category
        (
            variants_matrix,
            intermediate_decision_matrix,
        ) = PerfParser._build_variants_matrix()

        # Get all possible combinations of the variants
        expanded_variants = [
            variant_combination
            for set_size in range(len(Variants) + 1)
            for variant_combination in itertools.combinations(list(Variants), set_size)
        ]

        # Final stage combines the intermediate matrix with the
        # expanded variants and leaves a "base" category which
        # doesn't have any variant specifications (it catches them all)
        decision_matrix = {(BASE_CATEGORY_NAME,): intermediate_decision_matrix}
        for variant_combination in expanded_variants:
            expanded_variant_matrix = []

            # Perform an AND operation on the combination of variants
            # to determine where this particular combination can run
            for suite in Suites:
                suite_matrix = []
                suite_variants = PerfParser.suites[suite.value]["variants"]

                # Disable the variant combination if none of them
                # are found in the suite
                disable_variant = not any(
                    [variant.value in suite_variants for variant in variant_combination]
                )

                for platform in Platforms:
                    if disable_variant:
                        platform_row = [False for _ in Apps]
                    else:
                        platform_row = [
                            all(
                                variants_matrix[variant][suite][platform][app]
                                for variant in variant_combination
                                if variant.value in suite_variants
                            )
                            for app in Apps
                        ]
                    suite_matrix.append(platform_row)

                expanded_variant_matrix.append(suite_matrix)
            decision_matrix[variant_combination] = expanded_variant_matrix

        return decision_matrix

    def _skip_with_restrictions(value, restrictions, requested=[]):
        """Determines if we should skip an app, platform, or variant.

        We add base here since it's the base category variant that
        would always be displayed and it won't affect the app, or
        platform selections.
        """
        if restrictions is not None and value not in restrictions + [
            BASE_CATEGORY_NAME
        ]:
            return True
        if requested and value not in requested + [BASE_CATEGORY_NAME]:
            return True
        return False

    def build_category_matrix(
        requested_variants=[BASE_CATEGORY_NAME],
        requested_platforms=[],
        requested_apps=[],
        **kwargs,
    ):
        """Build a decision matrix for all the categories.

        It will have the form:
            - Category
                - Variants
                    - ...
        """
        # Build the base decision matrix
        decision_matrix = PerfParser._build_decision_matrix()

        # Here, the variants are further restricted by the category settings
        # using the `_skip_with_restrictions` method. This part also handles
        # explicitly requested platforms, apps, and variants.
        category_decision_matrix = {}
        for category, category_info in PerfParser.categories.items():
            category_matrix = copy.deepcopy(decision_matrix)

            for variant_combination, variant_matrix in decision_matrix.items():
                variant_runnable = True
                if BASE_CATEGORY_NAME not in variant_combination:
                    # Make sure that all portions of the variant combination
                    # target at least one of the suites in the category
                    tmp_variant_combination = set(
                        [v.value for v in variant_combination]
                    )
                    for suite in Suites:
                        if suite.value not in category_info["suites"]:
                            continue
                        tmp_variant_combination = tmp_variant_combination - set(
                            [
                                variant.value
                                for variant in variant_combination
                                if variant.value
                                in PerfParser.suites[suite.value]["variants"]
                            ]
                        )
                    if tmp_variant_combination:
                        # If it's not empty, then some variants
                        # are non-existent
                        variant_runnable = False

                for suite, platform, app in itertools.product(Suites, Platforms, Apps):
                    runnable = variant_runnable

                    # Disable this combination if there are any variant
                    # restrictions for this suite, or if the user didn't request it
                    # (and did request some variants). The same is done below with
                    # the apps, and platforms.
                    if any(
                        PerfParser._skip_with_restrictions(
                            variant.value if not isinstance(variant, str) else variant,
                            category_info.get("variant-restrictions", {}).get(
                                suite.value, None
                            ),
                            requested_variants,
                        )
                        for variant in variant_combination
                    ):
                        runnable = False

                    if PerfParser._skip_with_restrictions(
                        platform.value,
                        category_info.get("platform-restrictions", None),
                        requested_platforms,
                    ):
                        runnable = False

                    # If the platform is restricted, check if the appropriate
                    # flags were provided (or appropriate conditions hit). We do
                    # the same thing for apps below.
                    if (
                        PerfParser.platforms[platform.value].get("restriction", None)
                        is not None
                    ):
                        runnable = runnable and PerfParser.platforms[platform.value][
                            "restriction"
                        ](**kwargs)

                    if PerfParser._skip_with_restrictions(
                        app.value,
                        category_info.get("app-restrictions", {}).get(
                            suite.value, None
                        ),
                        requested_apps,
                    ):
                        runnable = False
                    if PerfParser.apps[app.value].get("restriction", None) is not None:
                        runnable = runnable and PerfParser.apps[app.value][
                            "restriction"
                        ](**kwargs)

                    category_matrix[variant_combination][suite][platform][app] = (
                        runnable and variant_matrix[suite][platform][app]
                    )

            category_decision_matrix[category] = category_matrix

        return category_decision_matrix

    def _accept_variant(suite, category_info, variant):
        """Checks if the variant can run in the given suite."""
        variant_restrictions = PerfParser.suites[suite]["variants"]
        if (
            category_info.get("original", {})
            .get("variant-restrictions", {})
            .get(suite, None)
            is not None
        ):
            # The category itself might have restricted variants
            variant_restrictions = category_info["original"]["variant-restrictions"][
                suite
            ]
        if variant in variant_restrictions:
            return True
        return False

    def _accept_variants(suite, category_info, variants):
        """Checks if some part of the variant combination can run in the suite."""
        for variant in variants:
            if PerfParser._accept_variant(suite, category_info, variant):
                return True
        return False

    def _maybe_add_variant_queries(
        suite,
        category_info,
        current_queries,
        variants,
    ):
        """Used to add variant queries to any suite that needs them."""
        modified_queries = copy.deepcopy(current_queries)
        if variants is not None:
            # We might not be able to add all of them to this suite,
            # so we go through them one by one
            for variant in variants:
                if PerfParser._accept_variant(suite, category_info, variant):
                    modified_queries.append(PerfParser.variants[variant]["query"])
        return modified_queries

    def _accept_category(
        suite,
        category_info,
        platforms=None,
        app=None,
    ):
        """Used for accepting categories based on the suite."""
        if platforms is not None and not any(
            platform in PerfParser.suites[suite]["platforms"] for platform in platforms
        ):
            return False
        app_restrictions = (
            PerfParser.suites[suite]["apps"]
            if category_info.get("app-restrictions", None) is None
            else category_info["app-restrictions"]
        )
        if app is not None and app not in app_restrictions:
            return False
        return True

    def _maybe_add_queries(
        suite,
        category_info,
        current_queries,
        new_queries,
        platforms=None,
        app=None,
    ):
        """This is a helper method to apply suite-based restrictions."""
        modified_queries = copy.deepcopy(current_queries)
        if PerfParser._accept_category(
            suite, category_info, platforms=platforms, app=app
        ):
            modified_queries.extend(new_queries)
        return modified_queries

    def expand_categories(
        android=False,
        chrome=False,
        safari=False,
        live_sites=False,
        profile=False,
        requested_variants=[],
        requested_platforms=[],
        requested_apps=[],
    ):
        """Setup the perf categories.

        This has multiple steps:
            (1) Expand the variants to all possible combinations
            (2) Expand the test categories for all valid platform+app combinations
            (3) Expand the categories from (2) into all possible combinations,
                by combining them with those created in (1). At this stage,
                we also check to make sure the variant combination is valid
                in the sense that it COULD run on the platform. It may still
                be undefined.

        We make use of global queries to provide a thorough protection
        against unwillingly scheduling tasks we very often don't want.

        Note that the flags are not intersectional. This means that if you
        have live_sites=True, and profile=False, you will get tasks which
        have profiling available to them. However, all of those tasks must
        also be live sites.
        """
        expanded_categories = {}

        # These global queries get applied to all of the categories. They make it
        # simpler to prevent, for example, chrome tests running in the
        # "Pageload desktop" category
        global_queries = {suite: [] for suite in PerfParser.suites}

        # Rather than dealing with these flags below, simply add the
        # variants related to them here
        if live_sites:
            requested_variants.append("live-sites")
        else:
            global_queries["raptor"].append(
                PerfParser.variants["live-sites"]["negation"]
            )
        if profile:
            requested_variants.append("profiling")
        else:
            global_queries["raptor"].append(
                PerfParser.variants["profiling"]["negation"]
            )

        if not chrome:
            global_queries["raptor"].append("!chrom")

        if not safari:
            global_queries["raptor"].append("!safari")

        # Start by expanding the variants the variants to include combinatorial
        # options, searching for these tasks is "best-effort" and we can't
        # guarantee all of them will have tasks selected as some may not be
        # defined in the Taskcluster config files
        expanded_variants = [
            variant_combination
            for set_size in range(len(PerfParser.variants.keys()) + 1)
            for variant_combination in itertools.combinations(
                list(PerfParser.variants.keys()), set_size
            )
        ]

        # Expand the test categories to show combined platforms and apps. By default,
        # we'll show all desktop platforms and no variants.
        for category, category_info in PerfParser.categories.items():

            # Setup the platforms
            for platform, platform_info in PerfParser.platforms.items():
                if len(requested_platforms) > 0 and platform not in requested_platforms:
                    # Skip the platform because it wasn't requested
                    continue

                platform_type = platform_info["platform"]
                if not android and platform_type == "android":
                    # Skip android if it wasn't requested
                    continue

                # Skip category if it can't use this platform
                if (
                    category_info.get("platform-restrictions", None)
                    and platform_type not in category_info["platform-restrictions"]
                ):
                    continue

                if not any(
                    PerfParser._accept_category(
                        suite,
                        category_info,
                        platforms=(
                            [platform_type]
                            + category_info.get("platforms", {}).get(suite, [])
                        ),
                    )
                    for suite in category_info["suites"]
                ):
                    continue

                # The queries field will hold all the queries needed to run
                # (in any order). Combinations of queries are used to make the
                # selected tests increasingly more specific.
                new_category = category + " %s" % platform
                cur_cat = {
                    "queries": {},
                    "tasks": category_info["tasks"],
                    "platform": platform_type,
                    "suites": category_info["suites"],
                    "original": category_info,
                }

                for suite, query in category_info["query"].items():
                    cur_cat["queries"][suite] = PerfParser._maybe_add_queries(
                        suite,
                        category_info,
                        category_info["query"][suite],
                        [platform_info["query"]] + global_queries[suite],
                        platforms=(
                            [platform_type]
                            + category_info.get("platforms", {}).get(suite, [])
                        ),
                    )

                # If we didn't request apps, add the global category
                if len(requested_apps) == 0:
                    expanded_categories[new_category] = cur_cat

                for app, app_info in PerfParser.apps.items():
                    if len(requested_apps) > 0 and app not in requested_apps:
                        # Skip the app because it wasn't requested
                        continue

                    if app.lower() in ("chrome", "chromium", "chrome-m") and not chrome:
                        # Skip chrome tests if not requested
                        continue
                    if app.lower() in ("safari",) and not safari:
                        # Skip Safari tests if not requested
                        continue
                    if (
                        platform_type not in app_info["platforms"]
                        and platform not in app_info["platforms"]
                    ):
                        # Ensure this app can run on this platform
                        continue
                    if not any(
                        PerfParser._accept_category(suite, category_info, app=app)
                        for suite in category_info["suites"]
                    ):
                        continue

                    new_app_category = new_category + " %s" % app
                    expanded_categories[new_app_category] = {
                        "queries": {
                            suite: PerfParser._maybe_add_queries(
                                suite,
                                category_info,
                                query,
                                [app_info["query"]],
                                app=app,
                            )
                            for suite, query in cur_cat["queries"].items()
                        },
                        "tasks": category_info["tasks"],
                        "platform": platform_type,
                        "suites": category_info["suites"],
                        "original": category_info,
                    }

        # Finally, handle expanding the variants. This needs to be done
        # outside the upper for-loop because variants can apply to all
        # of the expanded categories that get produced there.
        if len(requested_variants) > 0:
            new_categories = {}

            for expanded_category, info in expanded_categories.items():
                for variant_combination in expanded_variants:
                    if not variant_combination:
                        continue
                    # Check if the combination contains the requested variant
                    if not any(
                        variant in variant_combination for variant in requested_variants
                    ):
                        continue

                    # Ensure that this variant combination can run on this platform
                    runnable = True
                    for variant in variant_combination:
                        if (
                            info["platform"]
                            not in PerfParser.variants[variant]["platforms"]
                        ):
                            runnable = False
                            break
                        if (
                            len(info["suites"]) == 1
                            and variant
                            not in PerfParser.suites[info["suites"][0]]["variants"]
                        ):
                            # This section differs from below because we're handling
                            # single-suite categories and a single variant being
                            # non-existent prevents the entire category from existing but
                            # this isn't the case with more than 1 suite.
                            runnable = False
                            break
                    if not runnable:
                        continue

                    # Prevent running this variant combination when none of the suites
                    # specified can run it
                    if len(info["suites"]) > 1 and not any(
                        PerfParser._accept_variants(suite, info, variant_combination)
                        for suite in info["suites"]
                    ):
                        continue

                    # Build the category name, and setup the queries/tasks
                    # that it would use/select
                    new_variant_category = expanded_category + " %s" % "+".join(
                        variant_combination
                    )
                    new_categories[new_variant_category] = {
                        "queries": {
                            suite: PerfParser._maybe_add_variant_queries(
                                suite,
                                info,
                                suite_queries,
                                variant_combination,
                            )
                            for suite, suite_queries in info["queries"].items()
                        },
                        "tasks": info["tasks"],
                    }

                    # Now ensure that the queries for this new category
                    # don't contain negations for the variant which could
                    # come from the global queries
                    new_queries = {}
                    for suite, suite_queries in new_categories[new_variant_category][
                        "queries"
                    ].items():
                        for query in suite_queries:
                            if any(
                                [
                                    query
                                    == PerfParser.variants.get(variant)["negation"]
                                    for variant in variant_combination
                                ]
                            ):
                                # This query is a negation of one of the variants,
                                # exclude it
                                continue
                            new_queries.setdefault(suite, []).append(query)
                    new_categories[new_variant_category]["queries"] = new_queries

            expanded_categories.update(new_categories)

        return expanded_categories

    def perf_push_to_try(
        selected_tasks, selected_categories, queries, try_config, dry_run
    ):
        """Perf-specific push to try method.

        This makes use of logic from the CompareParser to do something
        very similar except with log redirection. We get the comparison
        revisions, then use the repository object to update between revisions
        and the LogProcessor for parsing out the revisions that are used
        to build the Perfherder links.
        """
        vcs = get_repository_object(build.topsrcdir)
        compare_commit, current_revision_ref = PerfParser.get_revisions_to_run(
            vcs, None
        )

        # Build commit message
        msg = "Perf selections={} (queries={})".format(
            ",".join(selected_categories),
            "&".join([q for q in queries if q is not None and len(q) > 0]),
        )

        updated = False
        new_revision_treeherder = ""
        base_revision_treeherder = ""
        try:
            # redirect_stdout allows us to feed each line into
            # a processor that we can use to catch the revision
            # while providing real-time output
            log_processor = LogProcessor()
            with redirect_stdout(log_processor):
                push_to_try(
                    "perf",
                    "{msg}".format(msg=msg),
                    # XXX Figure out if changing `fuzzy` to `perf` will break something
                    try_task_config=generate_try_task_config(
                        "fuzzy", selected_tasks, try_config
                    ),
                    stage_changes=False,
                    dry_run=dry_run,
                    closed_tree=False,
                    allow_log_capture=True,
                )

            new_revision_treeherder = log_processor.revision

            if not dry_run:
                vcs.update(compare_commit)
                updated = True

                with redirect_stdout(log_processor):
                    # XXX Figure out if we can use the `again` selector in some way
                    # Right now we would need to modify it to be able to do this.
                    # XXX Fix up the again selector for the perf selector (if it makes sense to)
                    push_to_try(
                        "perf-again",
                        "{msg}".format(msg=msg),
                        try_task_config=generate_try_task_config(
                            "fuzzy", selected_tasks, try_config
                        ),
                        stage_changes=False,
                        dry_run=dry_run,
                        closed_tree=False,
                        allow_log_capture=True,
                    )

                base_revision_treeherder = log_processor.revision
        finally:
            if updated:
                vcs.update(current_revision_ref)

        return base_revision_treeherder, new_revision_treeherder

    def run(
        update=False,
        show_all=False,
        android=False,
        chrome=False,
        safari=False,
        live_sites=False,
        parameters=None,
        profile=False,
        requested_variants=[],
        requested_platforms=[],
        requested_apps=[],
        try_config=None,
        dry_run=False,
        **kwargs,
    ):
        # Setup fzf
        fzf = fzf_bootstrap(update)

        if not fzf:
            print(FZF_NOT_FOUND)
            return 1

        all_tasks, dep_cache, cache_dir = setup_tasks_for_fzf(
            not dry_run,
            parameters,
            full=True,
            disable_target_task_filter=False,
        )
        base_cmd = build_base_cmd(fzf, dep_cache, cache_dir, show_estimates=False)

        # Perform the selection, then push to try and return the revisions
        queries = []
        selected_categories = []
        if not show_all:
            # Expand the categories first
            expanded_categories = PerfParser.expand_categories(
                android=android,
                chrome=chrome,
                safari=safari,
                live_sites=live_sites,
                profile=profile,
                requested_variants=requested_variants,
                requested_platforms=requested_platforms,
                requested_apps=requested_apps,
            )
            selected_tasks, selected_categories, queries = PerfParser.get_perf_tasks(
                base_cmd, all_tasks, expanded_categories
            )
        else:
            selected_tasks = PerfParser.get_tasks(base_cmd, queries, None, all_tasks)

        if len(selected_tasks) == 0:
            print("No tasks selected")
            return None

        return PerfParser.perf_push_to_try(
            selected_tasks, selected_categories, queries, try_config, dry_run
        )

    def run_category_checks():
        # XXX: Add a jsonschema check for the category definition
        # Make sure the queries don't specify variants in them
        variant_queries = {
            suite: [
                PerfParser.variants[variant]["query"]
                for variant in suite_info.get(
                    "variants", list(PerfParser.variants.keys())
                )
            ]
            + [
                PerfParser.variants[variant]["negation"]
                for variant in suite_info.get(
                    "variants", list(PerfParser.variants.keys())
                )
            ]
            for suite, suite_info in PerfParser.suites.items()
        }

        for category, category_info in PerfParser.categories.items():
            for suite, query in category_info["query"].items():
                if len(variant_queries[suite]) == 0:
                    # This suite has no variants
                    continue
                if any(any(v in q for q in query) for v in variant_queries[suite]):
                    raise InvalidCategoryException(
                        f"The '{category}' category suite query for '{suite}' "
                        f"uses a variant in it's query '{query}'."
                        "If you don't want a particular variant use the "
                        "`variant-restrictions` field in the category."
                    )

        return True


def run(
    dry_run=False,
    show_all=False,
    android=False,
    chrome=False,
    live_sites=False,
    parameters=None,
    profile=False,
    requested_variants=[],
    requested_platforms=[],
    requested_apps=[],
    **kwargs,
):
    # Make sure the categories are following
    # the rules we've setup
    PerfParser.run_category_checks()

    revisions = PerfParser.run(
        dry_run=dry_run,
        show_all=show_all,
        android=android,
        chrome=chrome,
        live_sites=live_sites,
        parameters=parameters,
        profile=profile,
        requested_variants=requested_variants,
        requested_platforms=requested_platforms,
        requested_apps=requested_apps,
        **kwargs,
    )

    if revisions is None:
        return

    # Provide link to perfherder for comparisons now
    perfcompare_url = PERFHERDER_BASE_URL % revisions
    print(
        "\n!!!NOTE!!!\n You'll be able to find a performance comparison here "
        "once the tests are complete (ensure you select the right "
        "framework): %s\n" % perfcompare_url
    )
    print(
        "If you need any help, you can find us in the #perf-help Matrix channel:\n"
        "https://matrix.to/#/#perf-help:mozilla.org"
    )
    print(
        "For more information on the performance tests, see our PerfDocs here:\n"
        "https://firefox-source-docs.mozilla.org/testing/perfdocs/"
    )
