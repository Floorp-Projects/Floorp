# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

from . import target_tasks

logger = logging.getLogger(__name__)

filter_task_functions = {}


def filter_task(name):
    """Generator to declare a task filter function."""

    def wrap(func):
        filter_task_functions[name] = func
        return func

    return wrap


@filter_task("target_tasks_method")
def filter_target_tasks(graph, parameters, graph_config):
    """Proxy filter to use legacy target tasks code.

    This should go away once target_tasks are converted to filters.
    """

    attr = parameters.get("target_tasks_method", "all_tasks")
    fn = target_tasks.get_method(attr)
    return fn(graph, parameters, graph_config)
