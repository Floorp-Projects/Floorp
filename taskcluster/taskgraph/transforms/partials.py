# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the partials task into an actual task description.
"""
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.partials import get_balrog_platform_name, get_builds
from taskgraph.util.platforms import architecture
from taskgraph.util.taskcluster import get_artifact_prefix

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


def _generate_task_output_files(job, filenames, locale=None):
    locale_output_path = '{}/'.format(locale) if locale else ''
    artifact_prefix = get_artifact_prefix(job)

    data = list()
    for filename in filenames:
        data.append({
            'type': 'file',
            'path': '/home/worker/artifacts/{}'.format(filename),
            'name': '{}/{}{}'.format(artifact_prefix, locale_output_path, filename)
        })
    data.append({
        'type': 'file',
        'path': '/home/worker/artifacts/manifest.json',
        'name': '{}/{}manifest.json'.format(artifact_prefix, locale_output_path)
    })
    return data


@transforms.add
def make_task_description(config, jobs):
    # If no balrog release history, then don't generate partials
    if not config.params.get('release_history'):
        return
    for job in jobs:
        dep_job = job['primary-dependency']

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'p(N)')

        label = job.get('label', "partials-{}".format(dep_job.label))
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')

        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('kind', 'build')
        treeherder.setdefault('tier', 1)

        dependencies = {dep_job.kind: dep_job.label}

        attributes = copy_attributes_from_dependent_job(dep_job)
        locale = dep_job.attributes.get('locale')
        if locale:
            attributes['locale'] = locale
            treeherder['symbol'] = "p({})".format(locale)
        attributes['shipping_phase'] = job['shipping-phase']

        build_locale = locale or 'en-US'

        builds = get_builds(config.params['release_history'], dep_th_platform,
                            build_locale)

        # If the list is empty there's no available history for this platform
        # and locale combination, so we can't build any partials.
        if not builds:
            continue

        extra = {'funsize': {'partials': list()}}
        update_number = 1

        locale_suffix = ''
        if locale:
            locale_suffix = '{}/'.format(locale)
        artifact_path = "<{}/{}/{}target.complete.mar>".format(
            dep_job.kind, get_artifact_prefix(dep_job), locale_suffix,
        )
        for build in sorted(builds):
            partial_info = {
                'locale': build_locale,
                'from_mar': builds[build]['mar_url'],
                'to_mar': {'artifact-reference': artifact_path},
                'platform': get_balrog_platform_name(dep_th_platform),
                'branch': config.params['project'],
                'update_number': update_number,
                'dest_mar': build,
            }
            if 'product' in builds[build]:
                partial_info['product'] = builds[build]['product']
            if 'previousVersion' in builds[build]:
                partial_info['previousVersion'] = builds[build]['previousVersion']
            if 'previousBuildNumber' in builds[build]:
                partial_info['previousBuildNumber'] = builds[build]['previousBuildNumber']
            extra['funsize']['partials'].append(partial_info)
            update_number += 1

        mar_channel_id = None
        if config.params['project'] == 'mozilla-beta':
            if 'devedition' in label:
                mar_channel_id = 'firefox-mozilla-aurora'
            else:
                mar_channel_id = 'firefox-mozilla-beta'
        elif config.params['project'] == 'mozilla-release':
            mar_channel_id = 'firefox-mozilla-release'
        elif 'esr' in config.params['project']:
            mar_channel_id = 'firefox-mozilla-esr'

        level = config.params['level']

        worker = {
            'artifacts': _generate_task_output_files(dep_job, builds.keys(), locale),
            'implementation': 'docker-worker',
            'docker-image': {'in-tree': 'funsize-update-generator'},
            'os': 'linux',
            'max-run-time': 3600 if 'asan' in dep_job.label else 600,
            'chain-of-trust': True,
            'taskcluster-proxy': True,
            'env': {
                'SHA1_SIGNING_CERT': 'nightly_sha1',
                'SHA384_SIGNING_CERT': 'nightly_sha384',
                'DATADOG_API_SECRET':
                    'project/releng/gecko/build/level-{}/datadog-api-key'.format(level),
                'EXTRA_PARAMS': '--arch={}'.format(architecture(attributes['build_platform'])),
            }
        }
        if mar_channel_id:
            worker['env']['ACCEPTED_MAR_CHANNEL_IDS'] = mar_channel_id
        if config.params.release_level() == 'staging':
            worker['env']['FUNSIZE_ALLOW_STAGING_PREFIXES'] = 'true'

        task = {
            'label': label,
            'description': "{} Partials".format(
                dep_job.task["metadata"]["description"]),
            'worker-type': 'b-linux',
            'dependencies': dependencies,
            'scopes': [
                'secrets:get:project/releng/gecko/build/level-%s/datadog-api-key' % level
            ],
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'extra': extra,
            'worker': worker,
        }

        # We only want caching on linux/windows due to bug 1436977
        if int(level) == 3 \
                and any([platform in dep_th_platform for platform in ['linux', 'windows']]):
            task['scopes'].append(
                'auth:aws-s3:read-write:tc-gp-private-1d-us-east-1/releng/mbsdiff-cache/')

        yield task
