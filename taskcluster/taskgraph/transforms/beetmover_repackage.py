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
UPSTREAM_ARTIFACT_UNSIGNED_PATHS = {
    r'^(linux(|64)|macosx64)(|-devedition)-nightly$':
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
    r'^win(32|64)(|-devedition)-nightly$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
            'host/bin/mar.exe',
            'host/bin/mbsdiff.exe',
        ],
    r'^(linux(|64)|macosx64|win(32|64))(|-devedition)-nightly-l10n$':
        _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
}

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_SIGNED_PATHS = {
    r'^linux(|64)(|-devedition|-asan-reporter)-nightly(|-l10n)$':
        ['target.tar.bz2', 'target.tar.bz2.asc'],
    r'^win(32|64)(|-devedition|-asan-reporter)-nightly(|-l10n)$': ['target.zip'],
}

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
    'target.complete.mar',
    'target.bz2.complete.mar',
    'target.installer.exe',
    'target.stub-installer.exe',
]

# Compile every regex once at import time
for dict_ in (
    UPSTREAM_ARTIFACT_UNSIGNED_PATHS, UPSTREAM_ARTIFACT_SIGNED_PATHS,
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

    Required('grandparent-tasks'): object,

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

        dependent_kind = dep_job.kind
        if dependent_kind == 'repackage-signing-l10n':
            dependent_kind = "repackage-signing"
        dependencies = {dependent_kind: dep_job}

        signing_name = "build-signing"
        if job.get('locale'):
            signing_name = "nightly-l10n-signing"
        dependencies['signing'] = job['grandparent-tasks'][signing_name]

        build_name = "build"
        if job.get('locale'):
            build_name = "unsigned-repack"
        dependencies["build"] = job['grandparent-tasks'][build_name]

        # repackage-l10n actually uses the repackage depname here
        dependencies["repackage"] = job['grandparent-tasks']["repackage"]

        # If this isn't a direct dependency, it won't be in there.
        if 'repackage-signing' not in dependencies:
            repackage_signing_name = "repackage-signing"
            if job.get('locale'):
                repackage_signing_name = "repackage-signing-l10n"
            dependencies["repackage-signing"] = job['grandparent-tasks'][repackage_signing_name]

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


def generate_upstream_artifacts(job, dependencies, platform, locale=None, project=None):

    build_mapping = UPSTREAM_ARTIFACT_UNSIGNED_PATHS
    build_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_PATHS
    repackage_mapping = UPSTREAM_ARTIFACT_REPACKAGE_PATHS
    repackage_signing_mapping = UPSTREAM_ARTIFACT_SIGNED_REPACKAGE_PATHS

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
    ]:
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

        upstream_artifacts = generate_upstream_artifacts(
            job, job['dependencies'], platform, locale,
            project=config.params['project']
        )

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': upstream_artifacts,
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


@transforms.add
def convert_deps(config, jobs):
    for job in jobs:
        job['dependencies'] = {
            name: dep_job.label
            for name, dep_job in job['dependencies'].items()
        }
        yield job
