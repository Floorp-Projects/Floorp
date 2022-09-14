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
    Required("next_version"): Any(str, None),
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
    parameters.setdefault("next_version", None)
