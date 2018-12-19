# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import logging

GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..'))

# Maximum number of dependencies a single task can have
# https://docs.taskcluster.net/reference/platform/taskcluster-queue/references/api#createTask
# specifies 100, but we also optionally add the decision task id as a dep in
# taskgraph.create, so let's set this to 99.
MAX_DEPENDENCIES = 99

# Enable fast task generation for local debugging
# This is normally switched on via the --fast/-F flag to `mach taskgraph`
# Currently this skips toolchain task optimizations and schema validation
fast = False

# Default rootUrl to use for command-line invocations
PRODUCTION_TASKCLUSTER_ROOT_URL = 'https://taskcluster.net'


def set_root_url_env():
    """Ensure that TASKCLUSTER_ROOT_URL is set, defaulting when run outside of a task."""
    logger = logging.getLogger('set_root_url_env')

    if 'TASKCLUSTER_ROOT_URL' not in os.environ:
        if 'TASK_ID' in os.environ:
            raise RuntimeError('TASKCLUSTER_ROOT_URL must be set when running in a task')
        else:
            logger.info('Setting TASKCLUSTER_ROOT_URL to default value (Firefox CI production)')
            os.environ['TASKCLUSTER_ROOT_URL'] = PRODUCTION_TASKCLUSTER_ROOT_URL
    logger.info('Running in Taskcluster instance {}{}'.format(
        os.environ['TASKCLUSTER_ROOT_URL'],
        ' with taskcluster-proxy' if 'TASKCLUSTER_PROXY_URL' in os.environ else ''))
