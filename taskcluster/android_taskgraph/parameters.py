# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from gecko_taskgraph.parameters import extend_parameters_schema
from voluptuous import Any, Required


def get_defaults(repo_root):
    return {
        "next_version": None,
        "release_type": "",
    }


extend_parameters_schema(
    {
        Required("next_version"): Any(str, None),
        Required("release_type"): str,
    },
    defaults_fn=get_defaults,
)


def get_decision_parameters(graph_config, parameters):
    parameters.setdefault("next_version", None)
    parameters.setdefault("release_type", "")
