# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from voluptuous import Optional, Required

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (generate_beetmover_artifact_map,
                                         generate_beetmover_upstream_artifacts,
                                         get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_worker_type_for_scope,
                                         should_use_artifact_map)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.treeherder import replace_group

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US = [
    "en-US/buildhub.json",
    "en-US/target.common.tests.tar.gz",
    "en-US/target.cppunittest.tests.tar.gz",
    "en-US/target.crashreporter-symbols.zip",
    "en-US/target.json",
    "en-US/target.mochitest.tests.tar.gz",
    "en-US/target.mozinfo.json",
    "en-US/target.reftest.tests.tar.gz",
    "en-US/target.talos.tests.tar.gz",
    "en-US/target.awsy.tests.tar.gz",
    "en-US/target.test_packages.json",
    "en-US/target.txt",
    "en-US/target.web-platform.tests.tar.gz",
    "en-US/target.xpcshell.tests.tar.gz",
    "en-US/target_info.txt",
    "en-US/mozharness.zip",
    "en-US/robocop.apk",
    "en-US/target.jsshell.zip",
]
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI = [
    "buildhub.json",
    "target.json",
    "target.mozinfo.json",
    "target.test_packages.json",
    "target.txt",
    "target_info.txt",
    "robocop.apk",
]
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_MOBILE_UPSTREAM_ARTIFACTS_SIGNED_EN_US = [
    "en-US/target.apk",
]
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_MOBILE_UPSTREAM_ARTIFACTS_SIGNED_MULTI = [
    "target.apk",
]


# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_UNSIGNED_PATHS = {
    'android-x86-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-x86_64-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-aarch64-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-api-16-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-x86-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-x86_64-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-aarch64-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-api-16-nightly-l10n': [],
    'android-api-16-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
}
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_SIGNED_PATHS = {
    'android-x86-nightly': ["en-US/target.apk"],
    'android-x86_64-nightly': ["en-US/target.apk"],
    'android-aarch64-nightly': ["en-US/target.apk"],
    'android-api-16-nightly': ["en-US/target.apk"],
    'android-x86-nightly-multi': ["target.apk"],
    'android-x86_64-nightly-multi': ["target.apk"],
    'android-aarch64-nightly-multi': ["target.apk"],
    'android-api-16-nightly-l10n': ["target.apk"],
    'android-api-16-nightly-multi': ["target.apk"],
}
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_SOURCE_ARTIFACTS = [
    "source.tar.xz",
    "source.tar.xz.asc",
]

transforms = TransformSequence()

beetmover_description_schema = schema.extend({
    # depname is used in taskref's to identify the taskID of the unsigned things
    Required('depname', default='build'): basestring,

    # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
    Optional('label'): basestring,

    # treeherder is allowed here to override any defaults we use for beetmover.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): task_description_schema['treeherder'],

    # locale is passed only for l10n beetmoving
    Optional('locale'): basestring,

    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('attributes'): task_description_schema['attributes'],
})


transforms.add_validate(beetmover_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault(
            'symbol',
            replace_group(dep_job.task['extra']['treeherder']['symbol'], 'BM')
        )
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault(
            'tier',
            dep_job.task.get('extra', {}).get('treeherder', {}).get('tier', 1)
        )
        treeherder.setdefault('kind', 'build')
        label = job['label']
        description = (
            "Beetmover submission for locale '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=attributes.get('locale', 'en-US'),
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        dependencies = {dep_job.kind: dep_job.label}

        # XXX release snap-repackage has a variable number of dependencies, depending on how many
        # "post-beetmover-dummy" jobs there are in the graph.
        if dep_job.kind != 'release-snap-repackage' and len(dep_job.dependencies) > 1:
            raise NotImplementedError(
                "Can't beetmove a signing task with multiple dependencies")
        signing_dependencies = dep_job.dependencies
        dependencies.update(signing_dependencies)

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get('attributes', {}))

        if job.get('locale'):
            attributes['locale'] = job['locale']

        bucket_scope = get_beetmover_bucket_scope(
            config, job_release_type=attributes.get('release-type')
        )
        action_scope = get_beetmover_action_scope(
            config, job_release_type=attributes.get('release-type')
        )

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, bucket_scope),
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'shipping-phase': job['shipping-phase'],
            'shipping-product': job.get('shipping-product'),
        }

        yield task


