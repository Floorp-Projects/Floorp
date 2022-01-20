# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import hashlib
import json
import re

from mozbuild.schedules import INCLUSIVE_COMPONENTS
from mozbuild.util import ReadOnlyDict
from voluptuous import (
    Any,
    Optional,
    Required,
)

from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.transforms.test.variant import TEST_VARIANTS
from gecko_taskgraph.util.attributes import keymatch
from gecko_taskgraph.util.keyed_by import evaluate_keyed_by
from gecko_taskgraph.util.taskcluster import (
    get_artifact_path,
    get_index_url,
)
from gecko_taskgraph.util.treeherder import split_symbol, join_symbol
from gecko_taskgraph.util.platforms import platform_family
from gecko_taskgraph.util.schema import (
    resolve_keyed_by,
    Schema,
)

transforms = TransformSequence()


@transforms.add
def limit_platforms(config, tasks):
    for task in tasks:
        if not task["limit-platforms"]:
            yield task
            continue

        limited_platforms = {key: key for key in task["limit-platforms"]}
        if keymatch(limited_platforms, task["test-platform"]):
            yield task


@transforms.add
def handle_suite_category(config, tasks):
    for task in tasks:
        task.setdefault("suite", {})

        if isinstance(task["suite"], str):
            task["suite"] = {"name": task["suite"]}

        suite = task["suite"].setdefault("name", task["test-name"])
        category = task["suite"].setdefault("category", suite)

        task.setdefault("attributes", {})
        task["attributes"]["unittest_suite"] = suite
        task["attributes"]["unittest_category"] = category

        script = task["mozharness"]["script"]
        category_arg = None
        if suite.startswith("test-verify") or suite.startswith("test-coverage"):
            pass
        elif script in ("android_emulator_unittest.py", "android_hardware_unittest.py"):
            category_arg = "--test-suite"
        elif script == "desktop_unittest.py":
            category_arg = f"--{category}-suite"

        if category_arg:
            task["mozharness"].setdefault("extra-options", [])
            extra = task["mozharness"]["extra-options"]
            if not any(arg.startswith(category_arg) for arg in extra):
                extra.append(f"{category_arg}={suite}")

        # From here on out we only use the suite name.
        task["suite"] = suite
        yield task


@transforms.add
def setup_talos(config, tasks):
    """Add options that are specific to talos jobs (identified by suite=talos)"""
    for task in tasks:
        if task["suite"] != "talos":
            yield task
            continue

        extra_options = task.setdefault("mozharness", {}).setdefault(
            "extra-options", []
        )
        extra_options.append("--use-talos-json")

        # win7 needs to test skip
        if task["build-platform"].startswith("win32"):
            extra_options.append("--add-option")
            extra_options.append("--setpref,gfx.direct2d.disabled=true")

        yield task


@transforms.add
def setup_browsertime_flag(config, tasks):
    """Optionally add `--browsertime` flag to Raptor pageload tests."""

    browsertime_flag = config.params["try_task_config"].get("browsertime", False)

    for task in tasks:
        if not browsertime_flag or task["suite"] != "raptor":
            yield task
            continue

        if task["treeherder-symbol"].startswith("Rap"):
            # The Rap group is subdivided as Rap{-fenix,-refbrow(...),
            # so `gecko_taskgraph.util.treeherder.replace_group` isn't appropriate.
            task["treeherder-symbol"] = task["treeherder-symbol"].replace(
                "Rap", "Btime", 1
            )

        extra_options = task.setdefault("mozharness", {}).setdefault(
            "extra-options", []
        )
        extra_options.append("--browsertime")

        yield task


@transforms.add
def handle_artifact_prefix(config, tasks):
    """Handle translating `artifact_prefix` appropriately"""
    for task in tasks:
        if task["build-attributes"].get("artifact_prefix"):
            task.setdefault("attributes", {}).setdefault(
                "artifact_prefix", task["build-attributes"]["artifact_prefix"]
            )
        yield task


