# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import json
import logging
import os
import re

from slugid import nice as slugid
from taskgraph.util.taskcluster import list_artifacts, get_artifact, get_task_definition
from ..util.parameterization import resolve_task_references
from .registry import register_callback_action
from .util import create_task_from_def, fetch_graph_and_labels

logger = logging.getLogger(__name__)


def get_failures(task_id):
    """Returns a dict containing properties containing a list of
    directories containing test failures and a separate list of
    individual test failures from the errorsummary.log artifact for
    the task.

    Calls the helper function munge_test_path to attempt to find an
    appropriate test path to pass to the task in
    MOZHARNESS_TEST_PATHS.  If no appropriate test path can be
    determined, nothing is returned.
    """
    re_test = re.compile(r'"test": "([^"]+)"')
    re_bad_test = re.compile(r'(Last test finished|'
                             r'Main app process exited normally|'
                             r'[(]SimpleTest/TestRunner.js[)]|'
                             r'remoteautomation.py|'
                             r'unknown test url|'
                             r'https?://localhost:\d+/\d+/\d+/.*[.]html)')
    re_extract_tests = [
        re.compile(r'(?:^[^:]+:)?(?:https?|file):[^ ]+/reftest/tests/([^ ]+)'),
        re.compile(r'(?:^[^:]+:)?(?:https?|file):[^:]+:[0-9]+/tests/([^ ]+)'),
        re.compile(r'xpcshell-[^ ]+\.ini:(.*)'),
    ]

    def munge_test_path(test_path):
        if re_bad_test.search(test_path):
            return None
        for r in re_extract_tests:
            m = r.match(test_path)
            if m:
                test_path = m.group(1)
                break
        return test_path

    dirs = set()
    tests = set()
    artifacts = list_artifacts(task_id)
    for artifact in artifacts:
        if 'name' in artifact and artifact['name'].endswith('errorsummary.log'):
            stream = get_artifact(task_id, artifact['name'])
            if stream:
                # Read all of the content from the stream and split
                # the lines out since on macosx and windows, the first
                # line is empty.
                for line in stream.read().split('\n'):
                    line = line.strip()
                    match = re_test.search(line)
                    if match:
                        test_path = munge_test_path(match.group(1))
                        if test_path:
                            tests.add(test_path)
                            test_dir = os.path.dirname(test_path)
                            if test_dir:
                                dirs.add(test_dir)
    return {'dirs': sorted(dirs), 'tests': sorted(tests)}


def create_isolate_failure_tasks(task_definition, failures, level):
    """
    Create tasks to re-run the original tasks plus tasks to test
    each failing test directory and individual path.

    """
    # redo the original task...
    logger.info("create_isolate_failure_tasks: task_definition: {},"
                "failures: {}".format(task_definition, failures))
    new_task_id = slugid()
    new_task_definition = copy.deepcopy(task_definition)
    th_dict = new_task_definition['extra']['treeherder']
    th_dict['groupSymbol'] = th_dict['groupSymbol'] + '-I'
    th_dict['tier'] = 3

    logger.info('Cloning original task')
    create_task_from_def(new_task_id, new_task_definition, level)

    for failure_group in failures:
        failure_group_suffix = '-id' if failure_group == 'dirs' else '-it'
        for failure_path in failures[failure_group]:
            new_task_id = slugid()
            new_task_definition = copy.deepcopy(task_definition)
            th_dict = new_task_definition['extra']['treeherder']
            th_dict['groupSymbol'] = th_dict['groupSymbol'] + '-I'
            th_dict['symbol'] = th_dict['symbol'] + failure_group_suffix
            th_dict['tier'] = 3
            suite = new_task_definition['extra']['suite']
            if '-chunked' in suite:
                suite = suite[:suite.index('-chunked')]
            if '-coverage' in suite:
                suite = suite[:suite.index('-coverage')]
            env_dict = new_task_definition['payload']['env']
            if 'MOZHARNESS_TEST_PATHS' not in env_dict:
                env_dict['MOZHARNESS_TEST_PATHS'] = {}
            if 'windows' in th_dict['machine']['platform']:
                failure_path = '\\'.join(failure_path.split('/'))
            env_dict['MOZHARNESS_TEST_PATHS'] = json.dumps({suite: [failure_path]})
            logger.info('Creating task for {}'.format(failure_path))
            create_task_from_def(new_task_id, new_task_definition, level)


@register_callback_action(
    name='isolate-test-failures',
    title='Isolate test failures in job',
    generic=True,
    symbol='it',
    description="Re-run Tests for original manifest, directories and tests for failing tests.",
    order=150,
    context=[
        {'kind': 'test'}
    ],
    schema={
        'type': 'object',
        'properties': {
            'times': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'maximum': 100,
                'title': 'Times',
                'description': 'How many times to run each task.',
            }
        },
        'additionalProperties': False
    },
)
def isolate_test_failures(parameters, graph_config, input, task_group_id, task_id):
    task = get_task_definition(task_id)
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config)

    pre_task = full_task_graph.tasks[task['metadata']['name']]

    # fix up the task's dependencies, similar to how optimization would
    # have done in the decision
    dependencies = {name: label_to_taskid[label]
                    for name, label in pre_task.dependencies.iteritems()}

    task_definition = resolve_task_references(pre_task.label, pre_task.task, dependencies)
    task_definition.setdefault('dependencies', []).extend(dependencies.itervalues())

    failures = get_failures(task_id)
    logger.info('isolate_test_failures: %s' % failures)
    for i in range(input['times']):
        create_isolate_failure_tasks(task_definition, failures, parameters['level'])
