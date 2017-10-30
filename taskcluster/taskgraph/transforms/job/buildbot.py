# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""

Support for running jobs via buildbot.

"""

from __future__ import absolute_import, print_function, unicode_literals
import copy
import slugid
from urlparse import urlparse

from taskgraph.util.schema import Schema, optionally_keyed_by, resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config
from voluptuous import Optional, Required, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.task import notification_schema


buildbot_run_schema = Schema({
    Required('using'): 'buildbot',

    # the buildername to use for buildbot-bridge, will expand {branch} in name from
    # the current project.
    Required('buildername'): basestring,

    # the product to use
    Required('product'): Any('firefox', 'mobile', 'fennec', 'devedition', 'thunderbird'),

    Optional('release-promotion'): bool,
    Optional('routes'): [basestring],
    Optional('properties'): {basestring: optionally_keyed_by('project', basestring)},

    Optional('notifications'): {
        Optional('completed'): Any(notification_schema, [basestring]),
        Optional('failed'): Any(notification_schema, [basestring]),
        Optional('artifact'): Any(notification_schema, [basestring]),
        Optional('exception'): Any(notification_schema, [basestring]),
    },
})


FULL_TASK_NAME = (
    "[{task[payload][properties][product]} "
    "{task[payload][properties][version]} "
    "build{task[payload][properties][build_number]}/"
    "{task[payload][sourcestamp][branch]}] "
    "{task[metadata][name]} task"
)


def bb_release_worker(config, worker, run):
    # props
    release_props = get_release_config(config, force=True)
    repo_path = urlparse(config.params['head_repository']).path.lstrip('/')
    revision = config.params['head_rev']
    branch = config.params['project']
    buildername = worker['buildername']
    underscore_version = release_props['version'].replace('.', '_')
    release_props.update({
        'release_promotion': True,
        'repo_path': repo_path,
        'revision': revision,
        'script_repo_revision': revision,
    })
    worker['properties'].update(release_props)
    # scopes
    worker['scopes'] = [
        "project:releng:buildbot-bridge:builder-name:{}".format(buildername)
    ]
    # routes
    if run.get('routes'):
        worker['routes'] = []
        repl_dict = {
            'branch': branch,
            'build_number': str(release_props['build_number']),
            'revision': revision,
            'underscore_version': underscore_version,
        }
        for route in run['routes']:
            route = route.format(**repl_dict)
            worker['routes'].append(route)

    notifications = run.get('notifications')
    if notifications:

        worker.setdefault('notifications', {})

        completed = notifications.get('completed')
        if completed:
            if isinstance(completed, list):
                worker['notifications']['task-completed'] = {
                    "subject": "Completed: {}".format(FULL_TASK_NAME),
                    "message": "{} has completed successfully! Yay!".format(FULL_TASK_NAME),
                    "ids": completed,
                }
            else:
                worker['notifications']['task-completed'] = completed

        failed = notifications.get('failed')
        if failed:
            if isinstance(failed, list):
                worker['notifications']['task-failed'] = {
                    "subject": "Failed: {}".format(FULL_TASK_NAME),
                    "message": "Uh-oh! {} failed.".format(FULL_TASK_NAME),
                    "ids": failed,
                }
            else:
                worker['notifications']['task-failed'] = failed

        exception = notifications.get('exception')
        if exception:
            if isinstance(exception, list):
                worker['notifications']['task-exception'] = {
                    "subject": "Exception: {}".format(FULL_TASK_NAME),
                    "message": "Uh-oh! {} resulted in an exception.".format(FULL_TASK_NAME),
                    "ids": exception,
                }
            else:
                worker['notifications']['task-exception'] = exception


def bb_ci_worker(config, worker):
    worker['properties'].update({
        'who': config.params['owner'],
        'upload_to_task_id': slugid.nice(),
    })


@run_job_using('buildbot-bridge', 'buildbot', schema=buildbot_run_schema)
def mozharness_on_buildbot_bridge(config, job, taskdesc):
    # resolve by-* keys first
    fields = [
        "run.properties.tuxedo_server_url",
    ]
    job = copy.deepcopy(job)
    for field in fields:
        resolve_keyed_by(job, field, field, **config.params)
    run = job['run']
    worker = taskdesc['worker']
    branch = config.params['project']
    product = run['product']

    buildername = run['buildername'].format(branch=branch)
    revision = config.params['head_rev']

    worker.update({
        'buildername': buildername,
        'sourcestamp': {
            'branch': branch,
            'repository': config.params['head_repository'],
            'revision': revision,
        },
        'properties': {
            'product': product,
        },
    })
    if run.get('properties'):
        worker['properties'].update(run['properties'])

    if run.get('release-promotion'):
        bb_release_worker(config, worker, run)
    else:
        bb_ci_worker(config, worker)
