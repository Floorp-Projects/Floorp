# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import datetime
import json
import taskcluster

DEFAULT_EXPIRES_IN = '1 year'


class TaskBuilder(object):
    def __init__(self, task_id, repo_url, branch, commit, owner, source, scheduler_id, build_worker_type, beetmover_worker_type, tasks_priority='lowest'):
        self.task_id = task_id
        self.repo_url = repo_url
        self.branch = branch
        self.commit = commit
        self.owner = owner
        self.source = source
        self.scheduler_id = scheduler_id
        self.build_worker_type = build_worker_type
        self.beetmover_worker_type = beetmover_worker_type
        self.tasks_priority = tasks_priority

    def craft_build_task(self, module_name, gradle_tasks, subtitle='', run_coverage=False,
                         is_snapshot=False, component=None, artifacts=None, timestamp=None):
        taskcluster_artifacts = {}
        # component is not None when this is a release build, in which case artifacts is defined too
        if component is not None:
            taskcluster_artifacts = {
                artifact['taskcluster_path']: {
                    'type': 'file',
                    'expires': taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
                    'path': artifact['build_fs_path'],
                } for artifact in artifacts
            }

        scopes = [
            "secrets:get:project/mobile/android-components/public-tokens"
        ] if run_coverage else []

        snapshot_flag = '-Psnapshot -Ptimestamp={} '.format(timestamp) if is_snapshot else ''
        coverage_flag = '-Pcoverage ' if run_coverage else ''
        gradle_command = (
            './gradlew --no-daemon clean ' + coverage_flag + snapshot_flag + gradle_tasks
        )

        post_gradle_command = 'automation/taskcluster/action/upload_coverage_report.sh' if run_coverage else ''

        command = ' && '.join(cmd for cmd in (gradle_command, post_gradle_command) if cmd)

        return self._craft_build_ish_task(
            name='Android Components - Module {} {}'.format(module_name, subtitle),
            description='Execute Gradle tasks for module {}'.format(module_name),
            command=command,
            scopes=scopes,
            artifacts=taskcluster_artifacts
        )

    def craft_barrier_task(self, label, dependencies):
        return self._craft_dummy_task(
            name=label,
            description='Dummy tasks that ensures all other tasks are correctly done before publishing them',
            dependencies=dependencies,
        )

    def craft_sign_task(self, build_task_label, barrier_task_label, artifacts, component_name, is_staging):
        payload = {
            "upstreamArtifacts": [{
                "paths": [artifact["taskcluster_path"] for artifact in artifacts],
                "formats": ["autograph_gpg"],
                "taskId": {'task-reference': '<build>'},
                "taskType": "build"
            }]
        }

        return self._craft_default_task_definition(
            worker_type='mobile-signing-dep-v1' if is_staging else 'mobile-signing-v1',
            provisioner_id='scriptworker-prov-v1',
            dependencies={'build': build_task_label, 'barrier': barrier_task_label},
            routes=[],
            scopes=[
                "project:mobile:android-components:releng:signing:cert:{}-signing".format("dep" if is_staging else "release"),
            ],
            name='Android Components - Sign Module :{}'.format(component_name),
            description="Sign release module {}".format(component_name),
            payload=payload
        )

    def craft_beetmover_task(
        self, build_task_label, sign_task_label, build_artifacts, sign_artifacts, component_name, is_snapshot,
            is_staging
    ):
        if is_snapshot:
            if is_staging:
                bucket_name = 'maven-snapshot-staging'
                bucket_public_url = 'https://maven-snapshots.stage.mozaws.net/'
            else:
                bucket_name = 'maven-snapshot-production'
                bucket_public_url = 'https://snapshots.maven.mozilla.org/'
        else:
            if is_staging:
                bucket_name = 'maven-staging'
                bucket_public_url = 'https://maven-default.stage.mozaws.net/'
            else:
                bucket_name = 'maven-production'
                bucket_public_url = 'https://maven.mozilla.org/'

        payload = {
            "artifactMap": [{
                "locale": "en-US",
                "paths": {
                    artifact['taskcluster_path']: {
                        'checksums_path': '',  # TODO beetmover marks this as required even though it's not needed here
                        'destinations': [artifact['maven_destination']]
                    } for artifact in (build_artifacts)
                },
                "taskId": {'task-reference': '<build>'},
            }, {
                "locale": "en-US",
                "paths": {
                    artifact['taskcluster_path']: {
                        'checksums_path': '',  # TODO beetmover marks this as required even though it's not needed here
                        'destinations': [artifact['maven_destination']]
                    } for artifact in (sign_artifacts)
                },
                "taskId": {'task-reference': '<signing>'},
            }],
            "upstreamArtifacts": [{
                'paths': [artifact['taskcluster_path'] for artifact in build_artifacts],
                'taskId': {'task-reference': '<build>'},
                'taskType': 'build',
            }, {
                'paths': [artifact['taskcluster_path'] for artifact in sign_artifacts],
                'taskId': {'task-reference': '<signing>'},
                'taskType': 'signing',
            }],
            "releaseProperties": {
                "appName": "snapshot_components" if is_snapshot else "components",
            },
        }

        return self._craft_default_task_definition(
            self.beetmover_worker_type,
            'scriptworker-prov-v1',
            dependencies={'build': build_task_label, 'signing': sign_task_label},
            routes=[],
            scopes=[
                "project:mobile:android-components:releng:beetmover:bucket:{}".format(bucket_name),
                "project:mobile:android-components:releng:beetmover:action:push-to-maven",
            ],
            name="Android Components - Publish Module :{} via beetmover".format(component_name),
            description="Publish release module {} to {}".format(component_name, bucket_public_url),
            payload=payload
        )

    def _craft_build_ish_task(
        self, name, description, command, dependencies=None, artifacts=None, scopes=None,
        routes=None, features=None, env_vars=None
    ):
        dependencies = {} if dependencies is None else dependencies
        artifacts = {} if artifacts is None else artifacts
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
        features = {} if features is None else features
        env_vars = {} if env_vars is None else env_vars

        checkout_command = (
            "git fetch {} {} --tags && "
            "git config advice.detachedHead false && "
            "git checkout {}".format(
                self.repo_url, self.branch, self.commit
            )
        )

        command = '{} && {}'.format(checkout_command, command)

        if artifacts:
            features['chainOfTrust'] = True
        if any(scope.startswith('secrets:') for scope in scopes):
            features['taskclusterProxy'] = True

        payload = {
            "features": features,
            "env": env_vars,
            "maxRunTime": 7200,
            "image": "mozillamobile/android-components:1.20",
            "command": [
                "/bin/bash",
                "--login",
                "-cx",
                command
            ],
            "artifacts": artifacts,
        }

        return self._craft_default_task_definition(
            self.build_worker_type,
            'aws-provisioner-v1',
            dependencies,
            routes,
            scopes,
            name,
            description,
            payload
        )

    def _craft_dummy_task(
        self, name, description,  dependencies=None, routes=None, scopes=None,
    ):
        routes = []
        scopes = []
        payload = {
            "maxRunTime": 600,
            "image": "alpine",
            "command": [
                "/bin/sh",
                "--login",
                "-cx",
                "echo \"Dummy task\""
            ],
        }

        return self._craft_default_task_definition(
            self.build_worker_type,
            'aws-provisioner-v1',
            dependencies,
            routes,
            scopes,
            name,
            description,
            payload
        )

    def _craft_default_task_definition(
        self, worker_type, provisioner_id, dependencies, routes, scopes, name, description, payload
    ):
        created = datetime.datetime.now()
        deadline = taskcluster.fromNow('1 day')
        expires = taskcluster.fromNow(DEFAULT_EXPIRES_IN)

        routes.append('checks')

        return {
            "attributes": {},
            "dependencies": dependencies,
            "label": name,
            "task": {
                "provisionerId": provisioner_id,
                "workerType": worker_type,
                "taskGroupId": self.task_id,
                "schedulerId": self.scheduler_id,
                "created": taskcluster.stringDate(created),
                "deadline": taskcluster.stringDate(deadline),
                "expires": taskcluster.stringDate(expires),
                "retries": 5,
                "tags": {},
                "priority": self.tasks_priority,
                "requires": "all-completed",
                "routes": routes,
                "scopes": scopes,
                "payload": payload,
                "metadata": {
                    "name": name,
                    "description": description,
                    "owner": self.owner,
                    "source": self.source,
                },
            }
        }
