# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.partners import (
    check_if_partners_enabled,
    get_ftp_platform,
    get_partner_config_by_kind,
)
from taskgraph.util.schema import (
    optionally_keyed_by,
    resolve_keyed_by,
)
from taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_beetmover_bucket_scope,
    get_worker_type_for_scope,
)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

from copy import deepcopy
import logging

logger = logging.getLogger(__name__)


beetmover_description_schema = schema.extend({
    # depname is used in taskref's to identify the taskID of the unsigned things
    Required('depname', default='build'): basestring,

    # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
    Optional('label'): basestring,

    Required('partner-bucket-scope'): optionally_keyed_by('release-level', basestring),
    Required('partner-public-path'): Any(None, basestring),
    Required('partner-private-path'): Any(None, basestring),

    Optional('extra'): object,
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('priority'): task_description_schema['priority'],
})

transforms = TransformSequence()
transforms.add(check_if_partners_enabled)
transforms.add_validate(beetmover_description_schema)


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'partner-bucket-scope', item_name=job['label'],
            **{'release-level': config.params.release_level()}
        )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        repack_id = dep_job.task.get('extra', {}).get('repack_id')
        if not repack_id:
            raise Exception("Cannot find repack id!")

        attributes = dep_job.attributes
        build_platform = attributes.get("build_platform")
        if not build_platform:
            raise Exception("Cannot find build platform!")

        label = dep_job.label.replace("repackage-signing-l10n", "beetmover-")
        label = dep_job.label.replace("repackage-signing-", "beetmover-")
        label = label.replace("repackage-", "beetmover-")
        label = label.replace("chunking-dummy-", "beetmover-")
        description = (
            "Beetmover submission for repack_id '{repack_id}' for build '"
            "{build_platform}/{build_type}'".format(
                repack_id=repack_id,
                build_platform=build_platform,
                build_type=attributes.get('build_type')
            )
        )

        dependencies = {}

        base_label = "release-partner-repack"
        if "eme" in config.kind:
            base_label = "release-eme-free-repack"
        dependencies["build"] = "{}-{}".format(base_label, build_platform)
        if "macosx" in build_platform or "win" in build_platform:
            dependencies["repackage"] = "{}-repackage-{}-{}".format(
                base_label, build_platform, repack_id.replace('/', '-')
            )
        dependencies["repackage-signing"] = "{}-repackage-signing-{}-{}".format(
             base_label, build_platform, repack_id.replace('/', '-')
        )

        attributes = copy_attributes_from_dependent_job(dep_job)

        task = {
            'label': label,
            'description': description,
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'shipping-phase': job['shipping-phase'],
            'shipping-product': job.get('shipping-product'),
            'partner-private-path': job['partner-private-path'],
            'partner-public-path': job['partner-public-path'],
            'partner-bucket-scope': job['partner-bucket-scope'],
            'extra': {
                'repack_id': repack_id,
            },
        }
        # we may have reduced the priority for partner jobs, otherwise task.py will set it
        if job.get('priority'):
            task['priority'] = job['priority']

        yield task


def populate_scopes_and_worker_type(config, job, bucket_scope, partner_public=False):
    action_scope = add_scope_prefix(config, 'beetmover:action:push-to-partner')

    task = deepcopy(job)
    task['scopes'] = [bucket_scope, action_scope]
    task['worker-type'] = get_worker_type_for_scope(config, bucket_scope)
    task['partner_public'] = partner_public
    if partner_public:
        task['label'] = "{}-public".format(task['label'])
    return task


@transforms.add
def split_public_and_private(config, jobs):
    public_bucket_scope = get_beetmover_bucket_scope(config)
    partner_config = get_partner_config_by_kind(config, config.kind)

    for job in jobs:
        partner_bucket_scope = add_scope_prefix(config, job['partner-bucket-scope'])
        partner, subpartner, _ = job['extra']['repack_id'].split('/')

        if partner_config[partner][subpartner].get('upload_to_candidates'):
            # public
            yield populate_scopes_and_worker_type(
                config, job, public_bucket_scope, partner_public=True
            )
        else:
            # private
            yield populate_scopes_and_worker_type(
                config, job, partner_bucket_scope, partner_public=False
            )


