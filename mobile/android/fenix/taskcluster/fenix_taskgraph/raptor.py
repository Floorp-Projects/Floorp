# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy
import json

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def add_variants(config, tasks):
    only_types = config.config["only-for-build-types"]
    only_abis = config.config["only-for-abis"]

    tests = list(tasks)

    for dep_task in config.kind_dependencies_tasks:
        build_type = dep_task.attributes.get("build-type", '')
        if build_type not in only_types:
            continue

        # TODO remove this if statement once signing tasks are fully created in taskgraph
        if not dep_task.label.startswith('sign: '):
            continue

        for abi, apk_path in dep_task.attributes["apks"].items():
            if abi not in only_abis:
                continue
            for test in tests:
                test = copy.deepcopy(test)
                attributes = copy.deepcopy(dep_task.attributes)
                attributes.update(test.get("attributes", {}))
                attributes["abi"] = abi
                attributes["apk"] = apk_path
                test["attributes"] = attributes
                test["primary-dependency"] = dep_task
                yield test


@transforms.add
def build_raptor_task(config, tasks):
    for task in tasks:
        signing = task.pop("primary-dependency")
        build_type = task["attributes"]["build-type"]
        abi = task["attributes"]["abi"]
        apk = task["attributes"]["apk"]

        test_name = task.pop("test-name")

        task["name"] = "{}-{}-{}".format(task["name"], build_type, abi)
        task["description"] = "{}-{}".format(build_type, abi)

        for key in ("worker-type", "treeherder.platform"):
            resolve_keyed_by(task, key, item_name=task["name"], **{"abi": abi})

        task["treeherder"] = inherit_treeherder_from_dep(task, signing)

        extra_config = {
            "installer_url": "<signing/{}>".format(apk),
            "test_packages_url": "<geckoview-nightly/public/build/en-US/target.test_packages.json>",
        }
        env = task["worker"]["env"]
        env["EXTRA_MOZHARNESS_CONFIG"] = {
            "artifact-reference": json.dumps(extra_config, sort_keys=True)
        }
        env["GECKO_HEAD_REV"] = "default"
        env["MOZILLA_BUILD_URL"] = {"artifact-reference": "<signing/{}>".format(apk)}
        env["MOZHARNESS_URL"] = {
            "artifact-reference": "<geckoview-nightly/public/build/en-US/mozharness.zip>"
        }
        env["TASKCLUSTER_WORKER_TYPE"] = task["worker-type"]

        worker = task["worker"]
        worker.setdefault("mounts", []).append(
            {
                "content": {
                    "url": "https://hg.mozilla.org/mozilla-central/raw-file/default/taskcluster/scripts/tester/test-linux.sh"
                },
                "file": "./test-linux.sh",
            }
        )
        task["run"]["command"].append("--test={}".format(test_name))
        task["run"]["command"].extend(task.pop("args", []))

        yield task
