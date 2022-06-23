# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from taskgraph.util import taskcluster as tc_util

GECKO = os.path.normpath(os.path.realpath(os.path.join(__file__, "..", "..", "..")))

# Maximum number of dependencies a single task can have
# https://firefox-ci-tc.services.mozilla.com/docs/reference/platform/queue/task-schema
# specifies 100, but we also optionally add the decision task id as a dep in
# taskgraph.create, so let's set this to 99.
MAX_DEPENDENCIES = 99

# Default rootUrl to use if none is given in the environment; this should point
# to the production Taskcluster deployment used for CI.
tc_util.PRODUCTION_TASKCLUSTER_ROOT_URL = "https://firefox-ci-tc.services.mozilla.com"


def register(graph_config):
    """Used to register Gecko specific extensions.

    Args:
        graph_config: The graph configuration object.
    """
    from gecko_taskgraph.parameters import register_parameters

    register_parameters()
