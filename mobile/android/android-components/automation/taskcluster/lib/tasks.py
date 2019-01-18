# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import datetime
import json
import taskcluster


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

    def craft_build_ish_task(
        self, name, description, command, dependencies=None, artifacts=None, scopes=None,
        routes=None, features=None
    ):
        dependencies = [] if dependencies is None else dependencies
        artifacts = {} if artifacts is None else artifacts
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
        features = {} if features is None else features

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

    def craft_beetmover_task(
        self, name, description, version, artifact_id, dependencies=None, upstreamArtifacts=None,
        scopes=None, is_snapshot=False
    ):
        dependencies = [] if dependencies is None else dependencies
        upstreamArtifacts = [] if upstreamArtifacts is None else upstreamArtifacts
        scopes = [] if scopes is None else scopes

        payload = {
            "maxRunTime": 600,
            "upstreamArtifacts": upstreamArtifacts,
            "releaseProperties": {
                "appName": "snapshot_components" if is_snapshot else "components",
            },
            "version": "{}-SNAPSHOT".format(version) if is_snapshot else version,
            "artifact_id": artifact_id
        }

        return self._craft_default_task_definition(
            self.beetmover_worker_type,
            'scriptworker-prov-v1',
            dependencies,
            [],
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
        expires = taskcluster.fromNow('1 year')

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
