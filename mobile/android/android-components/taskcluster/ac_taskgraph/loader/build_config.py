# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os

from copy import deepcopy
from taskgraph.loader.transform import loader as base_loader
from taskgraph.util.templates import merge

from ..build_config import get_components


def loader(kind, path, config, params, loaded_tasks):
    not_for_components = config.get("not-for-components", [])
    jobs = {
        '{}{}'.format(
            '' if build_type == 'regular' else build_type + '-',
            component['name']
        ): {
            'attributes': {
                'build-type': build_type,
                'component': component['name'],
            }
        }
        for component in get_components()
        for build_type in ('regular', 'release', 'snapshot')
        if (
            component['name'] not in not_for_components
            and (component['shouldPublish'] or build_type == 'regular')
        )
    }
    jobs = merge(jobs, config.pop('overriden-jobs', {}))

    config['jobs'] = jobs

    return base_loader(kind, path, config, params, loaded_tasks)
