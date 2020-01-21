# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Support for running tasks that download remote content and re-export
# it as task artifacts.

from __future__ import absolute_import, unicode_literals

from mozbuild.shellutil import quote as shell_quote

import io
import os
import re
from six import text_type

from voluptuous import (
    Any,
    Optional,
    Required,
)

import taskgraph

from .base import (
    TransformSequence,
)
from ..util.cached_tasks import (
    add_optimization,
)
from ..util.schema import (
    Schema,
)
from ..util.treeherder import (
    join_symbol,
)


CACHE_TYPE = 'content.v1'

FETCH_SCHEMA = Schema({
    # Name of the task.
    Required('name'): text_type,

    # Relative path (from config.path) to the file the task was defined
    # in.
    Optional('job-from'): text_type,

    # Description of the task.
    Required('description'): text_type,

    Required('fetch'): Any(
        {
            'type': 'static-url',

            # The URL to download.
            Required('url'): text_type,

            # The SHA-256 of the downloaded content.
            Required('sha256'): text_type,

            # Size of the downloaded entity, in bytes.
            Required('size'): int,

            # GPG signature verification.
            Optional('gpg-signature'): {
                # URL where GPG signature document can be obtained. Can contain the
                # value ``{url}``, which will be substituted with the value from
                # ``url``.
                Required('sig-url'): text_type,
                # Path to file containing GPG public key(s) used to validate
                # download.
                Required('key-path'): text_type,
            },

            # The name to give to the generated artifact. Defaults to the file
            # portion of the URL. Using a different extension converts the
            # archive to the given type. Only conversion to .tar.zst is
            # supported.
            Optional('artifact-name'): text_type,

            # Strip the given number of path components at the beginning of
            # each file entry in the archive.
            # Requires an artifact-name ending with .tar.zst.
            Optional('strip-components'): int,

            # Add the given prefix to each file entry in the archive.
            # Requires an artifact-name ending with .tar.zst.
            Optional('add-prefix'): text_type,

            # IMPORTANT: when adding anything that changes the behavior of the task,
            # it is important to update the digest data used to compute cache hits.
        },
        {
            'type': 'chromium-fetch',

            Required('script'): text_type,

            # Platform type for chromium build
            Required('platform'): text_type,

            # Chromium revision to obtain
            Optional('revision'): text_type,

            # The name to give to the generated artifact.
            Required('artifact-name'): text_type
        },
        {
            'type': 'git',
            Required('repo'): text_type,
            Required('revision'): text_type,
            Optional('artifact-name'): text_type,
            Optional('path-prefix'): text_type,
        }
    ),
})

transforms = TransformSequence()
transforms.add_validate(FETCH_SCHEMA)


@transforms.add
def process_fetch_job(config, jobs):
    # Converts fetch-url entries to the job schema.
    for job in jobs:
        if 'fetch' not in job:
            continue

        typ = job['fetch']['type']

        if typ == 'static-url':
            yield create_fetch_url_task(config, job)
        elif typ == 'chromium-fetch':
            yield create_chromium_fetch_task(config, job)
        elif typ == 'git':
            yield create_git_fetch_task(config, job)
        else:
            # validate() should have caught this.
            assert False


def make_base_task(config, name, description, command):
    # Fetch tasks are idempotent and immutable. Have them live for
    # essentially forever.
    if config.params['level'] == '3':
        expires = '1000 years'
    else:
        expires = '28 days'

    return {
        'attributes': {},
        'name': name,
        'description': description,
        'expires-after': expires,
        'label': 'fetch-%s' % name,
        'run-on-projects': [],
        'treeherder': {
            'kind': 'build',
            'platform': 'fetch/opt',
            'tier': 1,
        },
        'run': {
            'using': 'run-task',
            'checkout': False,
            'command': command,
        },
        'worker-type': 'images',
        'worker': {
            'chain-of-trust': True,
            'docker-image': {'in-tree': 'fetch'},
            'env': {},
            'max-run-time': 900,
        },
    }


