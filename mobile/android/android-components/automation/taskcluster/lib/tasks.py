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

    def craft_build_task(self, module_name, gradle_tasks, subtitle='', run_coverage=False, is_snapshot=False, artifact_info=None):
        artifacts = {} if artifact_info is None else {
            artifact_info['artifact']: {
                'type': 'file',
                'expires': taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
                'path': artifact_info['path']
            }
        }

        scopes = [
            "secrets:get:project/mobile/android-components/public-tokens"
        ] if run_coverage else []

        snapshot_flag = '-Psnapshot ' if is_snapshot else ''
        coverage_flag = '-Pcoverage ' if run_coverage else ''
        gradle_command = (
            './gradlew --no-daemon clean ' + coverage_flag + snapshot_flag + gradle_tasks
        )

        post_gradle_command = 'automation/taskcluster/action/upload_coverage_report.sh' if run_coverage else ''

        command = ' && '.join(cmd for cmd in (gradle_command, post_gradle_command) if cmd)

        features = {}
        if artifact_info is not None:
            features['chainOfTrust'] = True
        elif any(scope.startswith('secrets:') for scope in scopes):
            features['taskclusterProxy'] = True

        return self._craft_build_ish_task(
            name='Android Components - Module {} {}'.format(module_name, subtitle),
            description='Execure Gradle tasks for module {}'.format(module_name),
            command=command,
            features=features,
            scopes=scopes,
            artifacts=artifacts
        )

    def craft_wait_on_builds_task(self, dependencies):
        return self._craft_build_ish_task(
            name='Android Components - Wait on all builds to be completed',
            description='Dummy tasks that ensures all builds are correctly done before publishing them',
            dependencies=dependencies,
            command="exit 0"
        )

    def craft_detekt_task(self):
        return self._craft_build_ish_task(
            name='Android Components - detekt',
            description='Running detekt over all modules',
            command='./gradlew --no-daemon clean detekt'
        )

    def craft_ktlint_task(self):
        return self._craft_build_ish_task(
            name='Android Components - ktlint',
            description='Running ktlint over all modules',
            command='./gradlew --no-daemon clean ktlint'
        )

    def craft_compare_locales_task(self):
        return self._craft_build_ish_task(
            name='Android Components - compare-locales',
            description='Validate strings.xml with compare-locales',
            command='pip install "compare-locales>=5.0.2,<6.0" && compare-locales --validate l10n.toml .'
        )

    def craft_beetmover_task(
        self, build_task_id, wait_on_builds_task_id, version, artifact, artifact_name, is_snapshot, is_staging
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

        version = version.lstrip('v')
        payload = {
            "maxRunTime": 600,
            "upstreamArtifacts": [{
                'paths': [artifact],
                'taskId': build_task_id,
                'taskType': 'build',
                'zipExtract': True,
            }],
            "releaseProperties": {
                "appName": "snapshot_components" if is_snapshot else "components",
            },
            "version": "{}-SNAPSHOT".format(version) if is_snapshot else version,
            "artifact_id": artifact_name
        }

        return self._craft_default_task_definition(
            self.beetmover_worker_type,
            'scriptworker-prov-v1',
            dependencies=[build_task_id, wait_on_builds_task_id],
            routes=[],
            scopes=[
                "project:mobile:android-components:releng:beetmover:bucket:{}".format(bucket_name),
                "project:mobile:android-components:releng:beetmover:action:push-to-maven",
            ],
            name="Android Components - Publish Module :{} via beetmover".format(artifact_name),
            description="Publish release module {} to {}".format(artifact_name, bucket_public_url),
            payload=payload
        )

    def _craft_build_ish_task(
        self, name, description, command, dependencies=None, artifacts=None, scopes=None,
        routes=None, features=None
    ):
        dependencies = [] if dependencies is None else dependencies
        artifacts = {} if artifacts is None else artifacts
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
        features = {} if features is None else features

        checkout_command = (
            "git fetch {} {} --tags && "
            "git config advice.detachedHead false && "
            "git checkout {}".format(
                self.repo_url, self.branch, self.commit
            )
        )

        command = '{} && {}'.format(checkout_command, command)

        payload = {
            "features": features,
            "maxRunTime": 7200,
            "image": "mozillamobile/android-components:1.15",
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

    def _craft_default_task_definition(
        self, worker_type, provisioner_id, dependencies, routes, scopes, name, description, payload
    ):
        created = datetime.datetime.now()
        deadline = taskcluster.fromNow('1 day')
        expires = taskcluster.fromNow(DEFAULT_EXPIRES_IN)

        return {
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
            "dependencies": [self.task_id] + dependencies,
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


def schedule_task(queue, taskId, task):
    print("TASK", taskId)
    print(json.dumps(task, indent=4, separators=(',', ': ')))

    result = queue.createTask(taskId, task)
    print("RESULT", taskId)
    print(json.dumps(result))


def schedule_task_graph(ordered_groups_of_tasks):
    queue = taskcluster.Queue({'baseUrl': 'http://taskcluster/queue/v1'})
    full_task_graph = {}

    # TODO: Switch to async python to speed up submission
    for group_of_tasks in ordered_groups_of_tasks:
        for task_id, task_definition in group_of_tasks.items():
            schedule_task(queue, task_id, task_definition)

            full_task_graph[task_id] = {
                # Some values of the task definition are automatically filled. Querying the task
                # allows to have the full definition. This is needed to make Chain of Trust happy
                'task': queue.task(task_id),
            }
    return full_task_graph
