# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import attr
from mozilla_version.gecko import FirefoxVersion

from ..cli import BaseTryParser
from ..push import push_to_try


class ReleaseParser(BaseTryParser):
    name = 'release'
    arguments = [
        [['-v', '--version'],
         {'metavar': 'STR',
          'required': True,
          'action': 'store',
          'type': FirefoxVersion.parse,
          'help': "The version number to use for the staging release.",
          }],
    ]
    common_groups = ['push']


def run_try_release(version, push=True, message='{msg}', **kwargs):

    if version.is_beta:
        app_version = attr.evolve(version, beta_number=None)
    else:
        app_version = version

    files_to_change = {
        'browser/config/version.txt': '{}\n'.format(app_version),
        'browser/config/version_display.txt': '{}\n'.format(version),
    }

    release_type = version.version_type.name.lower()
    if release_type not in ('beta', 'release', 'esr'):
        raise Exception(
            "Can't do staging release for version: {} type: {}".format(
                version, version.version_type))
    task_config = {
        'version': 2,
        'parameters': {
            'target_tasks_method': 'staging_release_builds',
            'optimize_target_tasks': True,
            'include_nightly': True,
            'release_type': release_type,
        },
    }

    msg = 'staging release: {}'.format(version)
    return push_to_try(
        'release', message.format(msg=msg),
        push=push, closed_tree=kwargs["closed_tree"],
        try_task_config=task_config,
        files_to_change=files_to_change,
    )
