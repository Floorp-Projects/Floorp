# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import json
import logging
import re

import time
import yaml

from .generator import TaskGraphGenerator
from .create import create_tasks
from .parameters import Parameters
from .taskgraph import TaskGraph
from .actions import render_actions_json
from . import GECKO

from taskgraph.util.templates import Templates
from taskgraph.util.time import (
    json_time_from_now,
    current_json_time,
)

logger = logging.getLogger(__name__)

ARTIFACTS_DIR = 'artifacts'

# For each project, this gives a set of parameters specific to the project.
# See `taskcluster/docs/parameters.rst` for information on parameters.
PER_PROJECT_PARAMETERS = {
    'try': {
        'target_tasks_method': 'try_option_syntax',
        # Always perform optimization.  This makes it difficult to use try
        # pushes to run a task that would otherwise be optimized, but is a
        # compromise to avoid essentially disabling optimization in try.
        'optimize_target_tasks': True,
        # By default, the `try_option_syntax` `target_task_method` ignores this
        # parameter, and enables/disables nightlies depending whether
        # `--include-nightly` is specified in the commit message.
        # We're setting the `include_nightly` parameter to True here for when
        # we submit decision tasks against Try that use other
        # `target_task_method`s, like `nightly_fennec` or `mozilla_beta_tasks`,
        # which reference the `include_nightly` parameter.
        'include_nightly': True,
    },

    'ash': {
        'target_tasks_method': 'ash_tasks',
        'optimize_target_tasks': True,
        'include_nightly': False,
    },

    'cedar': {
        'target_tasks_method': 'cedar_tasks',
        'optimize_target_tasks': True,
        'include_nightly': False,
    },

    'graphics': {
        'target_tasks_method': 'graphics_tasks',
        'optimize_target_tasks': True,
        'include_nightly': False,
    },

    'mozilla-beta': {
        'target_tasks_method': 'mozilla_beta_tasks',
        'optimize_target_tasks': False,
        'include_nightly': True,
    },

    'mozilla-release': {
        'target_tasks_method': 'mozilla_release_tasks',
        'optimize_target_tasks': False,
        'include_nightly': True,
    },

    'pine': {
        'target_tasks_method': 'pine_tasks',
        'optimize_target_tasks': True,
        'include_nightly': False,
    },

    # the default parameters are used for projects that do not match above.
    'default': {
        'target_tasks_method': 'default',
        'optimize_target_tasks': True,
        'include_nightly': False,
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
    tgg = TaskGraphGenerator(
        root_dir=options['root'],
        parameters=parameters)

    # write out the parameters used to generate this graph
    write_artifact('parameters.yml', dict(**parameters))

    # write out the yml file for action tasks
    write_artifact('action.yml', get_action_yml(parameters))

    # write out the public/actions.json file
    write_artifact('actions.json', render_actions_json(parameters))

    # write out the full graph for reference
    full_task_json = tgg.full_task_graph.to_json()
    write_artifact('full-task-graph.json', full_task_json)

    # this is just a test to check whether the from_json() function is working
    _, _ = TaskGraph.from_json(full_task_json)

    # write out the target task set to allow reproducing this as input
    write_artifact('target-tasks.json', tgg.target_task_set.tasks.keys())

    # write out the optimized task graph to describe what will actually happen,
    # and the map of labels to taskids
    write_artifact('task-graph.json', tgg.morphed_task_graph.to_json())
    write_artifact('label-to-taskid.json', tgg.label_to_taskid)

    # actually create the graph
    create_tasks(tgg.morphed_task_graph, tgg.label_to_taskid, parameters)


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
        'pushdate',
        'owner',
        'level',
        'target_tasks_method',
    ] if n in options}

    # Define default filter list, as most configurations shouldn't need
    # custom filters.
    parameters['filters'] = [
        'check_servo',
        'target_tasks_method',
    ]

    # owner must be an email, but sometimes (e.g., for ffxbld) it is not, in which
    # case, fake it
    if '@' not in parameters['owner']:
        parameters['owner'] += '@noreply.mozilla.org'

    # use the pushdate as build_date if given, else use current time
    parameters['build_date'] = parameters['pushdate'] or int(time.time())
    # moz_build_date is the build identifier based on build_date
    parameters['moz_build_date'] = time.strftime("%Y%m%d%H%M%S",
                                                 time.gmtime(parameters['build_date']))

    project = parameters['project']
    try:
        parameters.update(PER_PROJECT_PARAMETERS[project])
    except KeyError:
        logger.warning("using default project parameters; add {} to "
                       "PER_PROJECT_PARAMETERS in {} to customize behavior "
                       "for this project".format(project, __file__))
        parameters.update(PER_PROJECT_PARAMETERS['default'])

    # `target_tasks_method` has higher precedence than `project` parameters
    if options.get('target_tasks_method'):
        parameters['target_tasks_method'] = options['target_tasks_method']

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

    match = re.match(r'https://(hg.mozilla.org)/(.*?)/?$', action_parameters['head_repository'])
    if not match:
        raise Exception('Unrecognized head_repository')
    repo_scope = 'assume:repo:{}/{}:*'.format(
        match.group(1), match.group(2))

    action_parameters.update({
        "action": "{{action}}",
        "action_args": "{{action_args}}",
        "repo_scope": repo_scope,
        "from_now": json_time_from_now,
        "now": current_json_time()
    })
    return templates.load('action.yml', action_parameters)
