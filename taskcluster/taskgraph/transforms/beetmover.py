# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import (
    validate_schema,
    TransformSequence
)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Schema, Any, Required, Optional


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
    "target.test_packages.json",
    "target.txt",
    "target.web-platform.tests.zip",
    "target.xpcshell.tests.zip",
    "target_info.txt",
    "target.jsshell.zip",
    "mozharness.zip",
    "target.langpack.xpi",
]
_DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US = [
    "update/target.complete.mar",
]
_DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N = [
    "target.langpack.xpi",
    "balrog_props.json",
]
_DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N = [
    "target.complete.mar",
]
_MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US = [
    "en-US/target.common.tests.zip",
    "en-US/target.cppunittest.tests.zip",
    "en-US/target.json",
    "en-US/target.mochitest.tests.zip",
    "en-US/target.mozinfo.json",
    "en-US/target.reftest.tests.zip",
    "en-US/target.talos.tests.zip",
    "en-US/target.test_packages.json",
    "en-US/target.txt",
    "en-US/target.web-platform.tests.zip",
    "en-US/target.xpcshell.tests.zip",
    "en-US/target_info.txt",
    "en-US/bouncer.apk",
    "en-US/mozharness.zip",
    "en-US/robocop.apk",
    "en-US/target.jsshell.zip",
]
_MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI = [
    "balrog_props.json",
    "target.common.tests.zip",
    "target.cppunittest.tests.zip",
    "target.json",
    "target.mochitest.tests.zip",
    "target.mozinfo.json",
    "target.reftest.tests.zip",
    "target.talos.tests.zip",
    "target.test_packages.json",
    "target.txt",
    "target.web-platform.tests.zip",
    "target.xpcshell.tests.zip",
    "target_info.txt",
    "bouncer.apk",
    "mozharness.zip",
    "robocop.apk",
    "target.jsshell.zip",
]
_MOBILE_UPSTREAM_ARTIFACTS_SIGNED_EN_US = [
    "en-US/target.apk",
]
_MOBILE_UPSTREAM_ARTIFACTS_SIGNED_MULTI = [
    "target.apk",
]


