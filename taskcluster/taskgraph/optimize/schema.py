#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import voluptuous
from six import text_type

from mozbuild import schedules

logger = logging.getLogger(__name__)


default_optimizations = (
    # always run this task (default)
    None,
    # always optimize this task
    {'always': None},
    # optimize strategy aliases for build kind
    {'build': list(schedules.ALL_COMPONENTS)},
    {'build-fuzzing': None},
    # search the index for the given index namespaces, and replace this task if found
    # the search occurs in order, with the first match winning
    {'index-search': [text_type]},
    # never optimize this task
    {'never': None},
    # skip the task except for every Nth push
    {'skip-unless-expanded': None},
    {'skip-unless-backstop': None},
    # skip this task if none of the given file patterns match
    {'skip-unless-changed': [text_type]},
    # skip this task if unless the change files' SCHEDULES contains any of these components
    {'skip-unless-schedules': list(schedules.ALL_COMPONENTS)},
    # optimize strategy aliases for the test kind
    {'test': list(schedules.ALL_COMPONENTS)},
    {'test-inclusive': list(schedules.ALL_COMPONENTS)},
    # optimize strategy alias for test-verify tasks
    {'test-verify': list(schedules.ALL_COMPONENTS)},
    # optimize strategy alias for upload-symbols tasks
    {'upload-symbols': None},
)

OptimizationSchema = voluptuous.Any(*default_optimizations)


def set_optimization_schema(schema_tuple):
    """Sets OptimizationSchema so it can be imported by the task transform.
    This function is called by projects that extend Firefox's taskgraph.
    It should be called by the project's taskgraph:register function before
    any transport or job runner code is imported.

    :param tuple schema_tuple: Tuple of possible optimization strategies
    """
    global OptimizationSchema
    if OptimizationSchema.validators == default_optimizations:
        logger.info("OptimizationSchema updated.")
        OptimizationSchema = voluptuous.Any(*schema_tuple)
    else:
        raise Exception('Can only call set_optimization_schema once.')
