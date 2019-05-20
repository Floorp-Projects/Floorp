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
        dependent_tasks = get_dependent_loaded_tasks(config, params, loaded_tasks, job)
        if not dependent_tasks:
            # PushApk must depend on signed APK. If no dependent task was found,
            # this means another plaform (like windows) is being processed
            continue

        job['dependent-tasks'] = dependent_tasks
        job['label'] = job['name']

        yield job


def get_dependent_loaded_tasks(config, params, loaded_tasks, job):
    nightly_tasks = (
        task for task in loaded_tasks if task.attributes.get('nightly')
    )
    tasks_with_matching_kind = (
        task for task in nightly_tasks if task.kind in config.get('kind-dependencies')
    )
    tasks = (
        task for task in tasks_with_matching_kind
        if task.attributes.get('shipping_product') == 'fennec'
        and task.attributes.get('release-type') == job['attributes']['release-type']
    )

    # XXX Bug 1368484: Aarch64 is not planned to ride the trains regularly. It stayed on central
    # for a couple of cycles, and is planned to stay on mozilla-beta until 68.
    if params['project'] in ('mozilla-central', 'mozilla-beta', 'try'):
        shipping_tasks = list(tasks)
    else:
        shipping_tasks = [
            task for task in tasks
            if 'aarch64' not in task.attributes.get('build_platform', '')
        ]

    return shipping_tasks