@transforms.add
def set_target(config, tasks):
    for task in tasks:
        build_platform = task["build-platform"]
        target = None
        if "target" in task:
            resolve_keyed_by(
                task, "target", item_name=task["test-name"], enforce_single_match=False
            )
            target = task["target"]
        if not target:
            if build_platform.startswith("macosx"):
                target = "target.dmg"
            elif build_platform.startswith("android"):
                target = "target.apk"
            elif build_platform.startswith("win"):
                target = "target.zip"
            else:
                target = "target.tar.bz2"

        if isinstance(target, dict):
            # TODO Remove hardcoded mobile artifact prefix
            index_url = get_index_url(target["index"])
            installer_url = "{}/artifacts/public/{}".format(index_url, target["name"])
            task["mozharness"]["installer-url"] = installer_url
        else:
            task["mozharness"]["build-artifact-name"] = get_artifact_path(task, target)

        yield task


@transforms.add
def set_treeherder_machine_platform(config, tasks):
    """Set the appropriate task.extra.treeherder.machine.platform"""
    translation = {
        # Linux64 build platform for asan is specified differently to
        # treeherder.
        "macosx1014-64/debug": "osx-10-14/debug",
        "macosx1014-64/opt": "osx-10-14/opt",
        "macosx1014-64-shippable/opt": "osx-10-14-shippable/opt",
        "macosx1100-64/opt": "osx-1100/opt",
        "macosx1100-64-shippable/opt": "osx-1100-shippable/opt",
        "win64-asan/opt": "windows10-64/asan",
        "win64-aarch64/opt": "windows10-aarch64/opt",
    }
    for task in tasks:
        # For most desktop platforms, the above table is not used for "regular"
        # builds, so we'll always pick the test platform here.
        # On macOS though, the regular builds are in the table.  This causes a
        # conflict in `verify_task_graph_symbol` once you add a new test
        # platform based on regular macOS builds, such as for QR.
        # Since it's unclear if the regular macOS builds can be removed from
        # the table, workaround the issue for QR.
        if "android" in task["test-platform"] and "pgo/opt" in task["test-platform"]:
            platform_new = task["test-platform"].replace("-pgo/opt", "/pgo")
            task["treeherder-machine-platform"] = platform_new
        elif "android-em-7.0-x86_64-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "android-em-7.0-x86_64-shippable-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "android-em-7.0-x86_64-lite-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "android-em-7.0-x86_64-shippable-lite-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "-qr" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"]
        elif "android-hw" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"]
        elif "android-em-7.0-x86_64" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        elif "android-em-7.0-x86" in task["test-platform"]:
            task["treeherder-machine-platform"] = task["test-platform"].replace(
                ".", "-"
            )
        # Bug 1602863 - must separately define linux64/asan and linux1804-64/asan
        # otherwise causes an exception during taskgraph generation about
        # duplicate treeherder platform/symbol.
        elif "linux64-asan/opt" in task["test-platform"]:
            task["treeherder-machine-platform"] = "linux64/asan"
        elif "linux1804-asan/opt" in task["test-platform"]:
            task["treeherder-machine-platform"] = "linux1804-64/asan"
        else:
            task["treeherder-machine-platform"] = translation.get(
                task["build-platform"], task["test-platform"]
            )
        yield task


@transforms.add
def set_download_symbols(config, tasks):
    """In general, we download symbols immediately for debug builds, but only
    on demand for everything else. ASAN builds shouldn't download
    symbols since they don't product symbol zips see bug 1283879"""
    for task in tasks:
        if task["test-platform"].split("/")[-1] == "debug":
            task["mozharness"]["download-symbols"] = True
        elif (
            task["build-platform"] == "linux64-asan/opt"
            or task["build-platform"] == "linux64-asan-qr/opt"
            or task["build-platform"] == "windows10-64-asan-qr/opt"
        ):
            if "download-symbols" in task["mozharness"]:
                del task["mozharness"]["download-symbols"]
        else:
            task["mozharness"]["download-symbols"] = "ondemand"
        yield task


@transforms.add
def handle_keyed_by(config, tasks):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        "instance-size",
        "docker-image",
        "max-run-time",
        "chunks",
        "e10s",
        "suite",
        "run-on-projects",
        "os-groups",
        "run-as-administrator",
        "workdir",
        "worker-type",
        "virtualization",
        "fetches.fetch",
        "fetches.toolchain",
        "target",
        "webrender-run-on-projects",
    ]
    for task in tasks:
        for field in fields:
            resolve_keyed_by(
                task,
                field,
                item_name=task["test-name"],
                enforce_single_match=False,
                project=config.params["project"],
                variant=task["attributes"].get("unittest_variant"),
            )
        yield task