def generate_upstream_artifacts(job, build_task_ref, repackage_task_ref,
                                repackage_signing_task_ref, platform, repack_id,
                                partner_path, repack_stub_installer=False):

    upstream_artifacts = []
    artifact_prefix = get_artifact_prefix(job)

    if "linux" in platform:
        upstream_artifacts.append({
            "taskId": {"task-reference": build_task_ref},
            "taskType": "build",
            "paths": ["{}/{}/target.tar.bz2".format(artifact_prefix, repack_id)],
            "locale": partner_path,
        })
        upstream_artifacts.append({
            "taskId": {"task-reference": repackage_signing_task_ref},
            "taskType": "repackage",
            "paths": ["{}/{}/target.tar.bz2.asc".format(artifact_prefix, repack_id)],
            "locale": partner_path,
        })
    elif "macosx" in platform:
        upstream_artifacts.append({
            "taskId": {"task-reference": repackage_task_ref},
            "taskType": "repackage",
            "paths": ["{}/{}/target.dmg".format(artifact_prefix, repack_id)],
            "locale": partner_path,
        })
        upstream_artifacts.append({
            "taskId": {"task-reference": repackage_signing_task_ref},
            "taskType": "repackage",
            "paths": ["{}/{}/target.dmg.asc".format(artifact_prefix, repack_id)],
            "locale": partner_path,
        })
    elif "win" in platform:
        upstream_artifacts.append({
            "taskId": {"task-reference": repackage_signing_task_ref},
            "taskType": "repackage",
            "paths": ["{}/{}/target.installer.exe".format(artifact_prefix, repack_id)],
            "locale": partner_path,
        })
        upstream_artifacts.append({
            "taskId": {"task-reference": repackage_signing_task_ref},
            "taskType": "repackage",
            "paths": ["{}/{}/target.installer.exe.asc".format(artifact_prefix, repack_id)],
            "locale": partner_path,
        })
        if platform.startswith('win32') and repack_stub_installer:
            upstream_artifacts.append({
                "taskId": {"task-reference": repackage_signing_task_ref},
                "taskType": "repackage",
                "paths": ["{}/{}/target.stub-installer.exe".format(artifact_prefix, repack_id)],
                "locale": partner_path,
            })
            upstream_artifacts.append({
                "taskId": {"task-reference": repackage_signing_task_ref},
                "taskType": "repackage",
                "paths": ["{}/{}/target.stub-installer.exe.asc".format(
                    artifact_prefix, repack_id)],
                "locale": partner_path,
            })

    if not upstream_artifacts:
        raise Exception("Couldn't find any upstream artifacts.")

    return upstream_artifacts


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        platform = job["attributes"]["build_platform"]
        repack_id = job["extra"]["repack_id"]
        partner, subpartner, locale = job['extra']['repack_id'].split('/')
        partner_config = get_partner_config_by_kind(config, config.kind)
        repack_stub_installer = partner_config[partner][subpartner].get('repack_stub_installer')
        build_task = None
        repackage_task = None
        repackage_signing_task = None

        for dependency in job["dependencies"].keys():
            if 'repackage-signing' in dependency:
                repackage_signing_task = dependency
            elif 'repackage' in dependency:
                repackage_task = dependency
            else:
                build_task = "build"

        build_task_ref = "<" + str(build_task) + ">"
        repackage_task_ref = "<" + str(repackage_task) + ">"
        repackage_signing_task_ref = "<" + str(repackage_signing_task) + ">"

        # generate the partner path; we'll send this to beetmover as the "locale"
        ftp_platform = get_ftp_platform(platform)
        repl_dict = {
            "build_number": config.params['build_number'],
            "locale": locale,
            "partner": partner,
            "platform": ftp_platform,
            "release_partner_build_number": config.params['release_partner_build_number'],
            "subpartner": subpartner,
            "version": config.params['version'],
        }
        partner_public = job['partner_public']
        if partner_public:
            partner_path_key = 'partner-public-path'
        else:
            partner_path_key = 'partner-private-path'
        # Kinds can set these to None
        if not job[partner_path_key]:
            continue
        partner_path = job[partner_path_key].format(**repl_dict)
        del(job['partner_public'])
        del(job['partner-private-path'])
        del(job['partner-public-path'])
        del(job['partner-bucket-scope'])

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(
                job, build_task_ref, repackage_task_ref,
                repackage_signing_task_ref, platform, repack_id,
                partner_path, repack_stub_installer
            ),
            'partner-public': partner_public,
        }
        job["worker"] = worker

        yield job
