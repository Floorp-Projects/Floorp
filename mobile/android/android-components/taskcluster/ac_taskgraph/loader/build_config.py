# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os

from copy import deepcopy
from taskgraph.loader.transform import loader as base_loader

from ..build_config import get_components


def loader(kind, path, config, params, loaded_tasks):
    config['jobs'] = jobs = {
        component['name']: {}
        for component in get_components()
    }
    jobs.update(config.pop('overriden-jobs', {}))

    for job_name, job_config in config.pop('additional-jobs', {}).iteritems():
        if job_name in jobs:
            raise ValueError('"{}" already exists in config["jobs"]'.format(job_name))
        jobs[job_name] = job_config

    return base_loader(kind, path, config, params, loaded_tasks)