@transforms.add
def setup_browsertime(config, tasks):
    """Configure browsertime dependencies for Raptor pageload tests that have
    `--browsertime` extra option."""

    for task in tasks:
        # We need to make non-trivial changes to various fetches, and our
        # `by-test-platform` may not be "compatible" with existing
        # `by-test-platform` filters.  Therefore we do everything after
        # `handle_keyed_by` so that existing fields have been resolved down to
        # simple lists.  But we use the `by-test-platform` machinery to express
        # filters so that when the time comes to move browsertime into YAML
        # files, the transition is straight-forward.
        extra_options = task.get("mozharness", {}).get("extra-options", [])

        if task["suite"] != "raptor" or "--browsertime" not in extra_options:
            yield task
            continue

        ts = {
            "by-test-platform": {
                "android.*": ["browsertime", "linux64-geckodriver", "linux64-node"],
                "linux.*": ["browsertime", "linux64-geckodriver", "linux64-node"],
                "macosx.*": ["browsertime", "macosx64-geckodriver", "macosx64-node"],
                "windows.*aarch64.*": [
                    "browsertime",
                    "win32-geckodriver",
                    "win32-node",
                ],
                "windows.*-32.*": ["browsertime", "win32-geckodriver", "win32-node"],
                "windows.*-64.*": ["browsertime", "win64-geckodriver", "win64-node"],
            },
        }

        task.setdefault("fetches", {}).setdefault("toolchain", []).extend(
            evaluate_keyed_by(ts, "fetches.toolchain", task)
        )

        fs = {
            "by-test-platform": {
                "android.*": ["linux64-ffmpeg-4.1.4"],
                "linux.*": ["linux64-ffmpeg-4.1.4"],
                "macosx.*": ["mac64-ffmpeg-4.1.1"],
                "windows.*aarch64.*": ["win64-ffmpeg-4.1.1"],
                "windows.*-32.*": ["win64-ffmpeg-4.1.1"],
                "windows.*-64.*": ["win64-ffmpeg-4.1.1"],
            },
        }

        cd_fetches = {
            "android.*": [
                "linux64-chromedriver-87",
            ],
            "linux.*": [
                "linux64-chromedriver-94",
                "linux64-chromedriver-95",
                "linux64-chromedriver-96",
            ],
            "macosx.*": [
                "mac64-chromedriver-94",
                "mac64-chromedriver-95",
                "mac64-chromedriver-96",
            ],
            "windows.*aarch64.*": [
                "win32-chromedriver-94",
                "win32-chromedriver-95",
                "win32-chromedriver-96",
            ],
            "windows.*-32.*": [
                "win32-chromedriver-94",
                "win32-chromedriver-95",
                "win32-chromedriver-96",
            ],
            "windows.*-64.*": [
                "win32-chromedriver-94",
                "win32-chromedriver-95",
                "win32-chromedriver-96",
            ],
        }

        chromium_fetches = {
            "linux.*": ["linux64-chromium"],
            "macosx.*": ["mac-chromium"],
            "windows.*aarch64.*": ["win32-chromium"],
            "windows.*-32.*": ["win32-chromium"],
            "windows.*-64.*": ["win64-chromium"],
        }

        cd_extracted_name = {
            "windows": "{}chromedriver.exe",
            "mac": "{}chromedriver",
            "default": "{}chromedriver",
        }

        if "--app=chrome" in extra_options or "--app=chrome-m" in extra_options:
            # Only add the chromedriver fetches when chrome is running
            for platform in cd_fetches:
                fs["by-test-platform"][platform].extend(cd_fetches[platform])
        if "--app=chromium" in extra_options:
            for platform in chromium_fetches:
                fs["by-test-platform"][platform].extend(chromium_fetches[platform])

            # The chromedrivers for chromium are repackaged into the archives
            # that we get the chromium binary from so we always have a compatible
            # version.
            cd_extracted_name = {
                "windows": "chrome-win/chromedriver.exe",
                "mac": "chrome-mac/chromedriver",
                "default": "chrome-linux/chromedriver",
            }

        # Disable the Raptor install step
        if "--app=chrome-m" in extra_options:
            extra_options.append("--noinstall")

        task.setdefault("fetches", {}).setdefault("fetch", []).extend(
            evaluate_keyed_by(fs, "fetches.fetch", task)
        )

        extra_options.extend(
            (
                "--browsertime-browsertimejs",
                "$MOZ_FETCHES_DIR/browsertime/node_modules/browsertime/bin/browsertime.js",
            )
        )  # noqa: E501

        eos = {
            "by-test-platform": {
                "windows.*": [
                    "--browsertime-node",
                    "$MOZ_FETCHES_DIR/node/node.exe",
                    "--browsertime-geckodriver",
                    "$MOZ_FETCHES_DIR/geckodriver.exe",
                    "--browsertime-chromedriver",
                    "$MOZ_FETCHES_DIR/" + cd_extracted_name["windows"],
                    "--browsertime-ffmpeg",
                    "$MOZ_FETCHES_DIR/ffmpeg-4.1.1-win64-static/bin/ffmpeg.exe",
                ],
                "macosx.*": [
                    "--browsertime-node",
                    "$MOZ_FETCHES_DIR/node/bin/node",
                    "--browsertime-geckodriver",
                    "$MOZ_FETCHES_DIR/geckodriver",
                    "--browsertime-chromedriver",
                    "$MOZ_FETCHES_DIR/" + cd_extracted_name["mac"],
                    "--browsertime-ffmpeg",
                    "$MOZ_FETCHES_DIR/ffmpeg-4.1.1-macos64-static/bin/ffmpeg",
                ],
                "default": [
                    "--browsertime-node",
                    "$MOZ_FETCHES_DIR/node/bin/node",
                    "--browsertime-geckodriver",
                    "$MOZ_FETCHES_DIR/geckodriver",
                    "--browsertime-chromedriver",
                    "$MOZ_FETCHES_DIR/" + cd_extracted_name["default"],
                    "--browsertime-ffmpeg",
                    "$MOZ_FETCHES_DIR/ffmpeg-4.1.4-i686-static/ffmpeg",
                ],
            }
        }

        extra_options.extend(evaluate_keyed_by(eos, "mozharness.extra-options", task))

        yield task


