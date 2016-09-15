# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transforms construct a task description to run the given test, based on a
test description.  The implementation here is shared among all test kinds, but
contains specific support for how we run tests in Gecko (via mozharness,
invoked in particular ways).

This is a good place to translate a test-description option such as
`single-core: true` to the implementation of that option in a task description
(worker options, mozharness commandline, environment variables, etc.)

The test description should be fully formed by the time it reaches these
transforms, and these transforms should not embody any specific knowledge about
what should run where. this is the wrong place for special-casing platforms,
for example - use `all_tests.py` instead.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.job.common import (
    docker_worker_support_vcs_checkout,
)

import logging

ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'

ARTIFACTS = [
    # (artifact name prefix, in-image path)
    ("public/logs/", "/home/worker/workspace/build/upload/logs/"),
    ("public/test", "/home/worker/artifacts/"),
    ("public/test_info/", "/home/worker/workspace/build/blobber_upload_dir/"),
]

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def make_task_description(config, tests):
    """Convert *test* descriptions to *task* descriptions (input to
    taskgraph.transforms.task)"""

    for test in tests:
        label = '{}-{}-{}'.format(config.kind, test['test-platform'], test['test-name'])
        if test['chunks'] > 1:
            label += '-{}'.format(test['this-chunk'])

        build_label = test['build-label']

        unittest_try_name = test.get('unittest-try-name', test['test-name'])

        attr_build_platform, attr_build_type = test['build-platform'].split('/', 1)

        suite = test['suite']
        if '/' in suite:
            suite, flavor = suite.split('/', 1)
        else:
            flavor = suite

        attributes = test.get('attributes', {})
        attributes.update({
            'build_platform': attr_build_platform,
            'build_type': attr_build_type,
            # only keep the first portion of the test platform
            'test_platform': test['test-platform'].split('/')[0],
            'test_chunk': str(test['this-chunk']),
            'unittest_suite': suite,
            'unittest_flavor': flavor,
            'unittest_try_name': unittest_try_name,
        })

        taskdesc = {}
        taskdesc['label'] = label
        taskdesc['description'] = test['description']
        taskdesc['attributes'] = attributes
        taskdesc['dependencies'] = {'build': build_label}
        taskdesc['deadline-after'] = '1 day'
        taskdesc['expires-after'] = test['expires-after']
        taskdesc['routes'] = []
        taskdesc['scopes'] = []
        taskdesc['extra'] = {
            'chunks': {
                'current': test['this-chunk'],
                'total': test['chunks'],
            },
            'suite': {
                'name': suite,
                'flavor': flavor,
            },
        }
        taskdesc['treeherder'] = {
            'symbol': test['treeherder-symbol'],
            'kind': 'test',
            'tier': test['tier'],
            'platform': test.get('treeherder-machine-platform', test['build-platform']),
        }

        # the remainder (the worker-type and worker) differs depending on the
        # worker implementation
        worker_setup_functions[test['worker-implementation']](config, test, taskdesc)

        # yield only the task description, discarding the test description
        yield taskdesc


worker_setup_functions = {}


def worker_setup_function(name):
    def wrap(func):
        worker_setup_functions[name] = func
        return func
    return wrap