def create_fetch_url_task(config, job):
    name = job['name']
    fetch = job['fetch']

    artifact_name = fetch.get('artifact-name')
    if not artifact_name:
        artifact_name = fetch['url'].split('/')[-1]

    command = [
        '/builds/worker/bin/fetch-content', 'static-url',
    ]

    # Arguments that matter to the cache digest
    args = [
        '--sha256', fetch['sha256'],
        '--size', '%d' % fetch['size'],
    ]

    if fetch.get('strip-components'):
        args.extend(['--strip-components', '%d' % fetch['strip-components']])

    if fetch.get('add-prefix'):
        args.extend(['--add-prefix', fetch['add-prefix']])

    command.extend(args)

    env = {}

    if 'gpg-signature' in fetch:
        sig_url = fetch['gpg-signature']['sig-url'].format(url=fetch['url'])
        key_path = os.path.join(taskgraph.GECKO, fetch['gpg-signature'][
            'key-path'])

        with io.open(key_path, 'r') as fh:
            gpg_key = fh.read()

        env['FETCH_GPG_KEY'] = gpg_key
        command.extend([
            '--gpg-sig-url', sig_url,
            '--gpg-key-env', 'FETCH_GPG_KEY',
        ])

    command.extend([
        fetch['url'], '/builds/worker/artifacts/%s' % artifact_name,
    ])

    task = make_base_task(config, name, job['description'], command)
    task['treeherder']['symbol'] = join_symbol('Fetch', name)
    task['worker']['artifacts'] = [{
        'type': 'directory',
        'name': 'public',
        'path': '/builds/worker/artifacts',
    }]
    task['worker']['env'] = env
    task['attributes']['fetch-artifact'] = 'public/%s' % artifact_name

    if not taskgraph.fast:
        cache_name = task['label'].replace('{}-'.format(config.kind), '', 1)

        # This adds the level to the index path automatically.
        add_optimization(
            config,
            task,
            cache_type=CACHE_TYPE,
            cache_name=cache_name,
            # We don't include the GPG signature in the digest because it isn't
            # materially important for caching: GPG signatures are supplemental
            # trust checking beyond what the shasum already provides.
            digest_data=args + [artifact_name],
        )

    return task


def create_git_fetch_task(config, job):
    name = job['name']
    fetch = job['fetch']
    path_prefix = fetch.get('path-prefix')
    if not path_prefix:
        path_prefix = fetch['repo'].rstrip('/').rsplit('/', 1)[-1]
    artifact_name = fetch.get('artifact-name')
    if not artifact_name:
        artifact_name = '{}.tar.zst'.format(path_prefix)

    if not re.match(r'[0-9a-fA-F]{40}', fetch['revision']):
        raise Exception(
            'Revision is not a sha1 in fetch task "{}"'.format(name))

    args = [
        '/builds/worker/bin/fetch-content',
        'git-checkout-archive',
        '--path-prefix',
        path_prefix,
        fetch['repo'],
        fetch['revision'],
        '/builds/worker/artifacts/%s' % artifact_name,
    ]

    task = make_base_task(config, name, job['description'], args)
    task['treeherder']['symbol'] = join_symbol('Fetch', name)
    task['worker']['artifacts'] = [{
        'type': 'directory',
        'name': 'public',
        'path': '/builds/worker/artifacts',
    }]
    task['attributes']['fetch-artifact'] = 'public/%s' % artifact_name

    if not taskgraph.fast:
        cache_name = task['label'].replace('{}-'.format(config.kind), '', 1)

        # This adds the level to the index path automatically.
        add_optimization(
            config,
            task,
            cache_type=CACHE_TYPE,
            cache_name=cache_name,
            digest_data=[fetch['revision'], path_prefix, artifact_name],
        )

    return task


def create_chromium_fetch_task(config, job):
    name = job['name']
    fetch = job['fetch']
    artifact_name = fetch.get('artifact-name')

    workdir = '/builds/worker'

    platform = fetch.get('platform')
    revision = fetch.get('revision')

    args = '--platform ' + shell_quote(platform)
    if revision:
        args += ' --revision ' + shell_quote(revision)

    cmd = [
        'bash',
        '-c',
        'cd {} && '
        '/usr/bin/python3 {} {}'.format(
            workdir, fetch['script'], args
        )
    ]

    env = {
        'UPLOAD_DIR': '/builds/worker/artifacts'
    }

    task = make_base_task(config, name, job['description'], cmd)
    task['treeherder']['symbol'] = join_symbol('Fetch', name)
    task['worker']['artifacts'] = [{
        'type': 'directory',
        'name': 'public',
        'path': '/builds/worker/artifacts',
    }]
    task['worker']['env'] = env
    task['attributes']['fetch-artifact'] = 'public/%s' % artifact_name

    if not taskgraph.fast:
        cache_name = task['label'].replace('{}-'.format(config.kind), '', 1)

        # This adds the level to the index path automatically.
        add_optimization(
            config,
            task,
            cache_type=CACHE_TYPE,
            cache_name=cache_name,
            digest_data=[
                "revision={}".format(revision),
                "platform={}".format(platform),
                "artifact_name={}".format(artifact_name),
            ],
        )

    return task
