# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Maximum number of dependencies a single task can have
# https://docs.taskcluster.net/reference/platform/taskcluster-queue/references/api#createTask
# specifies 100, but we also optionally add the decision task id as a dep in
# taskgraph.create, so let's set this to 99.
MAX_DEPENDENCIES = 99

# Enable fast task generation for local debugging
# This is normally switched on via the --fast/-F flag to `mach taskgraph`
# Currently this skips toolchain task optimizations and schema validation
fast = False