@worker_setup_function("docker-engine")
@worker_setup_function("docker-worker")
def docker_worker_setup(config, test, taskdesc):
    mozharness = test['mozharness']

    installer_url = ARTIFACT_URL.format('<build>', mozharness['build-artifact-name'])
    test_packages_url = ARTIFACT_URL.format('<build>',
                                            'public/build/target.test_packages.json')
    mozharness_url = ARTIFACT_URL.format('<build>',
                                         'public/build/mozharness.zip')

    taskdesc['worker-type'] = {
        'default': 'aws-provisioner-v1/desktop-test-large',
        'large': 'aws-provisioner-v1/desktop-test-large',
        'xlarge': 'aws-provisioner-v1/desktop-test-xlarge',
        'legacy': 'aws-provisioner-v1/desktop-test',
    }[test['instance-size']]

    worker = taskdesc['worker'] = {}
    worker['implementation'] = test['worker-implementation']
    worker['docker-image'] = test['docker-image']

    worker['allow-ptrace'] = True  # required for all tests, for crashreporter
    worker['relengapi-proxy'] = False  # but maybe enabled for tooltool below
    worker['loopback-video'] = test['loopback-video']
    worker['loopback-audio'] = test['loopback-audio']
    worker['max-run-time'] = test['max-run-time']

    worker['artifacts'] = [{
        'name': prefix,
        'path': path,
        'type': 'directory',
    } for (prefix, path) in ARTIFACTS]

    worker['caches'] = [{
        'type': 'persistent',
        'name': 'level-{}-{}-test-workspace'.format(
            config.params['level'], config.params['project']),
        'mount-point': "/home/worker/workspace",
    }]

    env = worker['env'] = {
        'MOZHARNESS_CONFIG': ' '.join(mozharness['config']),
        'MOZHARNESS_SCRIPT': mozharness['script'],
        'MOZHARNESS_URL': {'task-reference': mozharness_url},
        'MOZILLA_BUILD_URL': {'task-reference': installer_url},
        'NEED_PULSEAUDIO': 'true',
        'NEED_WINDOW_MANAGER': 'true',
    }

    if mozharness['set-moz-node-path']:
        env['MOZ_NODE_PATH'] = '/usr/local/bin/node'

    if 'actions' in mozharness:
        env['MOZHARNESS_ACTIONS'] = ' '.join(mozharness['actions'])

    if config.params['project'] == 'try':
        env['TRY_COMMIT_MSG'] = config.params['message']

    # handle some of the mozharness-specific options

    if mozharness['tooltool-downloads']:
        worker['relengapi-proxy'] = True
        worker['caches'].append({
            'type': 'persistent',
            'name': 'tooltool-cache',
            'mount-point': '/home/worker/tooltool-cache',
        })
        taskdesc['scopes'].extend([
            'docker-worker:relengapi-proxy:tooltool.download.internal',
            'docker-worker:relengapi-proxy:tooltool.download.public',
        ])

    # assemble the command line
    command = [
        '/home/worker/bin/run-task',
        # The workspace cache/volume is default owned by root:root.
        '--chown', '/home/worker/workspace',
    ]

    if test['checkout']:
        docker_worker_support_vcs_checkout(config, test, taskdesc)
        command.extend(['--vcs-checkout', '/home/worker/checkouts/gecko'])

    command.extend([
        '--',
        '/home/worker/bin/test-linux.sh',
    ])

    if mozharness.get('no-read-buildbot-config'):
        command.append("--no-read-buildbot-config")
    command.extend([
        {"task-reference": "--installer-url=" + installer_url},
        {"task-reference": "--test-packages-url=" + test_packages_url},
    ])
    command.extend(mozharness.get('extra-options', []))

    # TODO: remove the need for run['chunked']
    if mozharness.get('chunked') or test['chunks'] > 1:
        # Implement mozharness['chunking-args'], modifying command in place
        if mozharness['chunking-args'] == 'this-chunk':
            command.append('--total-chunk={}'.format(test['chunks']))
            command.append('--this-chunk={}'.format(test['this-chunk']))
        elif mozharness['chunking-args'] == 'test-suite-suffix':
            suffix = mozharness['chunk-suffix'].replace('<CHUNK>', str(test['this-chunk']))
            for i, c in enumerate(command):
                if isinstance(c, basestring) and c.startswith('--test-suite'):
                    command[i] += suffix

    if 'download-symbols' in mozharness:
        download_symbols = mozharness['download-symbols']
        download_symbols = {True: 'true', False: 'false'}.get(download_symbols, download_symbols)
        command.append('--download-symbols=' + download_symbols)

    worker['command'] = command