def get_mobile_project(task):
    """Returns the mobile project of the specified task or None."""

    if not task["build-platform"].startswith("android"):
        return

    mobile_projects = ("fenix", "geckoview", "refbrow", "chrome-m")

    for name in mobile_projects:
        if name in task["test-name"]:
            return name

    target = None
    if "target" in task:
        resolve_keyed_by(
            task, "target", item_name=task["test-name"], enforce_single_match=False
        )
        target = task["target"]
    if target:
        if isinstance(target, dict):
            target = target["name"]

        for name in mobile_projects:
            if name in target:
                return name

    return None


@transforms.add
def adjust_mobile_e10s(config, tasks):
    for task in tasks:
        project = get_mobile_project(task)
        if project == "geckoview":
            # Geckoview is always-e10s
            task["e10s"] = True
        yield task


@transforms.add
def disable_wpt_timeouts_on_autoland(config, tasks):
    """do not run web-platform-tests that are expected TIMEOUT on autoland"""
    for task in tasks:
        if (
            "web-platform-tests" in task["test-name"]
            and config.params["project"] == "autoland"
        ):
            task["mozharness"].setdefault("extra-options", []).append("--skip-timeout")
        yield task


@transforms.add
def enable_code_coverage(config, tasks):
    """Enable code coverage for the ccov build-platforms"""
    for task in tasks:
        if "ccov" in task["build-platform"]:
            # Do not run tests on fuzzing builds
            if "fuzzing" in task["build-platform"]:
                task["run-on-projects"] = []
                continue

            # Skip this transform for android code coverage builds.
            if "android" in task["build-platform"]:
                task.setdefault("fetches", {}).setdefault("toolchain", []).append(
                    "linux64-grcov"
                )
                task["mozharness"].setdefault("extra-options", []).append(
                    "--java-code-coverage"
                )
                yield task
                continue
            task["mozharness"].setdefault("extra-options", []).append("--code-coverage")
            task["instance-size"] = "xlarge"

            # Temporarily disable Mac tests on mozilla-central
            if "mac" in task["build-platform"]:
                task["run-on-projects"] = []

            # Ensure we always run on the projects defined by the build, unless the test
            # is try only or shouldn't run at all.
            if task["run-on-projects"] not in [[]]:
                task["run-on-projects"] = "built-projects"

            # Ensure we don't optimize test suites out.
            # We always want to run all test suites for coverage purposes.
            task.pop("schedules-component", None)
            task.pop("when", None)
            task["optimization"] = None

            # Add a toolchain and a fetch task for the grcov binary.
            if any(p in task["build-platform"] for p in ("linux", "osx", "win")):
                task.setdefault("fetches", {})
                task["fetches"].setdefault("fetch", [])
                task["fetches"].setdefault("toolchain", [])
                task["fetches"].setdefault("build", [])

            if "linux" in task["build-platform"]:
                task["fetches"]["toolchain"].append("linux64-grcov")
            elif "osx" in task["build-platform"]:
                task["fetches"]["fetch"].append("grcov-osx-x86_64")
            elif "win" in task["build-platform"]:
                task["fetches"]["toolchain"].append("win64-grcov")

            task["fetches"]["build"].append({"artifact": "target.mozinfo.json"})

            if "talos" in task["test-name"]:
                task["max-run-time"] = 7200
                if "linux" in task["build-platform"]:
                    task["docker-image"] = {"in-tree": "ubuntu1804-test"}
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--cycles,1")
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--tppagecycles,1")
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--no-upload-results")
                task["mozharness"]["extra-options"].append("--add-option")
                task["mozharness"]["extra-options"].append("--tptimeout,15000")
            if "raptor" in task["test-name"]:
                task["max-run-time"] = 1800
        yield task


