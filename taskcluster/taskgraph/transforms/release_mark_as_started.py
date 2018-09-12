
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add from parameters.yml into Balrog publishing tasks.
"""

from __future__ import absolute_import, print_function, unicode_literals

import json

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.l10n import parse_locales_file
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config

transforms = TransformSequence()


@transforms.add
def make_task_description(config, jobs):
    release_config = get_release_config(config)
    for job in jobs:
        resolve_keyed_by(
            job, 'worker-type', item_name=job['name'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'scopes', item_name=job['name'], project=config.params['project']
        )

        job['worker']['release-name'] = '{product}-{version}-build{build_number}'.format(
            product=job['shipping-product'].capitalize(),
            version=release_config['version'],
            build_number=release_config['build_number']
        )
        job['worker']['product'] = job['shipping-product']
        branch = config.params['head_repository'].split('https://hg.mozilla.org/')[1]
        job['worker']['branch'] = branch

        # locales files has different structure between mobile and desktop
        locales_file = job['locales-file']
        all_locales = {}

        if job['shipping-product'] == 'fennec':
            with open(locales_file, mode='r') as f:
                all_locales = json.dumps(json.load(f))
        else:
            all_locales = "\n".join([
                "{} {}".format(locale, revision)
                for locale, revision in sorted(parse_locales_file(job['locales-file']).items())
            ])

        job['worker']['locales'] = all_locales
        del job['locales-file']

        yield job
