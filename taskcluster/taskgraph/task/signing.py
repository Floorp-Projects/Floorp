# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from . import transform


class SigningTask(transform.TransformTask):
    """
    A task implementing a signing job.  These depend on nightly build jobs and
    sign the artifacts after a build has completed.
    """

    @classmethod
    def get_inputs(cls, kind, path, config, params, loaded_tasks):
        if (config.get('kind-dependencies', []) != ["build"] and
                config.get('kind-dependencies', []) != ["nightly-l10n"]):
            raise Exception("Signing kinds must depend on builds or l10n repacks")
        for task in loaded_tasks:
            if task.kind not in config.get('kind-dependencies'):
                continue
            if not task.attributes.get('nightly'):
                continue
            signing_task = {'dependent-task': task}

            yield signing_task
