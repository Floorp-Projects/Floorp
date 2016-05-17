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

ARTIFACTS_DIR = 'artifacts'

logger = logging.getLogger(__name__)

# For each project, this gives a set of parameters specific to the project.
# See `taskcluster/docs/parameters.rst` for information on parameters.
PER_PROJECT_PARAMETERS = {
    'try': {
        'target_tasks_method': 'try_option_syntax',
    },

    # the default parameters are used for projects that do not match above.
    'default': {
        'target_tasks_method': 'all_tasks',
    }
}


def taskgraph_decision(log, options):
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
        log=log,
        parameters=parameters,
        target_tasks_method=target_tasks_method)

    # write out the parameters used to generate this graph
    write_artifact('parameters.yml', dict(**parameters), log)

    # write out the full graph for reference
    write_artifact('full-task-graph.json',
                   taskgraph_to_json(tgg.full_task_graph),
                   log)

    # write out the target task set to allow reproducing this as input
    write_artifact('target_tasks.json',
                   tgg.target_task_set.tasks.keys(),
                   log)

    # write out the optimized task graph to describe what will happen
    write_artifact('task-graph.json',
                   taskgraph_to_json(tgg.optimized_task_graph),
                   log)

    # actually create the graph
    create_tasks(tgg.optimized_task_graph)


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
        'revision_hash',
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


def taskgraph_to_json(taskgraph):
    tasks = taskgraph.tasks

    def tojson(task):
        return {
            'task': task.task,
            'attributes': task.attributes,
            'dependencies': []
        }
    rv = {label: tojson(tasks[label]) for label in taskgraph.graph.nodes}

    # add dependencies with one trip through the graph edges
    for (left, right, name) in taskgraph.graph.edges:
        rv[left]['dependencies'].append((name, right))

    return rv


def write_artifact(filename, data, log):
    log(logging.INFO, 'writing-artifact', {
        'filename': filename,
    }, 'writing artifact file `{filename}`')
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
