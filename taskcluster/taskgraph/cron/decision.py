# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import pipes
import yaml
import re
import os
import slugid


def run_decision_task(job, params):
    arguments = []
    if 'target-tasks-method' in job:
        arguments.append('--target-tasks-method={}'.format(job['target-tasks-method']))
    return [
        make_decision_task(
            params,
            symbol=job['treeherder-symbol'],
            arguments=arguments),
    ]


def make_decision_task(params, symbol, arguments=[], head_rev=None):
    """Generate a basic decision task, based on the root
    .taskcluster.yml"""
    with open('.taskcluster.yml') as f:
        taskcluster_yml = f.read()

    if not head_rev:
        head_rev = params['head_rev']

    # do a cheap and dirty job of the template substitution that mozilla-taskcluster
    # does when it reads .taskcluster.yml
    comment = '"no push -- cron task \'{job_name}\'"'.format(**params),
    replacements = {
        '\'{{{?now}}}?\'': "{'relative-datestamp': '0 seconds'}",
        '{{{?owner}}}?': 'nobody@mozilla.org',
        '{{#shellquote}}{{{comment}}}{{/shellquote}}': comment,
        '{{{?source}}}?': params['head_repository'],
        '{{{?url}}}?': params['head_repository'],
        '{{{?project}}}?': params['project'],
        '{{{?level}}}?': params['level'],
        '{{{?revision}}}?': head_rev,
        '\'{{#from_now}}([^{]*){{/from_now}}\'': "{'relative-datestamp': '\\1'}",
        '{{{?pushdate}}}?': '0',
        # treeherder ignores pushlog_id, so set it to -1
        '{{{?pushlog_id}}}?': '-1',
        # omitted as unnecessary
        # {{#as_slugid}}..{{/as_slugid}}
    }
    for pattern, replacement in replacements.iteritems():
        taskcluster_yml = re.sub(pattern, replacement, taskcluster_yml)

    task = yaml.load(taskcluster_yml)['tasks'][0]['task']

    # set some metadata
    task['metadata']['name'] = 'Decision task for cron job ' + params['job_name']
    cron_task_id = os.environ.get('TASK_ID', '<cron task id>')
    descr_md = 'Created by a [cron task](https://tools.taskcluster.net/task-inspector/#{}/)'
    task['metadata']['description'] = descr_md.format(cron_task_id)

    # create new indices so these aren't mixed in with regular decision tasks
    for i, route in enumerate(task['routes']):
        if route.startswith('index'):
            task['routes'][i] = route + '-' + params['job_name']

    th = task['extra']['treeherder']
    th['groupSymbol'] = 'cron'
    th['symbol'] = symbol

    # add a scope based on the repository, with a cron:<job_name> suffix
    match = re.match(r'https://(hg.mozilla.org)/(.*?)/?$', params['head_repository'])
    if not match:
        raise Exception('Unrecognized head_repository')
    repo_scope = 'assume:repo:{}/{}:cron:{}'.format(
        match.group(1), match.group(2), params['job_name'])
    task.setdefault('scopes', []).append(repo_scope)

    # append arguments, quoted, to the decision task command
    shellcmd = task['payload']['command']
    shellcmd[-1] = shellcmd[-1].rstrip('\n')  # strip yaml artifact
    for arg in arguments:
        shellcmd[-1] += ' ' + pipes.quote(arg)

    task_id = slugid.nice()

    # set taskGroupid = taskId, as expected of decision tasks by other systems.
    # This creates a new taskGroup for this graph.
    task['taskGroupId'] = task_id

    # set the schedulerId based on the level
    task['schedulerId'] = 'gecko-level-{}'.format(params['level'])

    return (task_id, task)
