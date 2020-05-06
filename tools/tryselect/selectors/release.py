# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

import attr
from mozilla_version.gecko import FirefoxVersion

from ..cli import BaseTryParser
from ..push import push_to_try, vcs

TARGET_TASKS = {
    'staging': 'staging_release_builds',
    'release-sim': 'release_simulation',
}


def read_file(path):
    with open(path) as fh:
        return fh.read()


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
        [['--migration'],
         {'metavar': 'STR',
          'action': 'append',
          'dest': 'migrations',
          'choices': [
              'central-to-beta',
              'beta-to-release',
              'early-to-late-beta',
              'release-to-esr',
          ],
          'help': "Migration to run for the release (can be specified multiple times).",
          }],
        [['--no-limit-locales'],
         {'action': 'store_false',
          'dest': 'limit_locales',
          'help': "Don't build a limited number of locales in the staging release.",
          }],
        [['--tasks'],
         {'choices': TARGET_TASKS.keys(),
          'default': 'staging',
          'help': "Which tasks to run on-push.",
          }],

    ]
    common_groups = ['push']
    task_configs = ['disable-pgo', 'worker-overrides']

    def __init__(self, *args, **kwargs):
        super(ReleaseParser, self).__init__(*args, **kwargs)
        self.set_defaults(migrations=[])


def run(
    version, migrations, limit_locales, tasks,
    try_config=None, push=True, message='{msg}', closed_tree=False
):
    app_version = attr.evolve(version, beta_number=None, is_esr=False)

    files_to_change = {
        'browser/config/version.txt': '{}\n'.format(app_version),
        'browser/config/version_display.txt': '{}\n'.format(version),
        'config/milestone.txt': '{}\n'.format(app_version),
    }

    release_type = version.version_type.name.lower()
    if release_type not in ('beta', 'release', 'esr'):
        raise Exception(
            "Can't do staging release for version: {} type: {}".format(
                version, version.version_type))
    elif release_type == 'esr':
        release_type += str(version.major_number)
    task_config = {
        'version': 2,
        'parameters': {
            'target_tasks_method': TARGET_TASKS[tasks],
            'optimize_target_tasks': True,
            'release_type': release_type,
        },
    }
    if try_config:
        task_config['parameters']['try_task_config'] = try_config

    for migration in migrations:
        migration_path = os.path.join(
            vcs.path,
            'testing/mozharness/configs/merge_day',
            '{}.py'.format(migration.replace('-', '_')),
        )
        migration_config = {}
        with open(migration_path) as f:
            code = compile(f.read(), migration_path, "exec")
            exec(code, migration_config, migration_config)
        for (path, from_, to) in migration_config['config']['replacements']:
            if path in files_to_change:
                contents = files_to_change[path]
            else:
                contents = read_file(path)
            files_to_change[path] = contents.replace(from_, to)

    if limit_locales:
        files_to_change['browser/locales/l10n-changesets.json'] = read_file(
            os.path.join(vcs.path, 'browser/locales/l10n-onchange-changesets.json'))
        files_to_change['browser/locales/shipped-locales'] = "en-US\n" + read_file(
            os.path.join(vcs.path, 'browser/locales/onchange-locales'))

    msg = 'staging release: {}'.format(version)
    return push_to_try(
        'release', message.format(msg=msg),
        push=push, closed_tree=closed_tree,
        try_task_config=task_config,
        files_to_change=files_to_change,
    )
