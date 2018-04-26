# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.partials import (get_balrog_platform_name,
                                     get_partials_artifacts,
                                     get_partials_artifact_map)
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_phase,
                                         get_worker_type_for_scope)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

import logging
import re

logger = logging.getLogger(__name__)


_WINDOWS_BUILD_PLATFORMS = [
    'win64-nightly',
    'win32-nightly',
    'win64-devedition-nightly',
    'win32-devedition-nightly',
]

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US = [
    "target.common.tests.zip",
    "target.cppunittest.tests.zip",
    "target.crashreporter-symbols.zip",
    "target.json",
    "target.mochitest.tests.zip",
    "target.mozinfo.json",
    "target.reftest.tests.zip",
    "target.talos.tests.zip",
    "target.awsy.tests.zip",
    "target.test_packages.json",
    "target.txt",
    "target.web-platform.tests.tar.gz",
    "target.xpcshell.tests.zip",
    "target_info.txt",
    "target.jsshell.zip",
    "mozharness.zip",
]

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_UNSIGNED_PATHS = {
    r'^(linux(|64)|macosx64)-nightly$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
            'host/bin/mar',
            'host/bin/mbsdiff',
        ],
    r'^(linux(|64)|macosx64)-devedition-nightly$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
            'host/bin/mar',
            'host/bin/mbsdiff',
            # TODO Bug 1453033: Sign devedition langpacks
            'target.langpack.xpi',
        ],
    r'^linux64-asan-reporter-nightly$':
        filter(lambda a: a not in ('target.crashreporter-symbols.zip', 'target.jsshell.zip'),
               _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
                    "host/bin/mar",
                    "host/bin/mbsdiff",
                ]),
    r'^win(32|64)-nightly$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
            'host/bin/mar.exe',
            'host/bin/mbsdiff.exe',
        ],
    r'^win(32|64)-devedition-nightly$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
            'host/bin/mar.exe',
            'host/bin/mbsdiff.exe',
            # TODO Bug 1453033: Sign devedition langpacks
            'target.langpack.xpi',
        ],
    r'^(linux(|64)|macosx64|win(32|64))-nightly-l10n$': [],
    r'^(linux(|64)|macosx64|win(32|64))-devedition-nightly-l10n$':
        [
            # TODO Bug 1453033: Sign devedition langpacks
            'target.langpack.xpi',
        ],
}

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_SIGNED_PATHS = {
    r'^linux(|64)(|-devedition|-asan-reporter)-nightly(|-l10n)$':
        ['target.tar.bz2', 'target.tar.bz2.asc'],
    r'^win(32|64)(|-devedition)-nightly(|-l10n)$': ['target.zip'],
}

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_REPACKAGE_PATHS = {
    r'^macosx64(|-devedition)-nightly(|-l10n)$': ['target.dmg'],
}
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_SIGNED_REPACKAGE_PATHS = {
    r'^(linux(|64)|macosx64)(|-devedition|-asan-reporter)-nightly(|-l10n)$':
        ['target.complete.mar'],
    r'^win64(|-devedition)-nightly(|-l10n)$': ['target.complete.mar', 'target.installer.exe'],
    r'^win32(|-devedition)-nightly(|-l10n)$': [
        'target.complete.mar',
        'target.installer.exe',
        'target.stub-installer.exe'
        ],
}

# Compile every regex once at import time
for dict_ in (
    UPSTREAM_ARTIFACT_UNSIGNED_PATHS, UPSTREAM_ARTIFACT_SIGNED_PATHS,
    UPSTREAM_ARTIFACT_REPACKAGE_PATHS, UPSTREAM_ARTIFACT_SIGNED_REPACKAGE_PATHS,
):
    for uncompiled_regex, value in dict_.iteritems():
        compiled_regex = re.compile(uncompiled_regex)
        del dict_[uncompiled_regex]
        dict_[compiled_regex] = value

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

# shortcut for a string where task references are allowed
taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

beetmover_description_schema = Schema({
    # the dependent task (object) for this beetmover job, used to inform beetmover.
    Required('dependent-task'): object,

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
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            beetmover_description_schema, job,
            "In beetmover ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'BM-R')
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

        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}

        signing_name = "build-signing"
        if job.get('locale'):
            signing_name = "nightly-l10n-signing"
        signing_dependencies = {signing_name:
                                dep_job.dependencies[signing_name]
                                }
        dependencies.update(signing_dependencies)

        build_name = "build"
        if job.get('locale'):
            build_name = "unsigned-repack"
        build_dependencies = {"build":
                              dep_job.dependencies[build_name]
                              }
        dependencies.update(build_dependencies)

        repackage_name = "repackage"
        # repackage-l10n actually uses the repackage depname here
        repackage_dependencies = {"repackage":
                                  dep_job.dependencies[repackage_name]
                                  }
        dependencies.update(repackage_dependencies)

        # If this isn't a direct dependency, it won't be in there.
        if 'repackage-signing' not in dependencies:
            repackage_signing_name = "repackage-signing"
            repackage_signing_deps = {"repackage-signing":
                                      dep_job.dependencies[repackage_signing_name]
                                      }
            dependencies.update(repackage_signing_deps)

        attributes = copy_attributes_from_dependent_job(dep_job)
        if job.get('locale'):
            attributes['locale'] = job['locale']

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)
        phase = get_phase(config)

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, bucket_scope),
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'shipping-phase': job.get('shipping-phase', phase),
            'shipping-product': job.get('shipping-product'),
        }

        yield task


