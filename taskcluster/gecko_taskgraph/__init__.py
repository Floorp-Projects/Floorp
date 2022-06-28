# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from taskgraph.util import taskcluster as tc_util, schema

GECKO = os.path.normpath(os.path.realpath(os.path.join(__file__, "..", "..", "..")))

# Maximum number of dependencies a single task can have
# https://firefox-ci-tc.services.mozilla.com/docs/reference/platform/queue/task-schema
# specifies 100, but we also optionally add the decision task id as a dep in
# taskgraph.create, so let's set this to 99.
MAX_DEPENDENCIES = 99

# Default rootUrl to use if none is given in the environment; this should point
# to the production Taskcluster deployment used for CI.
tc_util.PRODUCTION_TASKCLUSTER_ROOT_URL = "https://firefox-ci-tc.services.mozilla.com"

# Schemas for YAML files should use dashed identifiers by default.  If there are
# components of the schema for which there is a good reason to use another format,
# they can be whitelisted here.
schema.WHITELISTED_SCHEMA_IDENTIFIERS.extend(
    [
        # upstream-artifacts are handed directly to scriptWorker, which expects interCaps
        lambda path: "[{!r}]".format("upstream-artifacts") in path,
        lambda path: (
            "[{!r}]".format("test_name") in path
            or "[{!r}]".format("json_location") in path
            or "[{!r}]".format("video_location") in path
        ),
    ]
)


def register(graph_config):
    """Used to register Gecko specific extensions.

    Args:
        graph_config: The graph configuration object.
    """
    from gecko_taskgraph.parameters import register_parameters

    register_parameters()
