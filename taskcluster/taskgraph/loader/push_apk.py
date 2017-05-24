# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .transform import loader as base_loader


def loader(kind, path, config, params, loaded_tasks):
    """
    Generate inputs implementing PushApk jobs. These depend on signed multi-locales nightly builds.
    """
    jobs = base_loader(kind, path, config, params, loaded_tasks)

    for job in jobs:
        job['dependent-tasks'] = get_dependent_loaded_tasks(config, loaded_tasks)
        yield job


def get_dependent_loaded_tasks(config, loaded_tasks):
    nightly_tasks = (
        task for task in loaded_tasks if task.attributes.get('nightly')
    )
    tasks_with_matching_kind = (
        task for task in nightly_tasks if task.kind in config.get('kind-dependencies')
    )
    android_tasks = [
        task for task in tasks_with_matching_kind
        if task.attributes.get('build_platform', '').startswith('android')
    ]

    return android_tasks
