# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from taskgraph.parameters import extend_parameters_schema
from voluptuous import All, Any, Range, Required


def get_defaults(repo_root):
    return {
        "next_version": None,
        "pull_request_number": None,
        "release_type": "",
    }


extend_parameters_schema(
    {
        Required("next_version"): Any(str, None),
        Required("pull_request_number"): Any(All(int, Range(min=1)), None),
        Required("release_type"): str,
    },
    defaults_fn=get_defaults,
)


def get_decision_parameters(graph_config, parameters):
    # Environment is defined in .taskcluster.yml
    pr_number = os.environ.get("MOBILE_PULL_REQUEST_NUMBER", None)
    parameters["pull_request_number"] = None if pr_number is None else int(pr_number)
    parameters.setdefault("next_version", None)
    parameters.setdefault("release_type", "")
