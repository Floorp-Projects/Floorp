# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import json
import taskcluster


class TaskBuilder(object):
    def __init__(self, task_id, owner, source, scheduler_id, build_worker_type):
        self.task_id = task_id
        self.owner = owner
        self.source = source
        self.scheduler_id = scheduler_id
        self.build_worker_type = build_worker_type

    def build_task(self, name, description, command, artifacts, features, scopes=[], routes=[]):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        return {
            "workerType": self.build_worker_type,
            "taskGroupId": self.task_id,
            "schedulerId": self.scheduler_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": "lowest",
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [self.task_id],
            "routes": routes,
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "features": features,
                "maxRunTime": 7200,
                "image": "mozillamobile/fenix:1.3",
                "command": [
                    "/bin/bash",
                    "--login",
                    "-c",
                    command
                ],
                "artifacts": artifacts,
                "deadline": taskcluster.stringDate(deadline)
            },
            "provisionerId": "aws-provisioner-v1",
            "metadata": {
                "name": name,
                "description": description,
                "owner": self.owner,
                "source": self.source
            }
        }

    def craft_signing_task(self, build_task_id, name, description, signing_format, is_staging, apks, scopes, routes):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        return {
            "workerType": 'mobile-signing-dep-v1' if is_staging else 'mobile-signing-v1',
            "taskGroupId": self.task_id,
            "schedulerId": self.scheduler_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": "lowest",
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [self.task_id, build_task_id],
            "routes": routes,
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "maxRunTime": 3600,
                "upstreamArtifacts": [
                    {
                        "paths": apks,
                        "formats": [signing_format],
                        "taskId": build_task_id,
                        "taskType": "build"
                    }
                ]
            },
            "provisionerId": "scriptworker-prov-v1",
            "metadata": {
                "name": name,
                "description": description,
                "owner": self.owner,
                "source": self.source
            }
        }

    def craft_push_task(self, signing_task_id, name, description, is_staging, apks, scopes, commit):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        return {
            "workerType": 'mobile-pushapk-dep-v1' if is_staging else 'mobile-pushapk-v1',
            "taskGroupId": self.task_id,
            "schedulerId": self.scheduler_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": "lowest",
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [self.task_id, signing_task_id],
            "routes": [],
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "commit": commit,
                "google_play_track": 'nightly',
                "upstreamArtifacts": [
                    {
                        "paths": apks,
                        "taskId": signing_task_id,
                        "taskType": "signing"
                    }
                ]
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
