# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

from ..util.templates import merge
from ..util.yaml import load_yaml

logger = logging.getLogger(__name__)


def loader(kind, path, config, params, loaded_tasks):
    """
    Get the input elements that will be transformed into tasks in a generic
    way.  The elements themselves are free-form, and become the input to the
    first transform.

    By default, this reads jobs from the `jobs` key, or from yaml files
    named by `jobs-from`.  The entities are read from mappings, and the
    keys to those mappings are added in the `name` key of each entity.

    If there is a `job-defaults` config, then every job is merged with it.
    This provides a simple way to set default values for all jobs of a kind.
    The `job-defaults` key can also be specified in a yaml file pointed to by
    `jobs-from`. In this case it will only apply to tasks defined in the same
    file.

    Other kind implementations can use a different loader function to
    produce inputs and hand them to `transform_inputs`.
    """
    def jobs():
        defaults = config.get('job-defaults')
        for name, job in config.get('jobs', {}).iteritems():
            if defaults:
                job = merge(defaults, job)
            job['job-from'] = 'kind.yml'
            yield name, job

        for filename in config.get('jobs-from', []):
            tasks = load_yaml(path, filename)

            file_defaults = tasks.pop('job-defaults', None)
            if defaults:
                file_defaults = merge(defaults, file_defaults or {})

            for name, job in tasks.iteritems():
                if file_defaults:
                    job = merge(file_defaults, job)
                job['job-from'] = filename
                yield name, job

    for name, job in jobs():
        job['name'] = name
        logger.debug("Generating tasks for {} {}".format(kind, name))
        yield job
