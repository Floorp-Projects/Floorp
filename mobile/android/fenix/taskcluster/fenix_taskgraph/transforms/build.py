# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

import datetime

from taskgraph.transforms.base import TransformSequence
from fenix_taskgraph.gradle import get_variant


transforms = TransformSequence()


@transforms.add
def add_variant_config(config, tasks):
    for task in tasks:
        attributes = task.setdefault("attributes", {})
        if not attributes.get("build-type"):
            attributes["build-type"] = task["name"]

        yield task


@transforms.add
def add_shippable_secrets(config, tasks):
    for task in tasks:
        secrets = task["run"].setdefault("secrets", [])
        dummy_secrets = task["run"].setdefault("dummy-secrets", [])

        if task.pop("include-shippable-secrets", False) and config.params["level"] == "3":
            build_type = task["attributes"]["build-type"]
            gradle_build_type = task["run"]["gradle-build-type"]
            secret_index = 'project/mobile/fenix/{}'.format(build_type)
            secrets.extend([{
                "key": key,
                "name": secret_index,
                "path": target_file,
            } for key, target_file in (
                ('adjust', '.adjust_token'),
                ('firebase', 'app/src/{}/res/values/firebase.xml'.format(gradle_build_type)),
                ('sentry_dsn', '.sentry_token'),
                ('mls', '.mls_token'),
                ('nimbus_url', '.nimbus'),
            )])
        else:
            dummy_secrets.extend([{
                "content": fake_value,
                "path": target_file,
            } for fake_value, target_file in (
                ("faketoken", ".adjust_token"),
                ("faketoken", ".mls_token"),
                ("https://fake@sentry.prod.mozaws.net/368", ".sentry_token"),
            )])

        yield task


@transforms.add
def build_gradle_command(config, tasks):
    for task in tasks:
        gradle_build_type = task["run"]["gradle-build-type"]
        variant_config = get_variant(gradle_build_type)

        task["run"]["gradlew"] = [
            "clean",
            "assemble{}".format(variant_config["name"].capitalize()),
        ]

        yield task

@transforms.add
def extra_gradle_options(config, tasks):
    for task in tasks:
        for extra in task["run"].pop("gradle-extra-options", []):
            task["run"]["gradlew"].append(extra)

        yield task

@transforms.add
def add_test_build_type(config, tasks):
    for task in tasks:
        test_build_type = task["run"].pop("test-build-type", "")
        if test_build_type:
            task["run"]["gradlew"].append(
                "-PtestBuildType={}".format(test_build_type)
            )
        yield task


@transforms.add
def add_disable_optimization(config, tasks):
    for task in tasks:
        if task.pop("disable-optimization", False):
            task["run"]["gradlew"].append("-PdisableOptimization")
        yield task


@transforms.add
def add_nightly_version(config, tasks):
    for task in tasks:
        if task.pop("include-nightly-version", False):
            task["run"]["gradlew"].extend([
                # We only set the `official` flag here. The actual version name will be determined
                # by Gradle (depending on the Gecko/A-C version being used)
                '-Pofficial'
            ])
        yield task


@transforms.add
def add_release_version(config, tasks):
    for task in tasks:
        if task.pop("include-release-version", False):
            task["run"]["gradlew"].extend([
                '-PversionName={}'.format(config.params["version"]),
                '-Pofficial'
            ])
        yield task


@transforms.add
def add_artifacts(config, tasks):
    for task in tasks:
        gradle_build_type = task["run"].pop("gradle-build-type")
        variant_config = get_variant(gradle_build_type)
        artifacts = task.setdefault("worker", {}).setdefault("artifacts", [])
        task["attributes"]["apks"] = apks = {}

        if "apk-artifact-template" in task:
            artifact_template = task.pop("apk-artifact-template")
            for apk in variant_config["apks"]:
                apk_name = artifact_template["name"].format(
                    **apk
                )
                artifacts.append({
                    "type": artifact_template["type"],
                    "name": apk_name,
                    "path": artifact_template["path"].format(
                        gradle_build_type=gradle_build_type,
                        **apk
                    ),
                })
                apks[apk["abi"]] = {
                    "name": apk_name,
                    "github-name": artifact_template["github-name"].format(
                        version=config.params["version"],
                        **apk
                    )
                }

        yield task


@transforms.add
def filter_incomplete_translation(config, tasks):
    for task in tasks:
        if task.pop("filter-incomplete-translations", False):
            # filter-release-translations modifies source, which could cause problems if we ever start caching source
            pre_gradlew = task["run"].setdefault("pre-gradlew", [])
            pre_gradlew.append(["python", "automation/taskcluster/l10n/filter-release-translations.py"])
        yield task
