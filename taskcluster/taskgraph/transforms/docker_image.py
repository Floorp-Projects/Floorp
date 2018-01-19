# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import _run_task_suffix
from .. import GECKO
from taskgraph.util.docker import (
    generate_context_hash,
)
from taskgraph.util.cached_tasks import add_optimization
from taskgraph.util.schema import (
    Schema,
    validate_schema,
)
from voluptuous import (
    Optional,
    Required,
)

DIGEST_RE = re.compile('^[0-9a-f]{64}$')

transforms = TransformSequence()

docker_image_schema = Schema({
    # Name of the docker image.
    Required('name'): basestring,

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
})


@transforms.add
def validate(config, tasks):
    for task in tasks:
        validate_schema(
            docker_image_schema, task,
            "In docker image {!r}:".format(task.get('name', 'unknown')))
        yield task


@transforms.add
def fill_template(config, tasks):
    available_packages = {}
    for task in config.kind_dependencies_tasks:
        if task.kind != 'packages':
            continue
        name = task.label.replace('packages-', '')
        for route in task.task.get('routes', []):
            if route.startswith('index.') and '.hash.' in route:
                # Only keep the hash part of the route.
                h = route.rsplit('.', 1)[1]
                assert DIGEST_RE.match(h)
                available_packages[name] = h
                break
    for task in tasks:
        image_name = task.pop('name')
        job_symbol = task.pop('symbol')
        args = task.pop('args', {})
        definition = task.pop('definition', image_name)
        packages = task.pop('packages', [])

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

        context_path = os.path.join('taskcluster', 'docker', definition)
        context_hash = generate_context_hash(
            GECKO, context_path, image_name, args)
        digest_data = [context_hash]

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
            'scopes': ['secrets:get:project/taskcluster/gecko/hgfingerprint'],
            'treeherder': {
                'symbol': job_symbol,
                'platform': 'taskcluster-images/opt',
                'kind': 'other',
                'tier': 1,
            },
            'run-on-projects': [],
            'worker-type': 'aws-provisioner-v1/gecko-{}-images'.format(
                config.params['level']),
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
            },
        }

        worker = taskdesc['worker']

        # We use the in-tree image_builder image to build docker images, but
        # that can't be used to build the image_builder image itself,
        # obviously. So we fall back to the last snapshot of the image that
        # was uploaded to docker hub.
        if image_name == 'image_builder':
            worker['docker-image'] = 'taskcluster/image_builder@sha256:' + \
                '24ce54a1602453bc93515aecd9d4ad25a22115fbc4b209ddb5541377e9a37315'
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
            digest_data.append('image_builder')

        worker['caches'] = [{
            'type': 'persistent',
            'name': 'level-{}-{}'.format(config.params['level'], cache_name),
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
                digest_data.append(available_packages[p])

        if len(digest_data) > 1:
            kwargs = {'digest_data': digest_data}
        else:
            kwargs = {'digest': digest_data[0]}
        add_optimization(
            config, taskdesc,
            cache_type="docker-images.v1",
            cache_name=image_name,
            **kwargs
        )

        yield taskdesc
