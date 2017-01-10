# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import logging

from . import transform
from ..util.yaml import load_yaml

logger = logging.getLogger(__name__)


class PostBuildTask(transform.TransformTask):
    """
    A task implementing a post-build job.  These depend on jobs and perform
    various followup tasks after a build has completed.

    The `only-for-build-platforms` kind configuration, if specified, will limit
    the build platforms for which a post-build task will be created.

    The `job-template' kind configuration points to a yaml file which will
    be used to create the input to the transforms.  It will have added to it
    keys `build-label`, the label for the build task, and `build-platform`, its
    platform.
    """

    @classmethod
    def get_inputs(cls, kind, path, config, params, loaded_tasks):
        if config.get('kind-dependencies', []) != ["build"]:
            raise Exception("PostBuildTask kinds must depend on builds")

        only_platforms = config.get('only-for-build-platforms')
        prototype = load_yaml(path, config.get('job-template'))

        for task in loaded_tasks:
            if task.kind != 'build':
                continue

            build_platform = task.attributes.get('build_platform')
            build_type = task.attributes.get('build_type')
            if not build_platform or not build_type:
                continue
            platform = "{}/{}".format(build_platform, build_type)
            if only_platforms and platform not in only_platforms:
                continue

            post_task = copy.deepcopy(prototype)
            post_task['build-label'] = task.label
            post_task['build-platform'] = platform
            post_task['build-task'] = task
            yield post_task
