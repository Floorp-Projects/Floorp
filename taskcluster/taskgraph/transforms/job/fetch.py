# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Support for running tasks that download remote content and re-export
# it as task artifacts.

from __future__ import absolute_import, unicode_literals

import os

from voluptuous import (
    Optional,
    Required,
)

import taskgraph

from . import (
    run_job_using,
)
from ...util.cached_tasks import (
    add_optimization,
)
from ...util.schema import (
    Schema,
)


CACHE_TYPE = 'content.v1'


url_schema = Schema({
    Required('using'): 'fetch-url',

    # Base work directory used to set up the task.
    Required('workdir'): basestring,

    # The URL to download.
    Required('url'): basestring,

    # The SHA-256 of the downloaded content.
    Required('sha256'): basestring,

    # Size of the downloaded entity, in bytes.
    Required('size'): int,

    # GPG signature verification.
    Optional('gpg-signature'): {
        # URL where GPG signature document can be obtained. Can contain the
        # value ``{url}``, which will be substituted with the value from
        # ``url``.
        Required('sig-url'): basestring,
        # Path to file containing GPG public key(s) used to validate download.
        Required('key-path'): basestring,
    },

    # The name to give to the generated artifact.
    Optional('artifact-name'): basestring,

    # IMPORTANT: when adding anything that changes the behavior of the task,
    # it is important to update the digest data used to compute cache hits.
})


@run_job_using('docker-worker', 'fetch-url',
               schema=url_schema)
def cache_url(config, job, taskdesc):
    """Configure a task to download a URL and expose it as an artifact."""
    run = job['run']

    worker = taskdesc['worker']
    worker['chain-of-trust'] = True

    # Fetch tasks are idempotent and immutable. Have them live for
    # essentially forever.
    if config.params['level'] == '3':
        expires = '1000 years'
    else:
        expires = '28 days'

    taskdesc['expires-after'] = expires

    artifact_name = run.get('artifact-name')
    if not artifact_name:
        artifact_name = run['url'].split('/')[-1]

    worker.setdefault('artifacts', []).append({
        'type': 'directory',
        'name': 'public',
        'path': '/builds/worker/artifacts',
    })

    env = worker.setdefault('env', {})

    args = [
        '/builds/worker/bin/fetch-content', 'static-url',
        '--sha256', run['sha256'],
        '--size', '%d' % run['size'],
    ]

    if 'gpg-signature' in run:
        sig_url = run['gpg-signature']['sig-url'].format(url=run['url'])
        key_path = os.path.join(taskgraph.GECKO, run['gpg-signature'][
            'key-path'])

        with open(key_path, 'rb') as fh:
            gpg_key = fh.read()

        env['FETCH_GPG_KEY'] = gpg_key
        args.extend([
            '--gpg-sig-url', sig_url,
            '--gpg-key-env', 'FETCH_GPG_KEY',
        ])

    args.extend([
        run['url'], '/builds/worker/artifacts/%s' % artifact_name,
    ])

    worker['command'] = ['/builds/worker/bin/run-task', '--'] + args

    attributes = taskdesc.setdefault('attributes', {})
    attributes['fetch-artifact'] = 'public/%s' % artifact_name

    if not taskgraph.fast:
        cache_name = taskdesc['label'].replace('{}-'.format(config.kind), '', 1)

        # This adds the level to the index path automatically.
        add_optimization(
            config,
            taskdesc,
            cache_type=CACHE_TYPE,
            cache_name=cache_name,
            # We don't include the GPG signature in the digest because it isn't
            # materially important for caching: GPG signatures are supplemental
            # trust checking beyond what the shasum already provides.
            digest_data=[run['sha256'], '%d' % run['size'], artifact_name],
        )
