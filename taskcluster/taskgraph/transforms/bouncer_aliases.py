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
            job, 'worker-type', item_name=job['name'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'scopes', item_name=job['name'], project=config.params['project']
        )

        job['scopes'].append('project:releng:bouncer:action:aliases')
        job['worker']['entries'] = craft_bouncer_entries(config, job)

        del job['bouncer-products']

        if job['worker']['entries']:
            yield job
        else:
            logger.warn('No bouncer entries defined in bouncer submission task for "{}". \
Job deleted.'.format(job['name']))


def craft_bouncer_entries(config, job):
    release_config = get_release_config(config)

    product = job['shipping-product']
    release_type = config.params['release_type']
    # The release_type is defined but may point to None.
    if not release_type:
        release_type = ''
    current_version = release_config['version']
    bouncer_products = job['bouncer-products']

    return {
        craft_bouncer_alias(product, bouncer_product, release_type): craft_bouncer_product_name(
            product, bouncer_product, current_version,
        )
        for bouncer_product in bouncer_products
    }


def craft_bouncer_alias(product, bouncer_product, release_type):
    return '{product}{channel}{postfix}'.format(
        product=_craft_product(product),
        channel=_craft_channel_string_of_alias(product, release_type),
        postfix=_craft_alias_postfix(bouncer_product)
    )


def _craft_product(product):
    # XXX devedition is provided in the channel function
    return 'firefox' if product == 'devedition' else product


def _craft_channel_string_of_alias(product, release_type):
    if product == 'devedition':
        return '-devedition'
    elif release_type == 'beta':
        return '-beta'
    elif 'esr' in release_type:
        return '-esr'

    return ''


def _craft_alias_postfix(bouncer_product):
    if 'stub' in bouncer_product:
        postfix = '-stub'
    elif 'installer' in bouncer_product or bouncer_product == 'apk':
        postfix = '-latest'
        if 'ssl' in bouncer_product:
            postfix = '{}-ssl'.format(postfix)
    else:
        raise Exception('Unknown bouncer product "{}"'.format(bouncer_product))

    return postfix
