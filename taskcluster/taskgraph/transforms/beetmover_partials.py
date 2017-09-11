# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add partial update artifacts to a beetmover task.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.partials import (get_balrog_platform_name,
                                     get_partials_artifacts,
                                     get_partials_artifact_map)

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


def generate_upstream_artifacts(release_history, platform, locale=None):
    if not locale or locale == 'en-US':
        artifact_prefix = 'public/build'
    else:
        artifact_prefix = 'public/build/{}'.format(locale)

    artifacts = get_partials_artifacts(release_history, platform, locale)

    upstream_artifacts = [{
        'taskId': {'task-reference': '<partials-signing>'},
        'taskType': 'signing',
        'paths': ["{}/{}".format(artifact_prefix, p)
                  for p in artifacts],
        'locale': locale or 'en-US',
    }]

    return upstream_artifacts


@transforms.add
def make_partials_artifacts(config, jobs):
    for job in jobs:
        locale = job["attributes"].get("locale")
        if locale:
            job['treeherder']['symbol'] = 'pBM({})'.format(locale)
        else:
            locale = 'en-US'
            job['treeherder']['symbol'] = 'pBM(N)'

        # Remove when proved reliable
        job['treeherder']['tier'] = 3

        platform = job["attributes"]["build_platform"]

        platform = get_balrog_platform_name(platform)
        upstream_artifacts = generate_upstream_artifacts(
            config.params.get('release_history'), platform, locale
        )

        job['worker']['upstream-artifacts'].extend(upstream_artifacts)

        extra = list()

        artifact_map = get_partials_artifact_map(
            config.params.get('release_history'), platform, locale)
        for artifact in artifact_map:
            extra.append({
                'locale': locale,
                'artifact_name': artifact,
                'buildid': artifact_map[artifact],
                'platform': platform,
            })

        job.setdefault('extra', {})
        job['extra']['partials'] = extra

        yield job
