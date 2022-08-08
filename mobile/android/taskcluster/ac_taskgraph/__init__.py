# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from importlib import import_module

from mozilla_version.maven import MavenVersion
from taskgraph.parameters import extend_parameters_schema
from voluptuous import All, Any, Range, Required

from .build_config import get_components, get_version


extend_parameters_schema({
    Required("pull_request_number"): Any(All(int, Range(min=1)), None),
    Required("base_rev"): Any(str, None),
    Required("next_version"): Any(str, None),
    Required("version"): str,
})

def register(graph_config):
    """
    Import all modules that are siblings of this one, triggering decorators in
    the process.
    """
    _import_modules(["job", "routes", "target_tasks", "worker_types", "release_promotion"])


def _import_modules(modules):
    for module in modules:
        import_module(f".{module}", package=__name__)


def get_decision_parameters(graph_config, parameters):
    # Environment is defined in .taskcluster.yml
    pr_number = os.environ.get("MOBILE_PULL_REQUEST_NUMBER", None)
    parameters["pull_request_number"] = None if pr_number is None else int(pr_number)
    parameters["base_rev"] = os.environ.get("MOBILE_BASE_REV")
    parameters["head_ref"] = os.environ.get("MOBILE_HEAD_REF")
    version = get_version()
    parameters["version"] = version
    parameters.setdefault("next_version", None)

    if parameters["tasks_for"] == "github-release":
        head_tag = parameters["head_tag"]
        if not head_tag:
            raise ValueError(
                "Cannot run github-release if `head_tag` is not defined. Got {}".format(
                    head_tag
                )
            )
        # XXX: tags are in the format of `v<semver>`
        if head_tag[1:] != version:
            raise ValueError(
                "Cannot run github-release if tag {} is different than in-tree "
                "{} from buildconfig.yml".format(head_tag[1:], version)
            )
        parameters["target_tasks_method"] = "release"
        version_string = parameters["version"]
        version = MavenVersion.parse(version_string)
        if version.is_release:
            next_version = version.bump("patch_number")
        else:
            raise ValueError(f"Unsupported version type: {version.version_type}")

        parameters["next_version"] = str(next_version)
