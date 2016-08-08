# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import json
import logging
import yaml

from .generator import TaskGraphGenerator
from .create import create_tasks
from .parameters import Parameters
from .target_tasks import get_method

from taskgraph.util.templates import Templates
from taskgraph.util.time import (
    json_time_from_now,
    current_json_time,
)

logger = logging.getLogger(__name__)

ARTIFACTS_DIR = 'artifacts'
GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..'))

# For each project, this gives a set of parameters specific to the project.
# See `taskcluster/docs/parameters.rst` for information on parameters.
PER_PROJECT_PARAMETERS = {
    'try': {
        'target_tasks_method': 'try_option_syntax',
        # for try, if a task was specified as a target, it should
        # not be optimized away
        'optimize_target_tasks': False,
    },

    'ash': {
        'target_tasks_method': 'ash_tasks',
        'optimize_target_tasks': True,
    },

    # the default parameters are used for projects that do not match above.
    'default': {
        'target_tasks_method': 'all_builds_and_tests',
        'optimize_target_tasks': True,
    }
}


def taskgraph_decision(options):
    """
    Run the decision task.  This function implements `mach taskgraph decision`,
    and is responsible for

     * processing decision task command-line options into parameters
     * running task-graph generation exactly the same way the other `mach
       taskgraph` commands do
     * generating a set of artifacts to memorialize the graph
     * calling TaskCluster APIs to create the graph
    """

    parameters = get_decision_parameters(options)

    # create a TaskGraphGenerator instance
    target_tasks_method = parameters.get('target_tasks_method', 'all_tasks')
    target_tasks_method = get_method(target_tasks_method)
    tgg = TaskGraphGenerator(
        root_dir=options['root'],
        parameters=parameters,
        target_tasks_method=target_tasks_method)

    # write out the parameters used to generate this graph
    write_artifact('parameters.yml', dict(**parameters))

    # write out the yml file for action tasks
    write_artifact('action.yml', get_action_yml(parameters))

    # write out the full graph for reference
    write_artifact('full-task-graph.json', tgg.full_task_graph.to_json())

    # write out the target task set to allow reproducing this as input
    write_artifact('target-tasks.json', tgg.target_task_set.tasks.keys())

    # write out the optimized task graph to describe what will actually happen,
    # and the map of labels to taskids
    write_artifact('task-graph.json', tgg.optimized_task_graph.to_json())
    write_artifact('label-to-taskid.json', tgg.label_to_taskid)

    # actually create the graph
    create_tasks(tgg.optimized_task_graph, tgg.label_to_taskid)


def get_decision_parameters(options):
    """
    Load parameters from the command-line options for 'taskgraph decision'.
    This also applies per-project parameters, based on the given project.

    """
    parameters = {n: options[n] for n in [
        'base_repository',
        'head_repository',
        'head_rev',
        'head_ref',
        'message',
        'project',
        'pushlog_id',
        'owner',
        'level',
        'target_tasks_method',
    ] if n in options}

    project = parameters['project']
    try:
        parameters.update(PER_PROJECT_PARAMETERS[project])
    except KeyError:
        logger.warning("using default project parameters; add {} to "
                       "PER_PROJECT_PARAMETERS in {} to customize behavior "
                       "for this project".format(project, __file__))
        parameters.update(PER_PROJECT_PARAMETERS['default'])

    return Parameters(parameters)


def write_artifact(filename, data):
    logger.info('writing artifact file `{}`'.format(filename))
    if not os.path.isdir(ARTIFACTS_DIR):
        os.mkdir(ARTIFACTS_DIR)
    path = os.path.join(ARTIFACTS_DIR, filename)
    if filename.endswith('.yml'):
        with open(path, 'w') as f:
            yaml.safe_dump(data, f, allow_unicode=True, default_flow_style=False)
    elif filename.endswith('.json'):
        with open(path, 'w') as f:
            json.dump(data, f, sort_keys=True, indent=2, separators=(',', ': '))
    else:
        raise TypeError("Don't know how to write to {}".format(filename))


def get_action_yml(parameters):
    templates = Templates(os.path.join(GECKO, "taskcluster/taskgraph"))
    action_parameters = parameters.copy()
    action_parameters.update({
        "decision_task_id": "{{decision_task_id}}",
        "task_labels": "{{task_labels}}",
        "from_now": json_time_from_now,
        "now": current_json_time()
    })
    return templates.load('action.yml', action_parameters)
