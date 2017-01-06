# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from . import transform


class RepackTask(transform.TransformTask):
    """
    A task implementing a l10n repack job.  These may depend on build jobs and
    do a repack of them
    """

    @classmethod
    def get_inputs(cls, kind, path, config, params, loaded_tasks):
        only_platforms = config.get('only-for-build-platforms')

        for task in loaded_tasks:
            if task.kind not in config.get('kind-dependencies'):
                continue

            build_platform = task.attributes.get('build_platform')
            build_type = task.attributes.get('build_type')
            if not build_platform or not build_type:
                continue
            platform = "{}/{}".format(build_platform, build_type)
            if only_platforms and platform not in only_platforms:
                continue

            repack_task = {'dependent-task': task}

            if config.get('job-template'):
                repack_task.update(config.get('job-template'))

            yield repack_task
