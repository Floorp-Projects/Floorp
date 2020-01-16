# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re

from collections import deque
import taskgraph
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import _run_task_suffix
from .. import GECKO
from taskgraph.util.docker import (
    generate_context_hash,
)
from taskgraph.util.taskcluster import get_root_url
from taskgraph.util.schema import (
    Schema,
)
from voluptuous import (
    Optional,
    Required,
)
from .task import task_description_schema

DIGEST_RE = re.compile('^[0-9a-f]{64}$')

transforms = TransformSequence()

docker_image_schema = Schema({
    # Name of the docker image.
    Required('name'): basestring,

    # Name of the parent docker image.
    Optional('parent'): basestring,

    # Treeherder symbol.
    Required('symbol'): basestring,

    # relative path (from config.path) to the file the docker image was defined
    # in.
    Optional('job-from'): basestring,

    # Arguments to use for the Dockerfile.
    Optional('args'): {basestring: basestring},

    # Name of the docker image definition under taskcluster/docker, when
    # different from the docker image name.
    Optional('definition'): basestring,

    # List of package tasks this docker image depends on.
    Optional('packages'): [basestring],

    Optional(
        "index",
        description="information for indexing this build so its artifacts can be discovered",
    ): task_description_schema['index'],

    Optional(
        "cache",
        description="Whether this image should be cached based on inputs.",
    ): bool,
})


transforms.add_validate(docker_image_schema)


def order_image_tasks(config, tasks):
    """Iterate image tasks in an order where parent images come first."""
    pending = deque(tasks)
    task_names = {task['name'] for task in pending}
    emitted = set()
    while True:
        try:
            task = pending.popleft()
        except IndexError:
            break
        parent = task.get('parent')
        if parent and parent not in emitted:
            if parent not in task_names:
                raise Exception('Missing parent image for {}-{}: {}'.format(
                    config.kind, task['name'], parent))
            pending.append(task)
            continue
        emitted.add(task['name'])
        yield task


