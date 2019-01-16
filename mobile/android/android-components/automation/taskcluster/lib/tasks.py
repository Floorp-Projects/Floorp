# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

    def raw_task(self, name, description, command, dependencies=[],
                   artifacts={}, scopes=[], routes=[], features={},
                   worker_type='github-worker'):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        return {
            "workerType": worker_type,
            "taskGroupId": self.task_id,
            "schedulerId": self.scheduler_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": self.tasks_priority,
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [self.task_id] + dependencies,
            "routes": routes,
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
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
            },
            "provisionerId": "aws-provisioner-v1",
            "metadata": {
                "name": name,
                "description": description,
                "owner": self.owner,
                "source": self.source
            }
        }

    def build_task(self, name, description, command, dependencies=[],
                   artifacts={}, scopes=[], routes=[], features={}):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        features = features.copy()
        features.update({
            "taskclusterProxy": True
        })

        return {
            "workerType": self.build_worker_type,
            "taskGroupId": self.task_id,
            "schedulerId": self.scheduler_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": self.tasks_priority,
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [self.task_id] + dependencies,
            "routes": routes,
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "features": features,
                "maxRunTime": 7200,
                "image": "mozillamobile/android-components:1.15",
                "command": [
                    "/bin/bash",
                    "--login",
                    "-cx",
                    command
                ],
                "artifacts": artifacts
            },
            "provisionerId": "aws-provisioner-v1",
            "metadata": {
                "name": name,
                "description": description,
                "owner": self.owner,
                "source": self.source
            }
        }

    def beetmover_task(self, name, description, version, artifact_id,
                       dependencies=[], upstreamArtifacts=[], scopes=[],
                       is_snapshot=False):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        return {
            "workerType": self.beetmover_worker_type,
            "taskGroupId": self.task_id,
            "schedulerId": self.scheduler_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": self.tasks_priority,
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [self.task_id] + dependencies,
            "routes": [],
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "maxRunTime": 600,
                "upstreamArtifacts": upstreamArtifacts,
                "releaseProperties": {
                    "appName": "snapshot_components" if is_snapshot else "components",
                },
                "version": "{}-SNAPSHOT".format(version) if is_snapshot else version,
                "artifact_id": artifact_id
            },
            "provisionerId": "scriptworker-prov-v1",
            "metadata": {
                "name": name,
                "description": description,
                "owner": self.owner,
                "source": self.source
            }
        }


def schedule_task(queue, taskId, task):
    print "TASK", taskId
    print json.dumps(task, indent=4, separators=(',', ': '))

    result = queue.createTask(taskId, task)
    print "RESULT", taskId
    print json.dumps(result)
