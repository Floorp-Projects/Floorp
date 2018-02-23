# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This script will be executed whenever a change is pushed to the
master branch. It will schedule multiple child tasks that build
the app, run tests and execute code quality tools:
"""

import datetime
import json
import taskcluster
import os

TASK_ID = os.environ.get('TASK_ID')
REPO_URL = os.environ.get('GITHUB_HEAD_REPO_URL')
BRANCH = os.environ.get('GITHUB_HEAD_BRANCH')
COMMIT = os.environ.get('GITHUB_HEAD_SHA')
OWNER = "skaspari@mozilla.com"
SOURCE = "https://github.com/mozilla-mobile/focus-android/tree/master/tools/taskcluster"

def generate_build_task():
	return taskcluster.slugId(), generate_task(
		name = "(Focus for Android) Build",
		description = "Build Focus/Klar for Android from source code.",
		command = ('echo "--" > .adjust_token'
				   ' && python tools/l10n/check_translations.py'
				   ' && ./gradlew --no-daemon clean assemble'))


def generate_unit_test_task(buildTaskId):
	return taskcluster.slugId(), generate_task(
		name = "(Focus for Android) Unit tests",
		description = "Run unit tests for Focus/Klar for Android.",
		command = 'echo "--" > .adjust_token && ./gradlew --no-daemon clean test',
		dependencies = [ buildTaskId ])


def generate_code_quality_task(buildTaskId):
	return taskcluster.slugId(), generate_task(
		name = "(Focus for Android) Code quality",
		description = "Run code quality tools on Focus/Klar for Android code base.",
		command = 'echo "--" > .adjust_token && ./gradlew --no-daemon clean detektCheck ktlint lint pmd checkstyle spotbugs',
		dependencies = [ buildTaskId ])


def generate_ui_test_task(dependencies):
	return taskcluster.slugId(), generate_task(
		name = "(Focus for Android) UI tests - Webview",
		description = "Run UI tests for Focus/Klar for Android.",
		command = ('echo "--" > .adjust_token'
			' && ./gradlew --no-daemon clean assembleFocusWebviewUniversalDebug assembleFocusWebviewUniversalDebugAndroidTest'
			' && ./tools/taskcluster/google-firebase-testlab-login.sh'
			' && tools/taskcluster/execute-firebase-test.sh focusWebviewUniversal app-focus-webview-universal-debug model=sailfish,version=26 model=Nexus5X,version=23 model=Nexus9,version=25 model=sailfish,version=25'),
		dependencies = dependencies,
		scopes = [ 'secrets:get:project/focus/firebase' ],
		artifacts = {
			"public": {
				"type": "directory",
				"path": "/opt/focus-android/test_artifacts",
				"expires": taskcluster.stringDate(taskcluster.fromNow('1 week'))
			}
		})

def generate_gecko_X86_ui_test_task(dependencies):
	return taskcluster.slugId(), generate_task(
		name = "(Focus for Android) UI tests - Gecko X86",
		description = "Run UI tests for Klar Gecko X86 for Android.",
		command = ('echo "--" > .adjust_token'
			' && ./gradlew --no-daemon clean assembleKlarGeckoX86Debug assembleKlarGeckoX86DebugAndroidTest'
			' && ./tools/taskcluster/google-firebase-testlab-login.sh'
			' && tools/taskcluster/execute-firebase-test.sh klarGeckoX86 app-klar-gecko-x86-debug model=Nexus5X,version=23'),
		dependencies = dependencies,
		scopes = [ 'secrets:get:project/focus/firebase' ],
		artifacts = {
			"public": {
				"type": "directory",
				"path": "/opt/focus-android/test_artifacts",
				"expires": taskcluster.stringDate(taskcluster.fromNow('1 week'))
			}
		})

def generate_gecko_ARM_ui_test_task(dependencies):
	return taskcluster.slugId(), generate_task(
		name = "(Focus for Android) UI tests - Gecko ARM",
		description = "Run UI tests for Klar Gecko ARM build for Android.",
		command = ('echo "--" > .adjust_token'
			' && ./gradlew --no-daemon clean assembleKlarGeckoArmDebug assembleKlarGeckoArmDebugAndroidTest'
			' && ./tools/taskcluster/google-firebase-testlab-login.sh'
			' && tools/taskcluster/execute-firebase-test.sh klarGeckoArm app-klar-gecko-arm-debug model=sailfish,version=26'),
		dependencies = dependencies,
		scopes = [ 'secrets:get:project/focus/firebase' ],
		artifacts = {
			"public": {
				"type": "directory",
				"path": "/opt/focus-android/test_artifacts",
				"expires": taskcluster.stringDate(taskcluster.fromNow('1 week'))
			}
		})


def generate_release_task(dependencies):
	return taskcluster.slugId(), generate_task(
		name = "(Focus for Android) Preview release",
		description = "Build preview versions for testing Focus/Klar for Android.",
		command = ('echo "--" > .adjust_token'
			       ' && ./gradlew --no-daemon clean assembleBeta'
			       ' && python tools/taskcluster/sign-preview-builds.py'
			       ' && touch /opt/focus-android/builds/`date +"%Y-%m-%d-%H-%M"`'
			       ' && touch /opt/focus-android/builds/' + COMMIT),
		dependencies = dependencies,
		scopes = [
			"secrets:get:project/focus/preview-key-store",
			"queue:route:index.project.focus.android.preview-builds"],
		routes = [ "index.project.focus.android.preview-builds" ],
		artifacts = {
			"public": {
				"type": "directory",
				"path": "/opt/focus-android/builds",
				"expires": taskcluster.stringDate(taskcluster.fromNow('1 month'))
			}
		})


def generate_task(name, description, command, dependencies = [], artifacts = {}, scopes = [], routes = []):
	created = datetime.datetime.now()
	expires = taskcluster.fromNow('1 month')
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
	    "dependencies": [ TASK_ID ] + dependencies,
	    "routes": routes,
	    "scopes": scopes,
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
				"git fetch %s %s && git config advice.detachedHead false && git checkout %s && %s" % (REPO_URL, BRANCH, COMMIT, command)
	        ],
	        "artifacts": artifacts,
	        "deadline": taskcluster.stringDate(deadline)
	    },
		"provisionerId": "aws-provisioner-v1",
		"metadata": {
			"name": name,
			"description": description,
			"owner": OWNER,
			"source": SOURCE
		}
	}


def schedule_task(queue, taskId, task):
	print "TASK", taskId
	print json.dumps(task, indent=4, separators=(',', ': '))

	result = queue.createTask(taskId, task)
	print json.dumps(result)


if __name__ == "__main__":
	queue = taskcluster.Queue({ 'baseUrl': 'http://taskcluster/queue/v1' })

	buildTaskId, buildTask = generate_build_task()
	schedule_task(queue, buildTaskId, buildTask)

	unitTestTaskId, unitTestTask = generate_unit_test_task(buildTaskId)
	schedule_task(queue, unitTestTaskId, unitTestTask)

	codeQualityTaskId, codeQualityTask = generate_code_quality_task(buildTaskId)
	schedule_task(queue, codeQualityTaskId, codeQualityTask)

	uiTestTaskId, uiTestTask = generate_ui_test_task([unitTestTaskId, codeQualityTaskId])
	schedule_task(queue, uiTestTaskId, uiTestTask)

	uiGeckoX86TestTaskId, uiGeckoX86TestTask = generate_gecko_X86_ui_test_task([unitTestTaskId, codeQualityTaskId])
	schedule_task(queue, uiGeckoX86TestTaskId, uiGeckoX86TestTask)

	uiGeckoARMTestTaskId, uiGeckoARMTestTask = generate_gecko_ARM_ui_test_task([unitTestTaskId, codeQualityTaskId])
	schedule_task(queue, uiGeckoARMTestTaskId, uiGeckoARMTestTask)

	releaseTaskId, releaseTask = generate_release_task([unitTestTaskId, codeQualityTaskId])
	schedule_task(queue, releaseTaskId, releaseTask)

