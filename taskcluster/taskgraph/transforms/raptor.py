# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy
from six import text_type

from voluptuous import (
    Optional,
    Required,
    Extra,
)

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.tests import test_description_schema
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema
from taskgraph.util.treeherder import split_symbol, join_symbol

transforms = TransformSequence()


raptor_description_schema = Schema(
    {
        # Raptor specific configs.
        Optional("apps"): optionally_keyed_by("test-platform", "subtest", [text_type]),
        Optional("raptor-test"): text_type,
        Optional("raptor-subtests"): optionally_keyed_by("app", "test-platform", list),
        Optional("activity"): optionally_keyed_by("app", text_type),
        Optional("binary-path"): optionally_keyed_by("app", text_type),
        # Configs defined in the 'test_description_schema'.
        Optional("max-run-time"): optionally_keyed_by(
            "app", "subtest", "test-platform", test_description_schema["max-run-time"]
        ),
        Optional("run-on-projects"): optionally_keyed_by(
            "app",
            "test-name",
            "raptor-test",
            "subtest",
            "variant",
            test_description_schema["run-on-projects"],
        ),
        Optional("variants"): optionally_keyed_by(
            "app", "subtest", test_description_schema["variants"]
        ),
        Optional("target"): optionally_keyed_by(
            "app", test_description_schema["target"]
        ),
        Optional("tier"): optionally_keyed_by(
            "app", "raptor-test", "subtest", "variant", test_description_schema["tier"]
        ),
        Optional("test-url-param"): optionally_keyed_by(
            "subtest", "test-platform", text_type
        ),
        Optional("run-visual-metrics"): optionally_keyed_by("app", bool),
        Required("test-name"): test_description_schema["test-name"],
        Required("test-platform"): test_description_schema["test-platform"],
        Required("require-signed-extensions"): test_description_schema[
            "require-signed-extensions"
        ],
        Required("treeherder-symbol"): test_description_schema["treeherder-symbol"],
        # Any unrecognized keys will be validated against the test_description_schema.
        Extra: object,
    }
)

transforms.add_validate(raptor_description_schema)


@transforms.add
def set_defaults(config, tests):
    for test in tests:
        test.setdefault("run-visual-metrics", False)
        yield test


@transforms.add
def split_apps(config, tests):
    app_symbols = {
        "chrome": "ChR",
        "chrome-m": "ChR",
        "chromium": "Cr",
        "fenix": "fenix",
        "refbrow": "refbrow",
    }

    for test in tests:
        apps = test.pop("apps", None)
        if not apps:
            yield test
            continue

        for app in apps:
            atest = deepcopy(test)
            suffix = "-{}".format(app)
            atest["app"] = app
            atest["description"] += " on {}".format(app.capitalize())

            name = atest["test-name"] + suffix
            atest["test-name"] = name
            atest["try-name"] = name

            if app in app_symbols:
                group, symbol = split_symbol(atest["treeherder-symbol"])
                group += "-{}".format(app_symbols[app])
                atest["treeherder-symbol"] = join_symbol(group, symbol)

            yield atest


@transforms.add
def handle_keyed_by_prereqs(config, tests):
    """
    Only resolve keys for prerequisite fields here since the
    these keyed-by options might have keyed-by fields
    as well.
    """
    for test in tests:
        resolve_keyed_by(test, "raptor-subtests", item_name=test["test-name"])
        yield test


@transforms.add
def split_raptor_subtests(config, tests):
    for test in tests:
        # For tests that have 'raptor-subtests' listed, we want to create a separate
        # test job for every subtest (i.e. split out each page-load URL into its own job)
        subtests = test.pop("raptor-subtests", None)
        if not subtests:
            yield test
            continue

        chunk_number = 0

        for subtest in subtests:
            chunk_number += 1

            # Create new test job
            chunked = deepcopy(test)
            chunked["chunk-number"] = chunk_number
            chunked["subtest"] = subtest
            chunked["subtest-symbol"] = subtest
            if isinstance(chunked["subtest"], list):
                chunked["subtest"] = subtest[0]
                chunked["subtest-symbol"] = subtest[1]
            chunked = resolve_keyed_by(
                chunked, "tier", chunked["subtest"], defer=["variant"]
            )
            yield chunked


@transforms.add
def handle_keyed_by(config, tests):
    fields = [
        "test-url-param",
        "variants",
        "limit-platforms",
        "activity",
        "binary-path",
        "fetches.fetch",
        "max-run-time",
        "run-on-projects",
        "target",
        "tier",
        "run-visual-metrics",
    ]
    for test in tests:
        for field in fields:
            resolve_keyed_by(
                test, field, item_name=test["test-name"], defer=["variant"]
            )
        yield test


@transforms.add
def split_page_load_by_url(config, tests):
    for test in tests:
        # `chunk-number` and 'subtest' only exists when the task had a
        # definition for `raptor-subtests`
        chunk_number = test.pop("chunk-number", None)
        subtest = test.get(
            "subtest"
        )  # don't pop as some tasks need this value after splitting variants
        subtest_symbol = test.pop("subtest-symbol", None)

        if not chunk_number or not subtest:
            yield test
            continue

        if len(subtest_symbol) > 10 and "ytp" not in subtest_symbol:
            raise Exception(
                "Treeherder symbol %s is lager than 10 char! Please use a different symbol."
                % subtest_symbol
            )

        if test["test-name"].startswith("browsertime-"):
            test["raptor-test"] = subtest

            # Remove youtube-playback in the test name to avoid duplication
            test["test-name"] = test["test-name"].replace("youtube-playback-", "")
        else:
            # Use full test name if running on webextension
            test["raptor-test"] = "raptor-tp6-" + subtest + "-{}".format(test["app"])

        # Only run the subtest/single URL
        test["test-name"] += "-{}".format(subtest)
        test["try-name"] += "-{}".format(subtest)

        # Set treeherder symbol and description
        group, _ = split_symbol(test["treeherder-symbol"])
        test["treeherder-symbol"] = join_symbol(group, subtest_symbol)
        test["description"] += " on {}".format(subtest)

        yield test


@transforms.add
def add_extra_options(config, tests):
    for test in tests:
        mozharness = test.setdefault("mozharness", {})
        if test.get("app", "") == "chrome-m":
            mozharness["tooltool-downloads"] = "internal"

        extra_options = mozharness.setdefault("extra-options", [])

        # Adding device name if we're on android
        test_platform = test["test-platform"]
        if test_platform.startswith("android-hw-g5"):
            extra_options.append("--device-name=g5")
        elif test_platform.startswith("android-hw-p2"):
            extra_options.append("--device-name=p2_aarch64")

        if test.pop("run-visual-metrics", False):
            extra_options.append("--browsertime-video")
            test["attributes"]["run-visual-metrics"] = True

        if "app" in test:
            extra_options.append(
                "--app={}".format(test["app"])
            )  # don't pop as some tasks need this value after splitting variants

        if "activity" in test:
            extra_options.append("--activity={}".format(test.pop("activity")))

        if "binary-path" in test:
            extra_options.append("--binary-path={}".format(test.pop("binary-path")))

        if "raptor-test" in test:
            extra_options.append("--test={}".format(test.pop("raptor-test")))

        if test["require-signed-extensions"]:
            extra_options.append("--is-release-build")

        if "test-url-param" in test:
            param = test.pop("test-url-param")
            if not param == []:
                extra_options.append(
                    "--test-url-params={}".format(param.replace(" ", ""))
                )

        extra_options.append("--project={}".format(config.params.get("project")))

        yield test
