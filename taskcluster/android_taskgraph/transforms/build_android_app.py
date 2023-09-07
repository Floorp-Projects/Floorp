# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the
build-apk and build-bundle kinds.
"""


from taskgraph.transforms.base import TransformSequence
from taskgraph.util import path

from android_taskgraph.build_config import get_variant

transforms = TransformSequence()


@transforms.add
def add_common_config(config, tasks):
    for task in tasks:
        fetches = task.setdefault("fetches", {})
        fetches["toolchain"] = ["android-sdk-linux"]
        fetches["external-gradle-dependencies"] = [
            "external-gradle-dependencies.tar.xz"
        ]

        run = task.setdefault("run", {})
        run["using"] = "gradlew"
        run["use-caches"] = False

        treeherder = task.setdefault("treeherder", {})
        treeherder["kind"] = "build"
        treeherder["tier"] = 1

        task["worker-type"] = "b-linux-large-gcp"

        worker = task.setdefault("worker", {})
        worker["docker-image"] = {}
        worker["docker-image"]["in-tree"] = "android-components"
        worker["max-run-time"] = 7200
        worker["chain-of-trust"] = True

        yield task


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

        if (
            task.pop("include-shippable-secrets", False)
            and config.params["level"] == "3"
        ):
            secrets.extend(
                [
                    {
                        "key": key,
                        "name": _get_secret_index(task["name"]),
                        "path": target_file,
                    }
                    for key, target_file in _get_secrets_keys_and_target_files(task)
                ]
            )
        else:
            dummy_secrets.extend(
                [
                    {
                        "content": fake_value,
                        "path": target_file,
                    }
                    for fake_value, target_file in (
                        ("faketoken", ".adjust_token"),
                        ("faketoken", ".mls_token"),
                        ("https://fake@sentry.prod.mozaws.net/368", ".sentry_token"),
                    )
                ]
            )

        yield task


def _get_secrets_keys_and_target_files(task):
    secrets = [
        ("adjust", ".adjust_token"),
        ("sentry_dsn", ".sentry_token"),
        ("mls", ".mls_token"),
        ("nimbus_url", ".nimbus"),
    ]

    if task["name"].startswith("fenix-"):
        gradle_build_type = task["run"]["gradle-build-type"]
        secrets.extend(
            [
                (
                    "firebase",
                    "app/src/{}/res/values/firebase.xml".format(gradle_build_type),
                ),
                ("wallpaper_url", ".wallpaper_url"),
                ("pocket_consumer_key", ".pocket_consumer_key"),
            ]
        )

    return secrets


def _get_secret_index(task_name):
    product_name = task_name.split("-")[0]
    secret_name = task_name[len(product_name) + 1 :]
    secret_project_name = (
        "focus-android" if product_name in ("focus", "klar") else product_name
    )
    return f"project/mobile/firefox-android/{secret_project_name}/{secret_name}"


@transforms.add
def build_pre_gradle_command(config, tasks):
    for task in tasks:
        source_project_name = task["source-project-name"]
        pre_gradlew = task["run"].setdefault("pre-gradlew", [])
        pre_gradlew.append(["cd", path.join("mobile", "android", source_project_name)])

        yield task


@transforms.add
def build_gradle_command(config, tasks):
    for task in tasks:
        gradle_build_type = task["run"]["gradle-build-type"]
        gradle_build_name = task["run"]["gradle-build-name"]
        variant_config = get_variant(gradle_build_type, gradle_build_name)
        variant_name = variant_config["name"][0].upper() + variant_config["name"][1:]

        package_command = task["run"].pop("gradle-package-command", "assemble")
        gradle_command = [
            "clean",
            f"{package_command}{variant_name}",
        ]

        if task["run"].pop("track-apk-size", False):
            gradle_command.append(f"apkSize{variant_name}")

        task["run"]["gradlew"] = gradle_command
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
            task["run"]["gradlew"].append(f"-PtestBuildType={test_build_type}")
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
            task["run"]["gradlew"].extend(
                [
                    # We only set the `official` flag here. The actual version name will be determined
                    # by Gradle (depending on the Gecko/A-C version being used)
                    "-Pofficial"
                ]
            )
        yield task


@transforms.add
def add_release_version(config, tasks):
    for task in tasks:
        if task.pop("include-release-version", False):
            task["run"]["gradlew"].extend(
                ["-PversionName={}".format(config.params["version"]), "-Pofficial"]
            )
        yield task


@transforms.add
def add_artifacts(config, tasks):
    for task in tasks:
        gradle_build_type = task["run"].pop("gradle-build-type")
        gradle_build_name = task["run"].pop("gradle-build-name")
        gradle_build = task["run"].pop("gradle-build")
        variant_config = get_variant(gradle_build_type, gradle_build_name)
        artifacts = task.setdefault("worker", {}).setdefault("artifacts", [])
        source_project_name = task.pop("source-project-name")

        task["attributes"]["apks"] = apks = {}

        if "apk-artifact-template" in task:
            artifact_template = task.pop("apk-artifact-template")

            for apk in variant_config["apks"]:
                apk_name = artifact_template["name"].format(
                    gradle_build=gradle_build, **apk
                )
                artifacts.append(
                    {
                        "type": artifact_template["type"],
                        "name": apk_name,
                        "path": artifact_template["path"].format(
                            gradle_build_type=gradle_build_type,
                            gradle_build=gradle_build,
                            source_project_name=source_project_name,
                            **apk,
                        ),
                    }
                )
                apks[apk["abi"]] = {
                    "name": apk_name,
                }
        elif "aab-artifact-template" in task:
            variant_name = variant_config["name"]
            artifact_template = task.pop("aab-artifact-template")
            artifacts.append(
                {
                    "type": artifact_template["type"],
                    "name": artifact_template["name"],
                    "path": artifact_template["path"].format(
                        gradle_build_type=gradle_build_type,
                        gradle_build=gradle_build,
                        source_project_name=source_project_name,
                        variant_name=variant_name,
                    ),
                }
            )
            task["attributes"]["aab"] = artifact_template["name"]

        yield task
