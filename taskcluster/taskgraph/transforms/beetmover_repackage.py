# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

import logging
logger = logging.getLogger(__name__)

# For developers: if you are adding any new artifacts here that need to be
# transfered to S3, please be aware you also need to follow-up with patch in
# the actual beetmoverscript logic that lies under
# https://github.com/mozilla-releng/beetmoverscript/. See example in bug
# 1348286
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
    "target.web-platform.tests.zip",
    "target.xpcshell.tests.zip",
    "target_info.txt",
    "target.jsshell.zip",
    "mozharness.zip",
    "target.langpack.xpi",
    "host/bin/mar",
    "host/bin/mbsdiff",
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

UPSTREAM_ARTIFACT_UNSIGNED_PATHS = {
    'macosx64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_EN_US,
    'macosx64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_UNSIGNED_L10N,
}
UPSTREAM_ARTIFACT_SIGNED_PATHS = {
    'macosx64-nightly': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_EN_US,
    'macosx64-nightly-l10n': _DESKTOP_UPSTREAM_ARTIFACTS_SIGNED_L10N,
}
UPSTREAM_ARTIFACT_REPACKAGE_PATHS = {
    'macosx64-nightly': ["target.dmg"],
    'macosx64-nightly-l10n': ["target.dmg"],
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
        treeherder.setdefault('symbol', 'tc(BM-R)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')
        label = job.get('label', "beetmover-{}".format(dep_job.label))
        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}

        # macosx nightly builds depend on repackage which use in tree docker
        # images and thus have two dependencies
        # change the signing_dependencies to be use the ones in
        docker_dependencies = {"docker-image":
                               dep_job.dependencies["docker-image"]
                               }
        dependencies.update(docker_dependencies)
        signing_dependencies = {"build-signing":
                                dep_job.dependencies["build-signing"]
                                }
        dependencies.update(signing_dependencies)
        repackage_dependencies = {"repackage":
                                  dep_job.label
                                  }
        dependencies.update(repackage_dependencies)
        build_dependencies = {"build":
                              dep_job.dependencies["build-signing"].replace("signing-", "build-")
                              }
        dependencies.update(build_dependencies)

        attributes = {
            'nightly': dep_job.attributes.get('nightly', False),
            'signed': dep_job.attributes.get('signed', False),
            'build_platform': dep_job.attributes.get('build_platform'),
            'build_type': dep_job.attributes.get('build_type'),
        }
        if job.get('locale'):
            attributes['locale'] = job['locale']

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

        task = {
            'label': label,
            'description': "{} Beetmover".format(
                dep_job.task["metadata"]["description"]),
            'worker-type': 'scriptworker-prov-v1/beetmoverworker-v1',
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task


def generate_upstream_artifacts(signing_task_ref, build_task_ref,
                                repackage_task_ref, platform,
                                locale=None):

    signing_mapping = UPSTREAM_ARTIFACT_SIGNED_PATHS
    build_mapping = UPSTREAM_ARTIFACT_UNSIGNED_PATHS
    repackage_mapping = UPSTREAM_ARTIFACT_REPACKAGE_PATHS

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
        }, {
        "taskId": {"task-reference": repackage_task_ref},
        "taskType": "repackage",
        "paths": ["{}/{}".format(artifact_prefix, p)
                  for p in repackage_mapping[platform]],
        "locale": locale or "en-US",
    }]

    return upstream_artifacts


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = (len(job["dependencies"]) == 4 and
                               any(['repackage' in j for j in job['dependencies']]))
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover_repackage must have four dependencies.")

        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]
        build_task = None
        repackage_task = None
        signing_task = None
        for dependency in job["dependencies"].keys():
            if 'repackage' in dependency:
                repackage_task = dependency
            elif 'signing' in dependency:
                signing_task = dependency
            else:
                build_task = "build"

        signing_task_ref = "<" + str(signing_task) + ">"
        build_task_ref = "<" + str(build_task) + ">"
        repackage_task_ref = "<" + str(repackage_task) + ">"
        upstream_artifacts = generate_upstream_artifacts(
            signing_task_ref, build_task_ref, repackage_task_ref, platform, locale
        )

        worker = {'implementation': 'beetmover',
                  'upstream-artifacts': upstream_artifacts}
        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