@transforms.add
def fill_template(config, tasks):
    available_packages = set()
    for task in config.kind_dependencies_tasks:
        if task.kind != 'packages':
            continue
        name = task.label.replace('packages-', '')
        available_packages.add(name)

    context_hashes = {}

    for task in order_image_tasks(config, tasks):
        image_name = task.pop('name')
        job_symbol = task.pop('symbol')
        args = task.pop('args', {})
        definition = task.pop('definition', image_name)
        packages = task.pop('packages', [])
        parent = task.pop('parent', None)

        for p in packages:
            if p not in available_packages:
                raise Exception('Missing package job for {}-{}: {}'.format(
                    config.kind, image_name, p))

        # Generating the context hash relies on arguments being set, so we
        # set this now, although it's not the final value (it's a
        # task-reference value, see further below). We add the package routes
        # containing a hash to get the overall docker image hash, so changes
        # to packages will be reflected in the docker image hash.
        args['DOCKER_IMAGE_PACKAGES'] = ' '.join('<{}>'.format(p)
                                                 for p in packages)
        if parent:
            args['DOCKER_IMAGE_PARENT'] = '{}:{}'.format(parent, context_hashes[parent])

        args['TASKCLUSTER_ROOT_URL'] = get_root_url(False)

        if not taskgraph.fast:
            context_path = os.path.join('taskcluster', 'docker', definition)
            context_hash = generate_context_hash(
                GECKO, context_path, image_name, args)
        else:
            context_hash = '0'*40
        digest_data = [context_hash]
        context_hashes[image_name] = context_hash

        description = 'Build the docker image {} for use by dependent tasks'.format(
            image_name)

        # Adjust the zstandard compression level based on the execution level.
        # We use faster compression for level 1 because we care more about
        # end-to-end times. We use slower/better compression for other levels
        # because images are read more often and it is worth the trade-off to
        # burn more CPU once to reduce image size.
        zstd_level = '3' if int(config.params['level']) == 1 else '10'

        # include some information that is useful in reconstructing this task
        # from JSON
        taskdesc = {
            'label': 'build-docker-image-' + image_name,
            'description': description,
            'attributes': {'image_name': image_name},
            'expires-after': '28 days' if config.params.is_try() else '1 year',
            'scopes': [
                'secrets:get:project/taskcluster/gecko/hgfingerprint',
                'secrets:get:project/taskcluster/gecko/hgmointernal',
            ],
            'treeherder': {
                'symbol': job_symbol,
                'platform': 'taskcluster-images/opt',
                'kind': 'other',
                'tier': 1,
            },
            'run-on-projects': [],
            'worker-type': 'images',
            'worker': {
                'implementation': 'docker-worker',
                'os': 'linux',
                'artifacts': [{
                    'type': 'file',
                    'path': '/builds/worker/workspace/artifacts/image.tar.zst',
                    'name': 'public/image.tar.zst',
                }],
                'env': {
                    'HG_STORE_PATH': '/builds/worker/checkouts/hg-store',
                    'HASH': context_hash,
                    'PROJECT': config.params['project'],
                    'IMAGE_NAME': image_name,
                    'DOCKER_IMAGE_ZSTD_LEVEL': zstd_level,
                    'GECKO_BASE_REPOSITORY': config.params['base_repository'],
                    'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
                    'GECKO_HEAD_REV': config.params['head_rev'],
                },
                'chain-of-trust': True,
                'docker-in-docker': True,
                'taskcluster-proxy': True,
                'max-run-time': 7200,
                # Retry on apt-get errors.
                'retry-exit-status': [100],
            },
        }
        # Retry for 'funsize-update-generator' if exit status code is -1
        if image_name in ['funsize-update-generator']:
            taskdesc['worker']['retry-exit-status'] = [-1]

        worker = taskdesc['worker']

        # We use the in-tree image_builder image to build docker images, but
        # that can't be used to build the image_builder image itself,
        # obviously. So we fall back to an image on docker hub, identified
        # by hash.  After the image-builder image is updated, it's best to push
        # and update this hash as well, to keep image-builder builds up to date.
        if image_name == 'image_builder':
            hash = 'sha256:c6622fd3e5794842ad83d129850330b26e6ba671e39c58ee288a616a3a1c4c73'
            worker['docker-image'] = 'taskcluster/image_builder@' + hash
            # Keep in sync with the Dockerfile used to generate the
            # docker image whose digest is referenced above.
            worker['volumes'] = [
                '/builds/worker/checkouts',
                '/builds/worker/workspace',
            ]
            cache_name = 'imagebuilder-v1'
        else:
            worker['docker-image'] = {'in-tree': 'image_builder'}
            cache_name = 'imagebuilder-sparse-{}'.format(_run_task_suffix())
            # Force images built against the in-tree image builder to
            # have a different digest by adding a fixed string to the
            # hashed data.
            # Append to this data whenever the image builder's output behavior
            # is changed, in order to force all downstream images to be rebuilt and
            # cached distinctly.
            digest_data.append('image_builder')
            # Updated for squashing images in Bug 1527394
            digest_data.append('squashing layers')

        worker['caches'] = [{
            'type': 'persistent',
            'name': cache_name,
            'mount-point': '/builds/worker/checkouts',
        }]

        for k, v in args.items():
            if k == 'DOCKER_IMAGE_PACKAGES':
                worker['env'][k] = {'task-reference': v}
            else:
                worker['env'][k] = v

        if packages:
            deps = taskdesc.setdefault('dependencies', {})
            for p in sorted(packages):
                deps[p] = 'packages-{}'.format(p)

        if parent:
            deps = taskdesc.setdefault('dependencies', {})
            deps[parent] = 'build-docker-image-{}'.format(parent)
            worker['env']['DOCKER_IMAGE_PARENT_TASK'] = {
                'task-reference': '<{}>'.format(parent),
            }
        if 'index' in task:
            taskdesc['index'] = task['index']

        if task.get('cache', True) and not taskgraph.fast:
            taskdesc['cache'] = {
                'type': 'docker-images.v2',
                'name': image_name,
                'digest-data': digest_data,
            }

        yield taskdesc