UPSTREAM_ARTIFACT_UNSIGNED_PATHS = {
    'linux64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "target.sdk.tar.bz2"
    ],
    'linux-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US + [
        "target.sdk.tar.bz2"
    ],
    'android-x86-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'android-api-15-nightly': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'macosx64-nightly': [],

    'linux64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'linux-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
    'android-x86-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'android-api-15-nightly-l10n': ["balrog_props.json"],
    'android-api-15-nightly-multi': _MOBILE_UPSTREAM_ARTIFACTS_UNSIGNED_MULTI,
    'macosx64-nightly-l10n': [],
}
UPSTREAM_ARTIFACT_SIGNED_PATHS = {
    'linux64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'android-x86-nightly': ["en-US/target.apk"],
    'android-api-15-nightly': ["en-US/target.apk"],
    'macosx64-nightly': [],

    'linux64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'linux-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N + [
        "target.tar.bz2",
        "target.tar.bz2.asc",
    ],
    'android-x86-nightly-multi': ["target.apk"],
    'android-api-15-nightly-l10n': ["target.apk"],
    'android-api-15-nightly-multi': ["target.apk"],
    'macosx64-nightly-l10n': [],
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
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        yield validate_schema(
            beetmover_description_schema, job,
            "In beetmover ({!r} kind) task for {!r}:".format(config.kind, label))


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        treeherder = job.get('treeherder', {})
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        label = job.get('label', "beetmover-{}".format(dep_job.label))
        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}
        # taskid_of_manifest always refers to the unsigned task
        if "signing" in dependent_kind:
            if len(dep_job.dependencies) > 1:
                raise NotImplementedError(
                    "can't beetmove a signing task with multiple dependencies")
            signing_dependencies = dep_job.dependencies
            dependencies.update(signing_dependencies)
            treeherder.setdefault('symbol', 'tc(BM-S)')
        else:
            treeherder.setdefault('symbol', 'tc(BM)')

        attributes = {
                'nightly': dep_job.attributes.get('nightly', False),
                'build_platform': dep_job.attributes.get('build_platform'),
                'build_type': dep_job.attributes.get('build_type'),
        }
        if job.get('locale'):
            attributes['locale'] = job['locale']

        task = {
            'label': label,
            'description': "{} Beetmover".format(
                dep_job.task["metadata"]["description"]),
            # do we have to define worker type somewhere?
            'worker-type': 'scriptworker-prov-v1/beetmoverworker-v1',
            # bump this to nightly / release when applicable+permitted
            'scopes': ["project:releng:beetmover:nightly"],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task


def generate_upstream_artifacts(taskid_to_beetmove, platform, locale=None, signing=False):
    task_type = "build"
    mapping = UPSTREAM_ARTIFACT_UNSIGNED_PATHS
    if signing:
        task_type = "signing"
        mapping = UPSTREAM_ARTIFACT_SIGNED_PATHS

    artifact_prefix = 'public/build'
    if locale:
        artifact_prefix = 'public/build/{}'.format(locale)
        platform = "{}-l10n".format(platform)

    upstream_artifacts = [{
        "taskId": {"task-reference": taskid_to_beetmove},
        "taskType": task_type,
        "paths": ["{}/{}".format(artifact_prefix, p) for p in mapping[platform]],
        "locale": locale or "en-US",
    }]
    if not locale and "android" in platform:
        # edge case to support 'multi' locale paths
        multi_platform = "{}-multi".format(platform)
        upstream_artifacts.append({
            "taskId": {"task-reference": taskid_to_beetmove},
            "taskType": task_type,
            "paths": ["{}/{}".format(artifact_prefix, p) for p in mapping[multi_platform]],
            "locale": "multi",
        })

    return upstream_artifacts


def generate_signing_upstream_artifacts(taskid_to_beetmove, taskid_of_manifest, platform,
                                        locale=None):
    upstream_artifacts = generate_upstream_artifacts(taskid_to_beetmove, platform, locale,
                                                     signing=True)
    if locale:
        artifact_prefix = 'public/build/{}'.format(locale)
    else:
        artifact_prefix = 'public/build'
    manifest_path = "{}/balrog_props.json".format(artifact_prefix)
    upstream_artifacts.append({
        "taskId": {"task-reference": taskid_of_manifest},
        "taskType": "build",
        "paths": [manifest_path],
        "locale": locale or "en-US",
    })

    return upstream_artifacts


def generate_build_upstream_artifacts(taskid_to_beetmove, platform, locale=None):
    upstream_artifacts = generate_upstream_artifacts(taskid_to_beetmove, platform, locale,
                                                     signing=False)
    return upstream_artifacts


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_signing_job = (len(job["dependencies"]) == 2 and
                                       any(['signing' in j for j in job['dependencies']]))
        valid_beetmover_build_job = len(job["dependencies"]) == 1
        if not valid_beetmover_build_job and not valid_beetmover_signing_job:
            raise NotImplementedError(
                "beetmover tasks must have either 1 or 2 dependencies. "
                "If 2, one of those must be a signing task"
            )

        build_kind = None
        signing_kind = None
        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]
        for dependency in job["dependencies"].keys():
            if 'signing' in dependency:
                signing_kind = dependency
            else:
                build_kind = dependency

        if signing_kind:
            taskid_to_beetmove = "<" + str(signing_kind) + ">"
            taskid_of_manifest = "<" + str(build_kind) + ">"
            update_manifest = True
            upstream_artifacts = generate_signing_upstream_artifacts(
                taskid_to_beetmove, taskid_of_manifest, platform, locale
            )
        else:
            taskid_to_beetmove = "<" + str(build_kind) + ">"
            update_manifest = False
            upstream_artifacts = generate_build_upstream_artifacts(
                taskid_to_beetmove, platform, locale
            )

        worker = {'implementation': 'beetmover',
                  'update_manifest': update_manifest,
                  'upstream-artifacts': upstream_artifacts}

        if locale:
            worker["locale"] = locale

        job["worker"] = worker

        yield job
