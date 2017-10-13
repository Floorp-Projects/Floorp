# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is executed on taskcluster as a "decision task". It will schedule
# individual tasks for taking screenshots for all locales.

import datetime
import json
import os
import sys
import taskcluster

# Add the l10n folder to the system path so that we can import scripts from it.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'l10n'))
from locales import SCREENSHOT_LOCALES

# Get task id and other metadata from the task we are running in (This only works on taskcluster)
TASK_ID = os.environ.get('TASK_ID')

# The number of locales that we will generate screenshots for per taskcluster task.
LOCALES_PER_TASK = 5

def generate_screenshot_task(locales):
	"""Generate a new task that takes screenshots for the given set of locales"""
	parameters = " ".join(locales)

	created = datetime.datetime.now()
	expires = taskcluster.fromNow('1 week')
	deadline = taskcluster.fromNow('1 day')

	return {
	    "workerType": "github-worker",
	    "taskGroupId": TASK_ID,
	    "expires": taskcluster.stringDate(expires),
	    "retries": 5,
	    "created": taskcluster.stringDate(created),
	    "tags": {},
	    "priority": "lowest",
	    "schedulerId": "taskcluster-github",
	    "deadline": taskcluster.stringDate(deadline),
	    "dependencies": [ TASK_ID ],
	    "routes": [],
	    "scopes": [],
	    "requires": "all-completed",
	    "payload": {
	        "features": {
	            "taskclusterProxy": True
	        },
	        "maxRunTime": 7200,
	        "image": "mozillamobile/focus-android",
	        "command": [
	            "/bin/bash",
	            "--login",
	            "-c",
	            """ git fetch origin
	            && git config advice.detachedHead false
	            && git checkout origin/master
	            && /opt/focus-android/tools/taskcluster/take-screenshots.sh %s""" % parameters
	        ],
	        "artifacts": {
				"public": {
					"type": "directory",
					"path": "/opt/focus-android/fastlane/metadata/android",
					"expires": taskcluster.stringDate(expires)
				}
	        },
	        "deadline": taskcluster.stringDate(deadline)
	    },
		"provisionerId": "aws-provisioner-v1",
		"metadata": {
			"name": "Screenshots for locales: %s" % parameters,
			"description": "Generating screenshots of Focus for Android in the specified locales (%s)" % parameters,
			"owner": "skaspari@mozilla.com",
			"source": "https://github.com/mozilla-mobile/focus-android/tree/master/tools/taskcluster"
		}
	}


def chunks(locales, n):
    """Yield successive n-sized chunks from the list of locales"""
    for i in range(0, len(locales), n):
        yield locales[i:i + n]


if __name__ == "__main__":
	print "Task:", TASK_ID

	queue = taskcluster.Queue({
		'baseUrl': 'http://taskcluster/queue/v1'
	})

	for chunk in chunks(SCREENSHOT_LOCALES, LOCALES_PER_TASK):
		taskId = taskcluster.slugId()
		print "Create screenshots task (%s) for locales:" % taskId, " ".join(chunk)

		task = generate_screenshot_task(chunk)
		print json.dumps(task, indent=4, separators=(',', ': '))

		result = queue.createTask(taskId, task)
		print json.dumps(result)