def generate_upstream_artifacts(job, build_task_ref, build_signing_task_ref,
                                repackage_task_ref, repackage_signing_task_ref,
                                platform, locale=None):

    build_mapping = UPSTREAM_ARTIFACT_UNSIGNED_PATHS
    build_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_PATHS
    repackage_mapping = UPSTREAM_ARTIFACT_REPACKAGE_PATHS
    repackage_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_REPACKAGE_PATHS

    artifact_prefix = get_artifact_prefix(job)
    if locale:
        artifact_prefix = '{}/{}'.format(artifact_prefix, locale)
        platform = "{}-l10n".format(platform)

    upstream_artifacts = []

    task_refs = [
        build_task_ref,
        build_signing_task_ref,
        repackage_task_ref,
        repackage_signing_task_ref
    ]
    tasktypes = ['build', 'signing', 'repackage', 'repackage']
    mapping = [
        build_mapping,
        build_signing_mapping,
        repackage_mapping,
        repackage_signing_mapping
    ]

    for ref, tasktype, mapping in zip(task_refs, tasktypes, mapping):
        platform_was_previously_matched_by_regex = None
        for platform_regex, paths in mapping.iteritems():
            if platform_regex.match(platform) is not None:
                _check_platform_matched_only_one_regex(
                    tasktype, platform, platform_was_previously_matched_by_regex, platform_regex
                )
                if paths:
                    upstream_artifacts.append({
                        "taskId": {"task-reference": ref},
                        "taskType": tasktype,
                        "paths": ["{}/{}".format(artifact_prefix, path) for path in paths],
                        "locale": locale or "en-US",
                    })
                platform_was_previously_matched_by_regex = platform_regex

    return upstream_artifacts


def generate_partials_upstream_artifacts(job, artifacts, platform, locale=None):
    artifact_prefix = get_artifact_prefix(job)
    if locale and locale != 'en-US':
        artifact_prefix = '{}/{}'.format(artifact_prefix, locale)

    upstream_artifacts = [{
        'taskId': {'task-reference': '<partials-signing>'},
        'taskType': 'signing',
        'paths': ["{}/{}".format(artifact_prefix, p)
                  for p in artifacts],
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


def is_valid_beetmover_job(job):
    # beetmover after partials-signing should have six dependencies.
    # windows builds w/o partials don't have docker-image, so fewer
    # dependencies
    if 'partials-signing' in job['dependencies'].keys():
        expected_dep_count = 5
    else:
        expected_dep_count = 4

    return (len(job["dependencies"]) == expected_dep_count and
            any(['repackage' in j for j in job['dependencies']]))


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        if not is_valid_beetmover_job(job):
            raise NotImplementedError(
                "{}: Beetmover_repackage must have five dependencies.".format(job['label'])
            )

        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]
        build_task = None
        build_signing_task = None
        repackage_task = None
        repackage_signing_task = None

        for dependency in job["dependencies"].keys():
            if 'repackage-signing' in dependency:
                repackage_signing_task = dependency
            elif 'repackage' in dependency:
                repackage_task = dependency
            elif 'signing' in dependency:
                # catches build-signing and nightly-l10n-signing
                build_signing_task = dependency
            else:
                build_task = "build"

        build_task_ref = "<" + str(build_task) + ">"
        build_signing_task_ref = "<" + str(build_signing_task) + ">"
        repackage_task_ref = "<" + str(repackage_task) + ">"
        repackage_signing_task_ref = "<" + str(repackage_signing_task) + ">"

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(
                job, build_task_ref, build_signing_task_ref, repackage_task_ref,
                repackage_signing_task_ref, platform, locale
            ),
        }
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

        # Remove when proved reliable
        # job['treeherder']['tier'] = 3

        platform = job["attributes"]["build_platform"]

        balrog_platform = get_balrog_platform_name(platform)

        artifacts = get_partials_artifacts(config.params.get('release_history'),
                                           balrog_platform, locale)

        # Dependency:        | repackage-signing | partials-signing
        # Partials artifacts |              Skip | Populate & yield
        # No partials        |             Yield |         continue
        if len(artifacts) == 0:
            if 'partials-signing' in job['dependencies']:
                continue
            else:
                yield job
                continue
        else:
            if 'partials-signing' not in job['dependencies']:
                continue

        upstream_artifacts = generate_partials_upstream_artifacts(
            job, artifacts, balrog_platform, locale
        )

        job['worker']['upstream-artifacts'].extend(upstream_artifacts)

        extra = list()

        artifact_map = get_partials_artifact_map(
            config.params.get('release_history'), balrog_platform, locale)
        for artifact in artifact_map:
            artifact_extra = {
                'locale': locale,
                'artifact_name': artifact,
                'buildid': artifact_map[artifact]['buildid'],
                'platform': balrog_platform,
            }
            for rel_attr in ('previousBuildNumber', 'previousVersion'):
                if artifact_map[artifact].get(rel_attr):
                    artifact_extra[rel_attr] = artifact_map[artifact][rel_attr]
            extra.append(artifact_extra)

        job.setdefault('extra', {})
        job['extra']['partials'] = extra

        yield job
