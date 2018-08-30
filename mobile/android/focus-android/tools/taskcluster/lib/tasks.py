# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import json
import os
import taskcluster 

class TaskBuilder(object):
    def __init__(self, task_id, repo_url, branch, commit, owner, source):
        self.task_id = task_id
        self.repo_url = repo_url
        self.branch = branch
        self.commit = commit
        self.owner = owner
        self.source = source

    def build_task(self, name, description, command, dependencies = [], artifacts = {}, scopes = [], routes = [], features = {}, worker_type = 'github-worker'):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        features = features.copy()
        features.update({
            "taskclusterProxy": True
        })

        return {
            "workerType": worker_type,
            "taskGroupId": self.task_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": "lowest",
            "schedulerId": "taskcluster-github",
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [ self.task_id ] + dependencies,
            "routes": routes,
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "features": features,
                "maxRunTime": 7200,
                "image": "mozillamobile/focus-android:1.0",
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


    def build_signing_task(self, build_task_id, name, description, apks=[], scopes=[], routes=[]):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        return {
            "workerType": 'mobile-signing-v1',
            "taskGroupId": self.task_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": "lowest",
            "schedulerId": "taskcluster-github",
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [ self.task_id, build_task_id],
            "routes": routes,
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "maxRunTime": 3600,
                "upstreamArtifacts": [
                    {
                    "paths": apks,
                    "formats": [
                        "focus-jar"
                    ],
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

    def build_push_task(self, signing_task_id, name, description, apks=[], scopes=[], track='internal', commit=False):
        created = datetime.datetime.now()
        expires = taskcluster.fromNow('1 year')
        deadline = taskcluster.fromNow('1 day')

        return {
            "workerType": 'mobile-pushapk-v1',
            "taskGroupId": self.task_id,
            "expires": taskcluster.stringDate(expires),
            "retries": 5,
            "created": taskcluster.stringDate(created),
            "tags": {},
            "priority": "lowest",
            "schedulerId": "taskcluster-github",
            "deadline": taskcluster.stringDate(deadline),
            "dependencies": [ self.task_id, signing_task_id],
            "routes": [],
            "scopes": scopes,
            "requires": "all-completed",
            "payload": {
                "commit": commit,
                "google_play_track": track,
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
                "owner": "skaspari@mozilla.com",
                "source": "https://github.com/mozilla-mobile/focus-android/tree/master/tools/taskcluster"
            }
        }

def schedule_task(queue, taskId, task):
    print "TASK", taskId
    print json.dumps(task, indent=4, separators=(',', ': '))

    result = queue.createTask(taskId, task)
    print "RESULT", taskId
    print json.dumps(result)
