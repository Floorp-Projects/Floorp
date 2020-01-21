# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

from six import text_type

from voluptuous import Any, Required, All, Optional
from taskgraph.util.schema import (
    optionally_keyed_by,
    validate_schema,
    Schema,
)


def even_15_minutes(minutes):
    if minutes % 15 != 0:
        raise ValueError("minutes must be evenly divisible by 15")


cron_yml_schema = Schema({
    'jobs': [{
        # Name of the crontask (must be unique)
        Required('name'): text_type,

        # what to run

        # Description of the job to run, keyed by 'type'
        Required('job'): {
            Required('type'): 'decision-task',

            # Treeherder symbol for the cron task
            Required('treeherder-symbol'): text_type,

            # --target-tasks-method './mach taskgraph decision' argument
            Required('target-tasks-method'): text_type,

            Optional(
                'optimize-target-tasks',
                description='If specified, this indicates whether the target '
                            'tasks are eligible for optimization. Otherwise, '
                            'the default for the project is used.',
            ): bool,
            Optional(
                'include-push-tasks',
                description='Whether tasks from the on-push graph should be re-used '
                            'in the cron graph.',
            ): bool,
            Optional(
                'rebuild-kinds',
                description='Kinds that should not be re-used from the on-push graph.',
            ): [text_type],
        },

        # when to run it

        # Optional set of projects on which this job should run; if omitted, this will
        # run on all projects for which cron tasks are set up.  This works just like the
        # `run_on_projects` attribute, where strings like "release" and "integration" are
        # expanded to cover multiple repositories.  (taskcluster/docs/attributes.rst)
        'run-on-projects': [text_type],

        # Array of times at which this task should run.  These *must* be a
        # multiple of 15 minutes, the minimum scheduling interval.  This field
        # can be keyed by project so that each project has a different schedule
        # for the same job.
        'when': optionally_keyed_by(
            'project',
            [
                {
                    'hour': int,
                    'minute': All(int, even_15_minutes),
                    # You probably don't want both day and weekday.
                    'day': int,  # Day of the month, as used by datetime.
                    'weekday': Any('Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday',
                                   'Saturday', 'Sunday')
                }
            ]
        ),
    }],
})


def validate(cron_yml):
    validate_schema(cron_yml_schema, cron_yml, "Invalid .cron.yml:")
