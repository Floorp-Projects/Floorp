# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.multi_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.partials import (get_balrog_platform_name,
                                     get_partials_artifacts_from_params,
                                     get_partials_info_from_params)
from taskgraph.util.scriptworker import (generate_beetmover_artifact_map,
                                         generate_beetmover_upstream_artifacts,
                                         generate_beetmover_partials_artifact_map,
                                         get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_worker_type_for_scope,
                                         should_use_artifact_map)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.treeherder import replace_group
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

import logging
import re

logger = logging.getLogger(__name__)


def _compile_regex_mapping(mapping):
    return {re.compile(regex): value for regex, value in mapping.iteritems()}


# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US = [
    "buildhub.json",
    "target.common.tests.tar.gz",
    "target.cppunittest.tests.tar.gz",
    "target.crashreporter-symbols.zip",
    "target.json",
    "target.mochitest.tests.tar.gz",
    "target.mozinfo.json",
    "target.reftest.tests.tar.gz",
    "target.talos.tests.tar.gz",
    "target.awsy.tests.tar.gz",
    "target.test_packages.json",
    "target.txt",
    "target.web-platform.tests.tar.gz",
    "target.xpcshell.tests.tar.gz",
    "target_info.txt",
    "target.jsshell.zip",
    "mozharness.zip",
    # This is only uploaded unsigned for Nightly, on beta/release/esr we
    # beetmove the signed copy from amo.
    "target.langpack.xpi",
]

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N = [
    # This is only uploaded unsigned for Nightly, on beta/release/esr we
    # beetmove the signed copy from amo.
    "target.langpack.xpi",
]


# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_UNSIGNED_PATHS = _compile_regex_mapping({
    r'^(linux(|64)|macosx64)(|-devedition)-(nightly|shippable)$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
            'host/bin/mar',
            'host/bin/mbsdiff',
        ],
    r'^linux64-asan-reporter-nightly$':
        filter(lambda a: a not in ('target.crashreporter-symbols.zip', 'target.jsshell.zip'),
               _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
                    "host/bin/mar",
                    "host/bin/mbsdiff",
                ]),
    r'^win64-asan-reporter-nightly$':
        filter(lambda a: a not in ('target.crashreporter-symbols.zip', 'target.jsshell.zip'),
               _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
                    "host/bin/mar.exe",
                    "host/bin/mbsdiff.exe",
                ]),
    r'^win(32|64(|-aarch64))(|-devedition)-(nightly|shippable)$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
            'host/bin/mar.exe',
            'host/bin/mbsdiff.exe',
        ],
    r'^(linux(|64)|macosx64|win(32|64))(|-devedition)-(nightly|shippable)-l10n$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
})

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_SIGNED_PATHS = _compile_regex_mapping({
    r'^linux(|64)(|-devedition|-asan-reporter)-(nightly|shippable)(|-l10n)$':
        ['target.tar.bz2', 'target.tar.bz2.asc'],
    r'^win(32|64)(|-aarch64)(|-devedition|-asan-reporter)-(nightly|shippable)(|-l10n)$':
        ['target.zip'],
})

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_REPACKAGE_PATHS = [
    'target.dmg',
]
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_SIGNED_REPACKAGE_PATHS = [
    'target.installer.exe',
    'target.stub-installer.exe',
]

UPSTREAM_ARTIFACT_SIGNED_MSI_PATHS = [
    'target.installer.msi',
]

UPSTREAM_ARTIFACT_SIGNED_MAR_PATHS = [
    'target.complete.mar',
    'target.bz2.complete.mar',
]

beetmover_description_schema = schema.extend({
    # depname is used in taskref's to identify the taskID of the unsigned things
    Required('depname', default='build'): basestring,

    # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
    Required('label'): basestring,

    # treeherder is allowed here to override any defaults we use for beetmover.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): task_description_schema['treeherder'],

    Optional('attributes'): task_description_schema['attributes'],

    # locale is passed only for l10n beetmoving
    Optional('locale'): basestring,
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    # Optional until we fix asan (run_on_projects?)
    Optional('shipping-product'): task_description_schema['shipping-product'],
})

transforms = TransformSequence()
transforms.add_validate(beetmover_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        upstream_symbol = dep_job.task['extra']['treeherder']['symbol']
        if 'build' in job['dependent-tasks']:
            upstream_symbol = job['dependent-tasks']['build'].task['extra']['treeherder']['symbol']
        treeherder.setdefault(
            'symbol',
            replace_group(upstream_symbol, 'BMR')
        )
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
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

        upstream_deps = job['dependent-tasks']

        signing_name = "build-signing"
        build_name = "build"
        repackage_name = "repackage"
        repackage_signing_name = "repackage-signing"
        msi_signing_name = "repackage-signing-msi"
        mar_signing_name = "mar-signing"
        if job.get('locale'):
            signing_name = "nightly-l10n-signing"
            build_name = "nightly-l10n"
            repackage_name = "repackage-l10n"
            repackage_signing_name = "repackage-signing-l10n"
            mar_signing_name = "mar-signing-l10n"
        dependencies = {
            "build": upstream_deps[build_name],
            "repackage": upstream_deps[repackage_name],
            "signing": upstream_deps[signing_name],
            "mar-signing": upstream_deps[mar_signing_name],
        }
        if 'partials-signing' in upstream_deps:
            dependencies['partials-signing'] = upstream_deps['partials-signing']
        if msi_signing_name in upstream_deps:
            dependencies[msi_signing_name] = upstream_deps[msi_signing_name]
        if repackage_signing_name in upstream_deps:
            dependencies["repackage-signing"] = upstream_deps[repackage_signing_name]

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get('attributes', {}))
        if job.get('locale'):
            attributes['locale'] = job['locale']

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

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


