# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging

from slugid import nice as slugid

from .util import (create_task_from_def, fetch_graph_and_labels)
from .registry import register_callback_action
from taskgraph.util.parameterization import resolve_task_references

TASKCLUSTER_QUEUE_URL = "https://queue.taskcluster.net/v1/task"

logger = logging.getLogger(__name__)


@register_callback_action(
    name='retrigger-mochitest-reftest-with-options',
    title='Mochitest/Reftest Retrigger',
    kind='hook',
    generic=True,
    symbol='tr',
    description="Retriggers the specified mochitest/reftest job with additional options",
    context=[{'test-type': 'mochitest'},
             {'test-type': 'reftest'}],
    order=0,
    schema={
        'type': 'object',
        'properties': {
            'path': {
                'type': 'string',
                'maxLength': 255,
                'default': '',
                'title': 'Path name',
                'description': 'Path of test to retrigger'
            },
            'logLevel': {
                'type': 'string',
                'enum': ['debug', 'info', 'warning', 'error', 'critical'],
                'default': 'debug',
                'title': 'Log level',
                'description': 'Log level for output (default is DEBUG, which is highest)'
            },
            'runUntilFail': {
                'type': 'boolean',
                'default': True,
                'title': 'Run until failure',
                'description': ('Runs the specified set of tests repeatedly '
                                'until failure (or 30 times)')
            },
            'repeat': {
                'type': 'integer',
                'default': 30,
                'minimum': 1,
                'title': 'Run tests N times',
                'description': ('Run tests repeatedly (usually used in '
                                'conjunction with runUntilFail)')
            },
            'environment': {
                'type': 'object',
                'default': {'MOZ_LOG': ''},
                'title': 'Extra environment variables',
                'description': 'Extra environment variables to use for this run',
                'additionalProperties': {'type': 'string'}
            },
            'preferences': {
                'type': 'object',
                'default': {'mygeckopreferences.pref': 'myvalue2'},
                'title': 'Extra gecko (about:config) preferences',
                'description': 'Extra gecko (about:config) preferences to use for this run',
                'additionalProperties': {'type': 'string'}
            }
        },
        'additionalProperties': False,
        'required': ['path']
    }
)
def mochitest_retrigger_action(parameters, graph_config, input, task_group_id, task_id, task):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)

    pre_task = full_task_graph.tasks[task['metadata']['name']]

    # fix up the task's dependencies, similar to how optimization would
    # have done in the decision
    dependencies = {name: label_to_taskid[label]
                    for name, label in pre_task.dependencies.iteritems()}
    new_task_definition = resolve_task_references(pre_task.label, pre_task.task, dependencies)
    new_task_definition.setdefault('dependencies', []).extend(dependencies.itervalues())

    # don't want to run mozharness tests, want a custom mach command instead
    new_task_definition['payload']['command'] += ['--no-run-tests']

    custom_mach_command = [task['tags']['test-type']]

    # mochitests may specify a flavor
    if new_task_definition['payload']['env'].get('MOCHITEST_FLAVOR'):
        custom_mach_command += [
            '--keep-open=false',
            '-f',
            new_task_definition['payload']['env']['MOCHITEST_FLAVOR']
        ]

    enable_e10s = json.loads(new_task_definition['payload']['env'].get(
        'ENABLE_E10S', 'true'))
    if not enable_e10s:
        custom_mach_command += ['--disable-e10s']

    custom_mach_command += ['--log-tbpl=-',
                            '--log-tbpl-level={}'.format(input.get('logLevel', 'debug'))]
    if input.get('runUntilFail'):
        custom_mach_command += ['--run-until-failure']
    if input.get('repeat'):
        custom_mach_command += ['--repeat', str(input.get('repeat', 30))]

    # add any custom gecko preferences
    for (key, val) in input.get('preferences', {}).iteritems():
        custom_mach_command += ['--setpref', '{}={}'.format(key, val)]

    custom_mach_command += [input['path']]
    new_task_definition['payload']['env']['CUSTOM_MACH_COMMAND'] = ' '.join(
        custom_mach_command)

    # update environment
    new_task_definition['payload']['env'].update(input.get('environment', {}))

    # tweak the treeherder symbol
    new_task_definition['extra']['treeherder']['symbol'] += '-custom'

    logging.info("New task definition: %s", new_task_definition)

    # actually create the new task
    new_task_id = slugid()
    create_task_from_def(new_task_id, new_task_definition, parameters['level'])
