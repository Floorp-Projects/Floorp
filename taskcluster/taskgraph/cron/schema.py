# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

from voluptuous import Schema, Any, Required, All, MultipleInvalid


def even_15_minutes(minutes):
    if minutes % 15 != 0:
        raise ValueError("minutes must be evenly divisible by 15")

cron_yml_schema = Schema({
    'jobs': [{
        # Name of the crontask (must be unique)
        Required('name'): basestring,

        # what to run

        # Description of the job to run, keyed by 'type'
        Required('job'): Any({
            Required('type'): 'decision-task',

            # Treeherder symbol for the cron task
            Required('treeherder-symbol'): basestring,

            # --triggered-by './mach taskgraph decision' argument
            'triggered-by': basestring,

            # --target-tasks-method './mach taskgraph decision' argument
            'target-tasks-method': basestring,
        }),

        # when to run it

        # Optional set of projects on which this job should run; if omitted, this will
        # run on all projects for which cron tasks are set up.  This works just like the
        # `run_on_projects` attribute, where strings like "release" and "integration" are
        # expanded to cover multiple repositories.  (taskcluster/docs/attributes.rst)
        'run-on-projects': [basestring],

        # Array of times at which this task should run.  These *must* be a multiple of
        # 15 minutes, the minimum scheduling interval.
        'when': [{'hour': int, 'minute': All(int, even_15_minutes)}],
    }],
})


def validate(cron_yml):
    try:
        cron_yml_schema(cron_yml)
    except MultipleInvalid as exc:
        msg = ["Invalid .cron.yml:"]
        for error in exc.errors:
            msg.append(str(error))
        raise Exception('\n'.join(msg))
