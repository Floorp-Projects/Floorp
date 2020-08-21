# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re

from six import text_type
from taskgraph.parameters import extend_parameters_schema
from voluptuous import All, Any, Optional, Range, Required


BETA_SEMVER = re.compile(r'^v\d+\.\d+\.\d+-beta\.\d+$')
RELEASE_SEMVER = re.compile(r'^v\d+\.\d+\.\d+(-rc\.\d+)?$')


extend_parameters_schema({
    Required("pull_request_number"): Any(All(int, Range(min=1)), None),
    Required("release_type"): text_type,
    Optional("shipping_phase"): Any('build', 'ship', None),
    Required("version"): text_type,
})


def get_decision_parameters(graph_config, parameters):
    head_tag = parameters["head_tag"].decode("utf-8")
    parameters["release_type"] = resolve_release_type(head_tag)
    parameters["version"] = head_tag[1:] if head_tag else ""

    pr_number = os.environ.get("MOBILE_PULL_REQUEST_NUMBER", None)
    parameters["pull_request_number"] = None if pr_number is None else int(pr_number)

    if parameters["tasks_for"] == "github-release":
        for param_name in ("release_type", "version"):
            if not parameters[param_name]:
                raise ValueError(
                    'Cannot run github-release if "{}" is not defined. Got: {}'.format(
                        param_name, parameters[param_name]
                    )
                )
        parameters["target_tasks_method"] = "release"


def resolve_release_type(head_tag):
    if not head_tag:
        return ""
    elif BETA_SEMVER.match(head_tag):
        return "beta"
    elif RELEASE_SEMVER.match(head_tag):
        return "release"
    else:
        raise ValueError('Github tag must be in semver format and prefixed with a "v", '
                         'e.g.: "v1.0.0-beta.0" (beta), "v1.0.0-rc.0" (release) or "v1.0.0" (release)')