@transforms.add
def handle_run_on_projects(config, tasks):
    """Handle translating `built-projects` appropriately"""
    for task in tasks:
        if task["run-on-projects"] == "built-projects":
            task["run-on-projects"] = task["build-attributes"].get(
                "run_on_projects", ["all"]
            )

        if task.pop("built-projects-only", False):
            built_projects = set(
                task["build-attributes"].get("run_on_projects", {"all"})
            )
            run_on_projects = set(task.get("run-on-projects", set()))

            # If 'all' exists in run-on-projects, then the intersection of both
            # is built-projects. Similarly if 'all' exists in built-projects,
            # the intersection is run-on-projects (so do nothing). When neither
            # contains 'all', take the actual set intersection.
            if "all" in run_on_projects:
                task["run-on-projects"] = sorted(built_projects)
            elif "all" not in built_projects:
                task["run-on-projects"] = sorted(run_on_projects & built_projects)
        yield task


@transforms.add
def handle_tier(config, tasks):
    """Set the tier based on policy for all test descriptions that do not
    specify a tier otherwise."""
    for task in tasks:
        if "tier" in task:
            resolve_keyed_by(
                task,
                "tier",
                item_name=task["test-name"],
                variant=task["attributes"].get("unittest_variant"),
                enforce_single_match=False,
            )

        # only override if not set for the test
        if "tier" not in task or task["tier"] == "default":
            if task["test-platform"] in [
                "linux64/opt",
                "linux64/debug",
                "linux64-shippable/opt",
                "linux64-devedition/opt",
                "linux64-asan/opt",
                "linux64-qr/opt",
                "linux64-qr/debug",
                "linux64-shippable-qr/opt",
                "linux1804-64/opt",
                "linux1804-64/debug",
                "linux1804-64-shippable/opt",
                "linux1804-64-devedition/opt",
                "linux1804-64-qr/opt",
                "linux1804-64-qr/debug",
                "linux1804-64-shippable-qr/opt",
                "linux1804-64-asan-qr/opt",
                "linux1804-64-tsan-qr/opt",
                "windows7-32-qr/debug",
                "windows7-32-qr/opt",
                "windows7-32-devedition-qr/opt",
                "windows7-32-shippable-qr/opt",
                "windows10-32-qr/debug",
                "windows10-32-qr/opt",
                "windows10-32-shippable-qr/opt",
                "windows10-32-2004-qr/debug",
                "windows10-32-2004-qr/opt",
                "windows10-32-2004-shippable-qr/opt",
                "windows10-aarch64-qr/opt",
                "windows10-64/debug",
                "windows10-64/opt",
                "windows10-64-shippable/opt",
                "windows10-64-devedition/opt",
                "windows10-64-qr/opt",
                "windows10-64-qr/debug",
                "windows10-64-shippable-qr/opt",
                "windows10-64-devedition-qr/opt",
                "windows10-64-asan-qr/opt",
                "windows10-64-2004-qr/opt",
                "windows10-64-2004-qr/debug",
                "windows10-64-2004-shippable-qr/opt",
                "windows10-64-2004-devedition-qr/opt",
                "windows10-64-2004-asan-qr/opt",
                "macosx1014-64/opt",
                "macosx1014-64/debug",
                "macosx1014-64-shippable/opt",
                "macosx1014-64-devedition/opt",
                "macosx1014-64-devedition-qr/opt",
                "macosx1014-64-qr/opt",
                "macosx1014-64-shippable-qr/opt",
                "macosx1014-64-qr/debug",
                "macosx1015-64/opt",
                "macosx1015-64/debug",
                "macosx1015-64-shippable/opt",
                "macosx1015-64-devedition/opt",
                "macosx1015-64-devedition-qr/opt",
                "macosx1015-64-qr/opt",
                "macosx1015-64-shippable-qr/opt",
                "macosx1015-64-qr/debug",
                "macosx1100-64-shippable-qr/opt",
                "macosx1100-64-qr/debug",
                "android-em-7.0-x86_64-shippable/opt",
                "android-em-7.0-x86_64-shippable-lite/opt",
                "android-em-7.0-x86_64/debug",
                "android-em-7.0-x86_64/debug-isolated-process",
                "android-em-7.0-x86_64-lite/debug",
                "android-em-7.0-x86_64/opt",
                "android-em-7.0-x86_64-lite/opt",
                "android-em-7.0-x86-shippable/opt",
                "android-em-7.0-x86-shippable-lite/opt",
                "android-em-7.0-x86_64-shippable-qr/opt",
                "android-em-7.0-x86_64-qr/debug",
                "android-em-7.0-x86_64-qr/debug-isolated-process",
                "android-em-7.0-x86_64-qr/opt",
                "android-em-7.0-x86_64-shippable-lite-qr/opt",
                "android-em-7.0-x86_64-lite-qr/debug",
                "android-em-7.0-x86_64-lite-qr/opt",
            ]:
                task["tier"] = 1
            else:
                task["tier"] = 2

        yield task