def generate_upstream_artifacts(job, signing_task_ref, build_task_ref, platform,
                                locale=None):
    build_mapping = UPSTREAM_ARTIFACT_UNSIGNED_PATHS
    signing_mapping = UPSTREAM_ARTIFACT_SIGNED_PATHS

    artifact_prefix = get_artifact_prefix(job)
    if locale:
        artifact_prefix = '{}/{}'.format(artifact_prefix, locale)
        platform = "{}-l10n".format(platform)

    if platform.endswith("-source"):
        return [
            {
                "taskId": {"task-reference": signing_task_ref},
                "taskType": "signing",
                "paths": ["{}/{}".format(artifact_prefix, p)
                          for p in UPSTREAM_SOURCE_ARTIFACTS],
                "locale": locale or "en-US",
            }
        ]

    upstream_artifacts = []

    # Some platforms (like android-api-16-nightly-l10n) may not depend on any unsigned artifact
    if build_mapping[platform]:
        upstream_artifacts.append({
            "taskId": {"task-reference": build_task_ref},
            "taskType": "build",
            "paths": ["{}/{}".format(artifact_prefix, p)
                      for p in build_mapping[platform]],
            "locale": locale or "en-US",
        })

    upstream_artifacts.append({
        "taskId": {"task-reference": signing_task_ref},
        "taskType": "signing",
        "paths": ["{}/{}".format(artifact_prefix, p)
                  for p in signing_mapping[platform]],
        "locale": locale or "en-US",
    })

    if not locale and "android" in platform:
        # edge case to support 'multi' locale paths
        multi_platform = "{}-multi".format(platform)
        upstream_artifacts.extend([{
            "taskId": {"task-reference": build_task_ref},
            "taskType": "build",
            "paths": ["{}/{}".format(artifact_prefix, p)
                      for p in build_mapping[multi_platform]],
            "locale": "multi",
        }, {
            "taskId": {"task-reference": signing_task_ref},
            "taskType": "signing",
            "paths": ["{}/{}".format(artifact_prefix, p)
                      for p in signing_mapping[multi_platform]],
            "locale": "multi",
        }])

    return upstream_artifacts


def craft_release_properties(config, job):
    params = config.params
    build_platform = job['attributes']['build_platform']
    build_platform = build_platform.replace('-nightly', '')
    build_platform = build_platform.replace('-shippable', '')
    if build_platform.endswith("-source"):
        build_platform = build_platform.replace('-source', '-release')

    # XXX This should be explicitly set via build attributes or something
    if 'android' in job['label'] or 'fennec' in job['label']:
        app_name = 'Fennec'
    elif config.graph_config['trust-domain'] == 'comm':
        app_name = 'Thunderbird'
    else:
        # XXX Even DevEdition is called Firefox
        app_name = 'Firefox'

    return {
        'app-name': app_name,
        'app-version': str(params['app_version']),
        'branch': params['project'],
        'build-id': str(params['moz_build_date']),
        'hash-type': 'sha512',
        'platform': build_platform,
    }


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = (len(job["dependencies"]) == 2 and
                               any(['signing' in j for j in job['dependencies']]))
        # XXX release snap-repackage has a variable number of dependencies, depending on how many
        # "post-beetmover-dummy" jobs there are in the graph.
        if '-snap-' not in job['label'] and not valid_beetmover_job:
            raise NotImplementedError("Beetmover must have two dependencies.")

        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]
        build_task = None
        signing_task = None
        for dependency in job["dependencies"].keys():
            if 'signing' in dependency:
                signing_task = dependency
            else:
                build_task = dependency

        signing_task_ref = "<" + str(signing_task) + ">"
        build_task_ref = "<" + str(build_task) + ">"

        if should_use_artifact_map(platform):
            upstream_artifacts = generate_beetmover_upstream_artifacts(
                config, job, platform, locale
            )
        else:
            upstream_artifacts = generate_upstream_artifacts(
                job, signing_task_ref, build_task_ref, platform, locale
            )
        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': upstream_artifacts,
        }

        if should_use_artifact_map(platform):
            worker['artifact-map'] = generate_beetmover_artifact_map(
                config, job, platform=platform, locale=locale)

        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
