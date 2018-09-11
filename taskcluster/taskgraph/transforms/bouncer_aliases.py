# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add from parameters.yml into bouncer submission tasks.
"""

from __future__ import absolute_import, print_function, unicode_literals

import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.bouncer_submission import craft_bouncer_product_name
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'worker-type', item_name=job['name'],
            **{'release-level': config.params.release_level()}
        )
        resolve_keyed_by(
            job, 'scopes', item_name=job['name'],
            **{'release-level': config.params.release_level()}
        )
        resolve_keyed_by(
            job, 'bouncer-products-per-alias',
            item_name=job['name'], project=config.params['project']
        )

        job['worker']['entries'] = craft_bouncer_entries(config, job)

        del job['bouncer-products-per-alias']

        if job['worker']['entries']:
            yield job
        else:
            logger.warn('No bouncer entries defined in bouncer submission task for "{}". \
Job deleted.'.format(job['name']))


def craft_bouncer_entries(config, job):
    release_config = get_release_config(config)

    product = job['shipping-product']
    current_version = release_config['version']
    bouncer_products_per_alias = job['bouncer-products-per-alias']

    return {
        bouncer_alias: craft_bouncer_product_name(
            product, bouncer_product, current_version,
        )
        for bouncer_alias, bouncer_product in bouncer_products_per_alias.items()
    }
