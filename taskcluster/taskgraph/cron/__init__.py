# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import datetime
import json
import logging
import os
import traceback
import requests
import yaml

from . import decision, schema
from .util import (
    match_utc,
    calculate_head_rev
)
from ..create import create_task
from taskgraph.util.attributes import match_run_on_projects
from taskgraph.util.schema import resolve_keyed_by

# Functions to handle each `job.type` in `.cron.yml`.  These are called with
# the contents of the `job` property from `.cron.yml` and should return a
# sequence of (taskId, task) tuples which will subsequently be fed to
# createTask.
JOB_TYPES = {
    'decision-task': decision.run_decision_task,
}

GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
logger = logging.getLogger(__name__)
_session = None


def get_session():
    global _session
    if not _session:
        _session = requests.Session()
    return _session


def load_jobs(params):
    with open(os.path.join(GECKO, '.cron.yml'), 'rb') as f:
        cron_yml = yaml.load(f)
    schema.validate(cron_yml)

    # resolve keyed_by fields in each job
    jobs = cron_yml['jobs']
    for job in jobs:
        resolve_keyed_by(job, 'when', 'Cron job ' + job['name'],
                         project=params['project'])

    return {j['name']: j for j in jobs}


def should_run(job, params):
    run_on_projects = job.get('run-on-projects', ['all'])
    if not match_run_on_projects(params['project'], run_on_projects):
        return False
    if not any(match_utc(params, hour=sched.get('hour'), minute=sched.get('minute'))
               for sched in job.get('when', [])):
        return False
    return True


def run_job(job_name, job, params):
    params['job_name'] = job_name

    try:
        job_type = job['job']['type']
        if job_type in JOB_TYPES:
            tasks = JOB_TYPES[job_type](job['job'], params)
        else:
            raise Exception("job type {} not recognized".format(job_type))
        if params['no_create']:
            for task_id, task in tasks:
                logger.info("Not creating task {} (--no-create):\n".format(task_id) +
                            json.dumps(task, sort_keys=True, indent=4, separators=(',', ': ')))
        else:
            for task_id, task in tasks:
                create_task(get_session(), task_id, params['job_name'], task)

    except Exception:
        # report the exception, but don't fail the whole cron task, as that
        # would leave other jobs un-run.  NOTE: we could report job failure to
        # a responsible person here via tc-notify
        traceback.print_exc()
        logger.error("cron job {} run failed; continuing to next job".format(
            params['job_name']))


def calculate_time(options):
    if 'TASK_ID' not in os.environ:
        # running in a development environment, so look for CRON_TIME or use
        # the current time
        if 'CRON_TIME' in os.environ:
            logger.warning("setting params['time'] based on $CRON_TIME")
            time = datetime.datetime.utcfromtimestamp(
                int(os.environ['CRON_TIME']))
        else:
            logger.warning("using current time for params['time']; try setting $CRON_TIME "
                           "to a timestamp")
            time = datetime.datetime.utcnow()
    else:
        # fetch this task from the queue
        res = get_session().get(
            'http://taskcluster/queue/v1/task/' + os.environ['TASK_ID'])
        if res.status_code != 200:
            try:
                logger.error(res.json()['message'])
            except:
                logger.error(res.text)
            res.raise_for_status()
        # the task's `created` time is close to when the hook ran, although that
        # may be some time ago if task execution was delayed
        created = res.json()['created']
        time = datetime.datetime.strptime(created, '%Y-%m-%dT%H:%M:%S.%fZ')

    # round down to the nearest 15m
    minute = time.minute - (time.minute % 15)
    time = time.replace(minute=minute, second=0, microsecond=0)
    logger.info("calculated cron schedule time is {}".format(time))
    return time


def taskgraph_cron(options):
    params = {
        # name of this cron job (set per job below)
        'job_name': '..',

        # repositories
        'base_repository': options['base_repository'],
        'head_repository': options['head_repository'],

        # the symbolic ref this should run against (which happens to be what
        # run-task checked out for us)
        'head_ref': options['head_ref'],

        # *calculated* head_rev; this is based on the current meaning of this
        # reference in the working copy
        'head_rev': calculate_head_rev(options),

        # the project (short name for the repository) and its SCM level
        'project': options['project'],
        'level': options['level'],

        # if true, tasks will not actually be created
        'no_create': options['no_create'],

        # the time that this cron task was created (as a UTC datetime object)
        'time': calculate_time(options),
    }

    jobs = load_jobs(params)

    if options['force_run']:
        job_name = options['force_run']
        logger.info("force-running cron job {}".format(job_name))
        run_job(job_name, jobs[job_name], params)
        return

    for job_name, job in sorted(jobs.items()):
        if should_run(job, params):
            logger.info("running cron job {}".format(job_name))
            run_job(job_name, job, params)
        else:
            logger.info("not running cron job {}".format(job_name))
