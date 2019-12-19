# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re

from importlib import import_module
from six import text_type
from voluptuous import All, Any, Range, Required

from taskgraph.parameters import extend_parameters_schema

BETA_SEMVER = re.compile(r'^v\d+\.\d+\.\d+-beta\.\d+$')
PRODUCTION_SEMVER = re.compile(r'^v\d+\.\d+\.\d+(-rc\.\d+)?$')


def register(graph_config):
    """
    Import all modules that are siblings of this one, triggering decorators in
    the process.
    """
    _import_modules(["job", "worker_types", "routes", "target_tasks"])
    extend_parameters_schema({
        Required("pull_request_number"): Any(All(int, Range(min=1)), None),
        Required("release_type"): text_type,
        Required("release_version"): text_type,
    })


def _import_modules(modules):
    for module in modules:
        import_module(".{}".format(module), package=__name__)


def get_decision_parameters(graph_config, parameters):
    head_tag = parameters["head_tag"].decode("utf-8")
    parameters["release_type"] = _resolve_release_type(head_tag)
    parameters["release_version"] = read_version_file()
    if head_tag:
        parameters["release_version"] = head_tag[1:]

    pr_number = os.environ.get("MOBILE_PULL_REQUEST_NUMBER", None)
    parameters["pull_request_number"] = None if pr_number is None else int(pr_number)

    if parameters["tasks_for"] == "github-release":
        for param_name in ("release_type", "release_version"):
            if not parameters[param_name]:
                raise ValueError(
                    'Cannot run github-release if "{}" is not defined. Got: {}'.format(
                        param_name, parameters[param_name]
                    )
                )
        parameters["target_tasks_method"] = "release"


def read_version_file():
    with open(os.path.join(os.path.dirname(__file__), '..', '..', 'version.txt')) as f:
        return f.read().strip().decode('utf-8')


def _resolve_release_type(head_tag):
    if not head_tag:
        return ""
    elif BETA_SEMVER.match(head_tag):
        return "beta"
    elif PRODUCTION_SEMVER.match(head_tag):
        return "production"
    else:
        raise ValueError('Github tag must be in semver format and prefixed with a "v", '
                         'e.g.: "v1.0.0-beta.0" (beta), "v1.0.0-rc.0" (production) or "v1.0.0" (production)')
