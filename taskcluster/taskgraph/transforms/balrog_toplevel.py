# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import yaml

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.scriptworker import (
    get_release_config,
)

from mozrelease.balrog import generate_update_properties
from mozilla_version.gecko import GeckoVersion

transforms = TransformSequence()


@transforms.add
def generate_update_line(config, jobs):
    """Resolve fields that can be keyed by platform, etc."""
    release_config = get_release_config(config)
    for job in jobs:
        config_file = job.pop('whats-new-config')
        with open(config_file, "rb") as f:
            update_config = yaml.safe_load(f)

        job['worker']['update-line'] = {}
        for blob_type, suffix in [('wnp', ''), ('no-wnp', '-No-WNP')]:
            context = {
                'release-type': config.params['release_type'],
                'product': job['shipping-product'],
                'version': GeckoVersion.parse(release_config['version']),
                'blob-type': blob_type,
            }
            job['worker']['update-line'][suffix] = generate_update_properties(
                context, update_config)

        yield job
