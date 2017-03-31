# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from . import transform


class PushApkTask(transform.TransformTask):
    """
    A task implementing a beetmover job.  These depend on nightly build and signing
    jobs and transfer the artifacts to S3 after build and signing are completed.
    """

    @classmethod
    def get_inputs(cls, kind, path, config, params, loaded_tasks):
        """
        Generate inputs implementing PushApk jobs. These depend on signed multi-locales nightly
        builds.
        """
        jobs = super(PushApkTask, cls).get_inputs(kind, path, config, params, loaded_tasks)

        for job in jobs:
            job['dependent-tasks'] = get_dependent_loaded_tasks(config, loaded_tasks)
            yield job


def get_dependent_loaded_tasks(config, loaded_tasks):
    nightly_tasks = (
        task for task in loaded_tasks if task.attributes.get('nightly')
    )
    tasks_with_matching_kind = (
        task for task in nightly_tasks if task.kind in config.get('kind-dependencies')
    )
    android_tasks = [
        task for task in tasks_with_matching_kind if 'android' in task.label
    ]

    return android_tasks
