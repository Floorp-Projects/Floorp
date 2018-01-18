# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_phase)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional


# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US = [
    "balrog_props.json",
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
    "target.langpack.xpi",
]

# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US = [
    "update/target.complete.mar",
]
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N = [
    "target.langpack.xpi",
    "balrog_props.json",
]
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N = [
    "target.complete.mar",
]
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
_MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US = [
    "en-US/target.common.tests.zip",
    "en-US/target.cppunittest.tests.zip",
    "en-US/target.crashreporter-symbols.zip",
    "en-US/target.json",
    "en-US/target.mochitest.tests.zip",
    "en-US/target.mozinfo.json",
    "en-US/target.reftest.tests.zip",
    "en-US/target.talos.tests.zip",
    "en-US/target.awsy.tests.zip",
    "en-US/target.test_packages.json",
    "en-US/target.txt",
    "en-US/target.web-platform.tests.tar.gz",
    "en-US/target.xpcshell.tests.zip",
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
    "balrog_props.json",
    "target.common.tests.zip",
    "target.cppunittest.tests.zip",
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
    "mozharness.zip",
    "robocop.apk",
    "target.jsshell.zip",
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
    'linux64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar",
        "host/bin/mbsdiff",
    ],
    'linux-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar",
        "host/bin/mbsdiff",
    ],
    'linux64-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar",
        "host/bin/mbsdiff",
    ],
    'linux-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar",
        "host/bin/mbsdiff",
    ],
    'linux64-source': [
    ],
    'linux64-devedition-source': [
    ],
    'linux64-fennec-source': [
    ],
    'android-x86-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-aarch64-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-api-16-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-x86-old-id-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-api-16-old-id-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'macosx64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar",
        "host/bin/mbsdiff",
    ],
    'macosx64-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar",
        "host/bin/mbsdiff",
    ],
    'win32-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar.exe",
        "host/bin/mbsdiff.exe",
    ],
    'win32-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar.exe",
        "host/bin/mbsdiff.exe",
    ],
    'win64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar.exe",
        "host/bin/mbsdiff.exe",
    ],
    'win64-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "host/bin/mar.exe",
        "host/bin/mbsdiff.exe",
    ],
    'linux64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'linux64-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'linux-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'linux-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'android-x86-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-x86-old-id-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-aarch64-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-api-16-nightly-l10n': ["balrog_props.json"],
    'android-api-16-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-api-16-old-id-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'macosx64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'macosx64-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'win32-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'win32-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'win64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'win64-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
}
# Until bug 1331141 is fixed, if you are adding any new artifacts here that
# need to be transfered to S3, please be aware you also need to follow-up
# with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
# See example in bug 1348286
UPSTREAM_ARTIFACT_SIGNED_PATHS = {
    'linux64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux64-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux64-source': [
        "SOURCE",
        "SOURCE.asc",
    ],
    'linux64-devedition-source': [
        "SOURCE",
        "SOURCE.asc",
    ],
    'linux64-fennec-source': [
        "SOURCE",
        "SOURCE.asc",
    ],
    'android-x86-nightly': ["en-US/target.apk"],
    'android-aarch64-nightly': ["en-US/target.apk"],
    'android-api-16-nightly': ["en-US/target.apk"],
    'android-x86-old-id-nightly': ["en-US/target.apk"],
    'android-api-16-old-id-nightly': ["en-US/target.apk"],
    'macosx64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.dmg",
        "target.dmg.asc",
    ],
    'macosx64-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.dmg",
        "target.dmg.asc",
    ],
    'win32-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.zip",
    ],
    'win32-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.zip",
    ],
    'win64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.zip",
    ],
    'win64-devedition-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.zip",
    ],
    'linux64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux64-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'android-x86-nightly-multi': ["target.apk"],
    'android-x86-old-id-nightly-multi': ["target.apk"],
    'android-aarch64-nightly-multi': ["target.apk"],
    'android-api-16-nightly-l10n': ["target.apk"],
    'android-api-16-nightly-multi': ["target.apk"],
    'android-api-16-old-id-nightly-multi': ["target.apk"],
    'macosx64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.dmg",
        "target.dmg.asc",
    ],
    'macosx64-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.dmg",
        "target.dmg.asc",
    ],
    'win32-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.zip",
    ],
    'win32-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.zip",
    ],
    'win64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.zip",
    ],
    'win64-devedition-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.zip",
    ],

}

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
        treeherder.setdefault('symbol', 'BM-S')
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

        if len(dep_job.dependencies) > 1:
            raise NotImplementedError(
                "Can't beetmove a signing task with multiple dependencies")
        signing_dependencies = dep_job.dependencies
        dependencies.update(signing_dependencies)

        attributes = copy_attributes_from_dependent_job(dep_job)

        if job.get('locale'):
            attributes['locale'] = job['locale']

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)
        phase = get_phase(config)

        task = {
            'label': label,
            'description': description,
            'worker-type': 'scriptworker-prov-v1/beetmoverworker-v1',
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'shipping-phase': phase,
        }

        yield task


def generate_upstream_artifacts(signing_task_ref, build_task_ref, platform,
                                locale=None):
    build_mapping = UPSTREAM_ARTIFACT_UNSIGNED_PATHS
    signing_mapping = UPSTREAM_ARTIFACT_SIGNED_PATHS

    artifact_prefix = 'public/build'
    if locale:
        artifact_prefix = 'public/build/{}'.format(locale)
        platform = "{}-l10n".format(platform)

    upstream_artifacts = [{
        "taskId": {"task-reference": build_task_ref},
        "taskType": "build",
        "paths": ["{}/{}".format(artifact_prefix, p)
                  for p in build_mapping[platform]],
        "locale": locale or "en-US",
        }, {
        "taskId": {"task-reference": signing_task_ref},
        "taskType": "signing",
        "paths": ["{}/{}".format(artifact_prefix, p)
                  for p in signing_mapping[platform]],
        "locale": locale or "en-US",
    }]

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


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = (len(job["dependencies"]) == 2 and
                               any(['signing' in j for j in job['dependencies']]))
        if not valid_beetmover_job:
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
        upstream_artifacts = generate_upstream_artifacts(
            signing_task_ref, build_task_ref, platform, locale
        )

        worker = {'implementation': 'beetmover',
                  'upstream-artifacts': upstream_artifacts}
        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
