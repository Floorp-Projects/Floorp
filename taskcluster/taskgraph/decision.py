# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import json
import logging

import time
import yaml

from .generator import TaskGraphGenerator
from .create import create_tasks
from .parameters import Parameters, get_version, get_app_version
from .taskgraph import TaskGraph
from .try_option_syntax import parse_message
from .actions import render_actions_json
from taskgraph.util.partials import populate_release_history

logger = logging.getLogger(__name__)

ARTIFACTS_DIR = 'artifacts'

# For each project, this gives a set of parameters specific to the project.
# See `taskcluster/docs/parameters.rst` for information on parameters.
PER_PROJECT_PARAMETERS = {
    'try': {
        'target_tasks_method': 'try_tasks',
        # By default, the `try_option_syntax` `target_task_method` ignores this
        # parameter, and enables/disables nightlies depending whether
        # `--include-nightly` is specified in the commit message.
        # We're setting the `include_nightly` parameter to True here for when
        # we submit decision tasks against Try that use other
        # `target_task_method`s, like `nightly_fennec` or `mozilla_beta_tasks`,
        # which reference the `include_nightly` parameter.
        'include_nightly': True,
    },

    'try-comm-central': {
        'target_tasks_method': 'try_tasks',
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
        'optimize_target_tasks': True,
        'include_nightly': True,
    },

    'mozilla-release': {
        'target_tasks_method': 'mozilla_release_tasks',
        'optimize_target_tasks': True,
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


def full_task_graph_to_runnable_jobs(full_task_json):
    runnable_jobs = {}
    for label, node in full_task_json.iteritems():
        if not ('extra' in node['task'] and 'treeherder' in node['task']['extra']):
            continue

        th = node['task']['extra']['treeherder']
        runnable_jobs[label] = {
            'symbol': th['symbol']
        }

        for i in ('groupName', 'groupSymbol', 'collection'):
            if i in th:
                runnable_jobs[label][i] = th[i]
        if th.get('machine', {}).get('platform'):
            runnable_jobs[label]['platform'] = th['machine']['platform']
    return runnable_jobs


def taskgraph_decision(options, parameters=None):
    """
    Run the decision task.  This function implements `mach taskgraph decision`,
    and is responsible for

     * processing decision task command-line options into parameters
     * running task-graph generation exactly the same way the other `mach
       taskgraph` commands do
     * generating a set of artifacts to memorialize the graph
     * calling TaskCluster APIs to create the graph
    """

    parameters = parameters or get_decision_parameters(options)

    # create a TaskGraphGenerator instance
    tgg = TaskGraphGenerator(
        root_dir=options.get('root'),
        parameters=parameters)

    # write out the parameters used to generate this graph
    write_artifact('parameters.yml', dict(**parameters))

    # write out the public/actions.json file
    write_artifact('actions.json', render_actions_json(parameters, tgg.graph_config))

    # write out the full graph for reference
    full_task_json = tgg.full_task_graph.to_json()
    write_artifact('full-task-graph.json', full_task_json)

    # write out the public/runnable-jobs.json.gz file
    write_artifact('runnable-jobs.json.gz', full_task_graph_to_runnable_jobs(full_task_json))

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

    for n in (
        'comm_base_repository',
        'comm_head_repository',
        'comm_head_rev',
        'comm_head_ref',
    ):
        if n in options and options[n] is not None:
            parameters[n] = options[n]

    # Define default filter list, as most configurations shouldn't need
    # custom filters.
    parameters['filters'] = [
        'check_servo',
        'target_tasks_method',
    ]
    parameters['existing_tasks'] = {}
    parameters['do_not_optimize'] = []
    parameters['build_number'] = 1
    parameters['version'] = get_version()
    parameters['app_version'] = get_app_version()
    parameters['next_version'] = None
    parameters['release_type'] = ''
    parameters['release_eta'] = ''
    parameters['release_enable_partners'] = False
    parameters['release_partners'] = []
    parameters['release_partner_config'] = {}
    parameters['release_partner_build_number'] = 1
    parameters['release_enable_emefree'] = False

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

    # If the target method is nightly, we should build partials. This means
    # knowing what has been released previously.
    # An empty release_history is fine, it just means no partials will be built
    parameters.setdefault('release_history', dict())
    if 'nightly' in parameters.get('target_tasks_method', ''):
        parameters['release_history'] = populate_release_history('Firefox', project)

    if options.get('try_task_config_file'):
        task_config_file = os.path.abspath(options.get('try_task_config_file'))
    else:
        # if try_task_config.json is present, load it
        task_config_file = os.path.join(os.getcwd(), 'try_task_config.json')

    # load try settings
    if 'try' in project:
        parameters['try_mode'] = None
        if os.path.isfile(task_config_file):
            logger.info("using try tasks from {}".format(task_config_file))
            parameters['try_mode'] = 'try_task_config'
            with open(task_config_file, 'r') as fh:
                parameters['try_task_config'] = json.load(fh)
        else:
            parameters['try_task_config'] = None

        if 'try:' in parameters['message']:
            parameters['try_mode'] = 'try_option_syntax'
            args = parse_message(parameters['message'])
            parameters['try_options'] = args
        else:
            parameters['try_options'] = None

        if parameters['try_mode']:
            # The user has explicitly requested a set of jobs, so run them all
            # regardless of optimization.  Their dependencies can be optimized,
            # though.
            parameters['optimize_target_tasks'] = False
        else:
            # For a try push with no task selection, apply the default optimization
            # process to all of the tasks.
            parameters['optimize_target_tasks'] = True

    else:
        parameters['try_mode'] = None
        parameters['try_task_config'] = None
        parameters['try_options'] = None

    result = Parameters(**parameters)
    result.check()
    return result


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
    elif filename.endswith('.gz'):
        import gzip
        with gzip.open(path, 'wb') as f:
            f.write(json.dumps(data))
    else:
        raise TypeError("Don't know how to write to {}".format(filename))
