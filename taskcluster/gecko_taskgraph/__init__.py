# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from taskgraph import config as taskgraph_config
from taskgraph.util import taskcluster as tc_util, schema

from gecko_taskgraph.config import graph_config_schema

GECKO = os.path.normpath(os.path.realpath(os.path.join(__file__, "..", "..", "..")))

# Maximum number of dependencies a single task can have
# https://firefox-ci-tc.services.mozilla.com/docs/reference/platform/queue/task-schema
# specifies 100, but we also optionally add the decision task id as a dep in
# taskgraph.create, so let's set this to 99.
MAX_DEPENDENCIES = 99

# Overwrite Taskgraph's default graph_config_schema with a custom one.
taskgraph_config.graph_config_schema = graph_config_schema

# Default rootUrl to use if none is given in the environment; this should point
# to the production Taskcluster deployment used for CI.
tc_util.PRODUCTION_TASKCLUSTER_ROOT_URL = "https://firefox-ci-tc.services.mozilla.com"

# Schemas for YAML files should use dashed identifiers by default. If there are
# components of the schema for which there is a good reason to use another format,
# exceptions can be added here.
schema.EXCEPTED_SCHEMA_IDENTIFIERS.extend(
    [
        "test_name",
        "json_location",
        "video_location",
    ]
)


def register(graph_config):
    """Used to register Gecko specific extensions.

    Args:
        graph_config: The graph configuration object.
    """
    from gecko_taskgraph.parameters import register_parameters
    from gecko_taskgraph import (  # noqa: trigger target task method registration
        target_tasks,
    )

    register_parameters()
