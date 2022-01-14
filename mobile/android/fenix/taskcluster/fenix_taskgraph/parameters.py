# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re

from mozilla_version.fenix import FenixVersion
from six import text_type
from taskgraph.parameters import extend_parameters_schema
from voluptuous import All, Any, Optional, Range, Required

from .release_promotion import read_version_file

extend_parameters_schema({
    Required("pull_request_number"): Any(All(int, Range(min=1)), None),
    Required("release_type"): text_type,
    Optional("shipping_phase"): Any('build', 'ship', None),
    Required("version"): text_type,
    Required("next_version"): Any(None, text_type),
})


def get_decision_parameters(graph_config, parameters):
    parameters.setdefault("release_type", "")
    head_tag = parameters["head_tag"]
    parameters["version"] = head_tag[1:] if head_tag else ""

    pr_number = os.environ.get("MOBILE_PULL_REQUEST_NUMBER", None)
    parameters["pull_request_number"] = None if pr_number is None else int(pr_number)
    parameters.setdefault("next_version", None)