@transforms.add
def apply_raptor_tier_optimization(config, tasks):
    for task in tasks:
        if task["suite"] != "raptor":
            yield task
            continue

        if not task["test-platform"].startswith("android-hw"):
            task["optimization"] = {"skip-unless-expanded": None}
            if task["tier"] > 1:
                task["optimization"] = {"skip-unless-backstop": None}

        if task["attributes"].get("unittest_variant"):
            task["tier"] = max(task["tier"], 2)
        yield task


@transforms.add
def disable_try_only_platforms(config, tasks):
    """Turns off platforms that should only run on try."""
    try_only_platforms = ()
    for task in tasks:
        if any(re.match(k + "$", task["test-platform"]) for k in try_only_platforms):
            task["run-on-projects"] = []
        yield task


@transforms.add
def ensure_spi_disabled_on_all_but_spi(config, tasks):
    for task in tasks:
        variant = task["attributes"].get("unittest_variant", "")
        has_setpref = (
            "gtest" not in task["suite"]
            and "cppunit" not in task["suite"]
            and "jittest" not in task["suite"]
            and "junit" not in task["suite"]
            and "raptor" not in task["suite"]
        )

        if (
            has_setpref
            and variant != "socketprocess"
            and variant != "socketprocess_networking"
        ):
            task["mozharness"]["extra-options"].append(
                "--setpref=media.peerconnection.mtransport_process=false"
            )
            task["mozharness"]["extra-options"].append(
                "--setpref=network.process.enabled=false"
            )

        yield task


@transforms.add
def split_e10s(config, tasks):
    for task in tasks:
        e10s = task["e10s"]

        if e10s:
            task_copy = copy.deepcopy(task)
            task_copy["e10s"] = True
            task_copy["attributes"]["e10s"] = True
            yield task_copy

        if not e10s or e10s == "both":
            task["e10s"] = False
            task["attributes"]["e10s"] = False
            group, symbol = split_symbol(task["treeherder-symbol"])
            if group != "?":
                group += "-1proc"
            task["treeherder-symbol"] = join_symbol(group, symbol)
            task["mozharness"]["extra-options"].append("--disable-e10s")
            yield task