def generate_upstream_artifacts(
    config, job, dependencies, platform, locale=None, project=None
):

    build_mapping = UPSTREAM_ARTIFACT_UNSIGNED_PATHS
    build_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_PATHS
    repackage_mapping = UPSTREAM_ARTIFACT_REPACKAGE_PATHS
    repackage_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_REPACKAGE_PATHS
    msi_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_MSI_PATHS
    mar_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_MAR_PATHS

    artifact_prefix = get_artifact_prefix(job)
    if locale:
        artifact_prefix = '{}/{}'.format(artifact_prefix, locale)
        platform = "{}-l10n".format(platform)

    upstream_artifacts = []

    for task_type, mapping in [
        ("build", build_mapping),
        ("signing", build_signing_mapping),
    ]:
        platform_was_previously_matched_by_regex = None
        for platform_regex, paths in mapping.iteritems():
            if platform_regex.match(platform) is not None:
                _check_platform_matched_only_one_regex(
                    task_type, platform, platform_was_previously_matched_by_regex, platform_regex
                )
                platform_was_previously_matched_by_regex = platform_regex
                if paths:
                    usable_paths = paths[:]

                    if 'target.langpack.xpi' in usable_paths and \
                            not project == "mozilla-central":
                        # XXX This is only beetmoved for m-c nightlies.
                        # we should determine this better
                        usable_paths.remove('target.langpack.xpi')

                        if not len(usable_paths):
                            # We may have removed our only path.
                            continue

                    upstream_artifacts.append({
                        "taskId": {"task-reference": "<{}>".format(task_type)},
                        "taskType": task_type,
                        "paths": ["{}/{}".format(artifact_prefix, path) for path in usable_paths],
                        "locale": locale or "en-US",
                    })

    for task_type, cot_type, paths in [
        ('repackage', 'repackage', repackage_mapping),
        ('repackage-signing', 'repackage', repackage_signing_mapping),
        ('repackage-signing-msi', 'repackage', msi_signing_mapping),
        ('mar-signing', 'signing', mar_signing_mapping),
    ]:
        if task_type not in dependencies:
            continue

        paths = ["{}/{}".format(artifact_prefix, path) for path in paths]
        paths = [
            path for path in paths
            if path in dependencies[task_type].release_artifacts]

        if not paths:
            continue

        upstream_artifacts.append({
            "taskId": {"task-reference": "<{}>".format(task_type)},
            "taskType": cot_type,
            "paths": paths,
            "locale": locale or "en-US",
        })

    return upstream_artifacts


def generate_partials_upstream_artifacts(job, artifacts, platform, locale=None):
    artifact_prefix = get_artifact_prefix(job)
    if locale and locale != 'en-US':
        artifact_prefix = '{}/{}'.format(artifact_prefix, locale)

    upstream_artifacts = [{
        'taskId': {'task-reference': '<partials-signing>'},
        'taskType': 'signing',
        'paths': ["{}/{}".format(artifact_prefix, path)
                  for path, _ in artifacts],
        'locale': locale or 'en-US',
    }]

    return upstream_artifacts


def _check_platform_matched_only_one_regex(
    task_type, platform, platform_was_previously_matched_by_regex, platform_regex
):
    if platform_was_previously_matched_by_regex is not None:
        raise Exception('In task type "{task_type}", platform "{platform}" matches at \
least 2 regular expressions. First matched: "{first_matched}". Second matched: \
"{second_matched}"'.format(
            task_type=task_type, platform=platform,
            first_matched=platform_was_previously_matched_by_regex.pattern,
            second_matched=platform_regex.pattern
        ))


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]

        if should_use_artifact_map(platform):
            upstream_artifacts = generate_beetmover_upstream_artifacts(
                config, job, platform, locale)
        else:
            upstream_artifacts = generate_upstream_artifacts(
                config, job, job['dependencies'], platform, locale,
                project=config.params['project']
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


@transforms.add
def make_partials_artifacts(config, jobs):
    for job in jobs:
        locale = job["attributes"].get("locale")
        if not locale:
            locale = 'en-US'

        platform = job["attributes"]["build_platform"]

        if 'partials-signing' not in job['dependencies']:
            yield job
            continue

        balrog_platform = get_balrog_platform_name(platform)
        artifacts = get_partials_artifacts_from_params(config.params.get('release_history'),
                                                       balrog_platform, locale)

        upstream_artifacts = generate_partials_upstream_artifacts(
            job, artifacts, balrog_platform, locale
        )

        job['worker']['upstream-artifacts'].extend(upstream_artifacts)

        extra = list()

        partials_info = get_partials_info_from_params(
            config.params.get('release_history'), balrog_platform, locale)

        if should_use_artifact_map(platform):
            job['worker']['artifact-map'].extend(
                generate_beetmover_partials_artifact_map(
                    config, job, partials_info, platform=platform, locale=locale))

        for artifact in partials_info:
            artifact_extra = {
                'locale': locale,
                'artifact_name': artifact,
                'buildid': partials_info[artifact]['buildid'],
                'platform': balrog_platform,
            }
            for rel_attr in ('previousBuildNumber', 'previousVersion'):
                if partials_info[artifact].get(rel_attr):
                    artifact_extra[rel_attr] = partials_info[artifact][rel_attr]
            extra.append(artifact_extra)

        job.setdefault('extra', {})
        job['extra']['partials'] = extra

        yield job


@transforms.add
def convert_deps(config, jobs):
    for job in jobs:
        job['dependencies'] = {
            name: dep_job.label
            for name, dep_job in job['dependencies'].items()
        }
        yield job
