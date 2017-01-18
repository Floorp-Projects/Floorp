# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from . import transform


class BeetmoverTask(transform.TransformTask):
    """
    A task implementing a signing job.  These depend on nightly build jobs and
    sign the artifacts after a build has completed.

    We use a dictionary to create the input to the transforms.
    It will have added to it keys `build-label`, the label for the build task,
    and `build-platform` / `build-type`, its platform and type.
    """

    @classmethod
    def get_inputs(cls, kind, path, config, params, loaded_tasks):
        if config.get('kind-dependencies', []) != ["build", "build-signing"] and \
           config.get('kind-dependencies', []) != ["nightly-l10n", "nightly-l10n-signing"]:
            raise Exception("Beetmover kinds must depend on builds or signing builds")
        for task in loaded_tasks:
            if not task.attributes.get('nightly'):
                continue
            if task.kind not in config.get('kind-dependencies'):
                continue
            beetmover_task = {}
            beetmover_task['dependent-task'] = task

            yield beetmover_task