test_setting_description_schema = Schema(
    {
        Required("_hash"): str,
        "platform": {
            Required("arch"): Any("32", "64", "aarch64", "arm7", "x86_64"),
            Required("os"): {
                Required("name"): Any("android", "linux", "macosx", "windows"),
                Required("version"): str,
                Optional("build"): str,
            },
            Optional("device"): str,
            Optional("machine"): Any("ref-hw-2017"),
        },
        "build": {
            Required("type"): Any("opt", "debug", "debug-isolated-process"),
            Any(
                "asan",
                "ccov",
                "clang-trunk",
                "devedition",
                "lite",
                "mingwclang",
                "shippable",
                "tsan",
            ): bool,
        },
        "runtime": {Any(*list(TEST_VARIANTS.keys()) + ["1proc"]): bool},
    },
    check=False,
)
"""Schema test settings must conform to. Validated by
:py:func:`~test.test_mozilla_central.test_test_setting`"""


@transforms.add
def set_test_setting(config, tasks):
    """A test ``setting`` is the set of configuration that uniquely
    distinguishes a test task from other tasks that run the same suite
    (ignoring chunks).

    There are three different types of information that make up a setting:

    1. Platform - Information describing the underlying platform tests run on,
    e.g, OS, CPU architecture, etc.

    2. Build - Information describing the build being tested, e.g build type,
    ccov, asan/tsan, etc.

    3. Runtime - Information describing which runtime parameters are enabled,
    e.g, prefs, environment variables, etc.

    This transform adds a ``test-setting`` object to the ``extra`` portion of
    all test tasks, of the form:

    .. code-block:: json

        {
            "platform": { ... },
            "build": { ... },
            "runtime": { ... }
        }

    This information could be derived from the label, but consuming this
    object is less brittle.
    """
    # Some attributes have a dash in them which complicates parsing. Ensure we
    # don't split them up.
    # TODO Rename these so they don't have a dash.
    dash_attrs = [
        "clang-trunk",
        "ref-hw-2017",
    ]
    dash_token = "%D%"
    platform_re = re.compile(r"(\D+)(\d*)")

    for task in tasks:
        setting = {
            "platform": {
                "os": {},
            },
            "build": {},
            "runtime": {},
        }

        # parse platform and build information out of 'test-platform'
        platform, build_type = task["test-platform"].split("/", 1)

        # ensure dashed attributes don't get split up
        for attr in dash_attrs:
            if attr in platform:
                platform = platform.replace(attr, attr.replace("-", dash_token))

        parts = platform.split("-")

        # restore dashes now that split is finished
        for i, part in enumerate(parts):
            if dash_token in part:
                parts[i] = part.replace(dash_token, "-")

        match = platform_re.match(parts.pop(0))
        assert match
        os_name, os_version = match.groups()

        device = machine = os_build = None
        if os_name == "android":
            device = parts.pop(0)
            if device == "hw":
                device = parts.pop(0)
            else:
                device = "emulator"

            os_version = parts.pop(0)
            if parts[0].isdigit():
                os_version = f"{os_version}.{parts.pop(0)}"

            if parts[0] == "android":
                parts.pop(0)

            arch = parts.pop(0)

        else:
            arch = parts.pop(0)
            if parts[0].isdigit():
                os_build = parts.pop(0)

            if parts[0] == "ref-hw-2017":
                machine = parts.pop(0)

        # It's not always possible to glean the exact architecture used from
        # the task, so sometimes this will just be set to "32" or "64".
        setting["platform"]["arch"] = arch
        setting["platform"]["os"] = {
            "name": os_name,
            "version": os_version,
        }

        if os_build:
            setting["platform"]["os"]["build"] = os_build

        if device:
            setting["platform"]["device"] = device

        if machine:
            setting["platform"]["machine"] = machine

        # parse remaining parts as build attributes
        setting["build"]["type"] = build_type
        while parts:
            attr = parts.pop(0)
            if attr == "qr":
                # all tasks are webrender now, no need to store it
                continue

            setting["build"][attr] = True

        # set runtime configuration
        if not task["e10s"]:
            setting["runtime"]["1proc"] = True

        unittest_variant = task["attributes"].get("unittest_variant")
        if unittest_variant:
            for variant in unittest_variant.split("+"):
                setting["runtime"][variant] = True

        # add a hash of the setting object for easy comparisons
        setting["_hash"] = hashlib.sha256(
            json.dumps(setting, sort_keys=True).encode("utf-8")
        ).hexdigest()[:12]

        task["test-setting"] = ReadOnlyDict(**setting)
        yield task


@transforms.add
def allow_software_gl_layers(config, tasks):
    """
    Handle the "allow-software-gl-layers" property for platforms where it
    applies.
    """
    for task in tasks:
        if task.get("allow-software-gl-layers"):
            # This should be set always once bug 1296086 is resolved.
            task["mozharness"].setdefault("extra-options", []).append(
                "--allow-software-gl-layers"
            )

        yield task


@transforms.add
def enable_webrender(config, tasks):
    """
    Handle the "webrender" property by passing a flag to mozharness if it is
    enabled.
    """
    for task in tasks:
        if task.get("webrender"):
            extra_options = task["mozharness"].setdefault("extra-options", [])
            extra_options.append("--enable-webrender")
            # We only want to 'setpref' on tests that have a profile
            if not task["attributes"]["unittest_category"] in [
                "cppunittest",
                "geckoview-junit",
                "gtest",
                "jittest",
                "raptor",
            ]:
                extra_options.append("--setpref=layers.d3d11.enable-blacklist=false")

            # run webrender variants on the projects specified on webrender-run-on-projects
            if task.get("webrender-run-on-projects") is not None:
                task["run-on-projects"] = task["webrender-run-on-projects"]

        yield task


@transforms.add
def set_schedules_for_webrender_android(config, tasks):
    """android-hw has limited resources, we need webrender on phones"""
    for task in tasks:
        if task["suite"] in ["crashtest", "reftest"] and task[
            "test-platform"
        ].startswith("android-hw"):
            task["schedules-component"] = "android-hw-gfx"
        yield task


@transforms.add
def set_retry_exit_status(config, tasks):
    """Set the retry exit status to TBPL_RETRY, the value returned by mozharness
    scripts to indicate a transient failure that should be retried."""
    for task in tasks:
        task["retry-exit-status"] = [4]
        yield task


@transforms.add
def set_profile(config, tasks):
    """Set profiling mode for tests."""
    ttconfig = config.params["try_task_config"]
    profile = ttconfig.get("gecko-profile", False)
    settings = (
        "gecko-profile-interval",
        "gecko-profile-entries",
        "gecko-profile-threads",
        "gecko-profile-features",
    )

    for task in tasks:
        if profile and task["suite"] in ["talos", "raptor"]:
            extras = task["mozharness"]["extra-options"]
            extras.append("--gecko-profile")
            for setting in settings:
                value = ttconfig.get(setting)
                if value is not None:
                    # These values can contain spaces (eg the "DOM Worker"
                    # thread) and the command is constructed in different,
                    # incompatible ways on different platforms.

                    if task["test-platform"].startswith("win"):
                        # Double quotes for Windows (single won't work).
                        extras.append("--" + setting + '="' + str(value) + '"')
                    else:
                        # Other platforms keep things as separate values,
                        # rather than joining with spaces.
                        extras.append("--" + setting + "=" + str(value))

        yield task


@transforms.add
def set_tag(config, tasks):
    """Set test for a specific tag."""
    tag = None
    if config.params["try_mode"] == "try_option_syntax":
        tag = config.params["try_options"]["tag"]
    for task in tasks:
        if tag:
            task["mozharness"]["extra-options"].extend(["--tag", tag])
        yield task


@transforms.add
def set_test_type(config, tasks):
    types = ["mochitest", "reftest", "talos", "raptor", "geckoview-junit", "gtest"]
    for task in tasks:
        for test_type in types:
            if test_type in task["suite"] and "web-platform" not in task["suite"]:
                task.setdefault("tags", {})["test-type"] = test_type
        yield task


@transforms.add
def set_schedules_components(config, tasks):
    for task in tasks:
        if "optimization" in task or "when" in task:
            yield task
            continue

        category = task["attributes"]["unittest_category"]
        schedules = task.get("schedules-component", category)
        if isinstance(schedules, str):
            schedules = [schedules]

        schedules = set(schedules)
        if schedules & set(INCLUSIVE_COMPONENTS):
            # if this is an "inclusive" test, then all files which might
            # cause it to run are annotated with SCHEDULES in moz.build,
            # so do not include the platform or any other components here
            task["schedules-component"] = sorted(schedules)
            yield task
            continue

        schedules.add(category)
        schedules.add(platform_family(task["build-platform"]))

        if task["webrender"]:
            schedules.add("webrender")

        task["schedules-component"] = sorted(schedules)
        yield task
