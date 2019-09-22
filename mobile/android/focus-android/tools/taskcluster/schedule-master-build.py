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

from lib.tasks import schedule_task


TASK_ID = os.environ.get('TASK_ID')
REPO_URL = os.environ.get('MOBILE_HEAD_REPOSITORY')
BRANCH = os.environ.get('MOBILE_HEAD_BRANCH')
COMMIT = os.environ.get('MOBILE_HEAD_REV')
OWNER = "skaspari@mozilla.com"
SOURCE = "https://github.com/mozilla-mobile/focus-android/tree/master/tools/taskcluster"


def generate_build_task():
    return taskcluster.slugId(), generate_task(
        name="(Focus for Android) Build",
        description="Build Focus/Klar for Android from source code.",
        command=('echo "--" > .adjust_token'
                 ' && python tools/l10n/check_translations.py'
                 ' && ./gradlew --no-daemon clean assemble'))


def generate_unit_test_task(buildTaskId):
    return taskcluster.slugId(), generate_task(
        name="(Focus for Android) Unit tests",
        description="Run unit tests for Focus/Klar for Android.",
        command='echo "--" > .adjust_token && ./gradlew --no-daemon clean test',
        dependencies=[buildTaskId])


def generate_code_quality_task(buildTaskId):
    return taskcluster.slugId(), generate_task(
        name="(Focus for Android) Code quality",
        description="Run code quality tools on Focus/Klar for Android code base.",
        command='echo "--" > .adjust_token && ./gradlew --no-daemon clean detektCheck ktlint lint pmd checkstyle spotbugs',
        dependencies=[buildTaskId])


def generate_compare_locales_task():
    return taskcluster.slugId(), generate_task(
        name="(Focus for Android) String validation",
        description="Check Focus/Klar for Android for errors in en-US and l10n.",
        command=('pip install "compare-locales>=5.0.2,<6.0"'
                 ' && mkdir -p /opt/focus-android/test_artifacts'
                 ' && compare-locales --validate l10n.toml .'
                 ' && compare-locales --json=/opt/focus-android/test_artifacts/data.json l10n.toml .'),
        artifacts={
            "public": {
                "type": "directory",
                "path": "/opt/focus-android/test_artifacts",
                "expires": taskcluster.stringDate(taskcluster.fromNow('1 week'))
            }
        })


def generate_ui_test_task(dependencies, engine="Klar", device="arm"):
    '''
    :param str engine: Klar, Webview
    :param str device: ARM, X86
    :return: uiWebviewARMTestTaskId, uiWebviewARMTestTask
    '''
    if engine is "Klar":
        engine = "geckoview"
        assemble_engine = engine 
    elif engine is "Webview":
        engine = "webview"
        assemble_engine = "Focus"
    else:
        raise Exception("ERROR: unknown engine type --> Aborting!")

    task_name = "(Focus for Android) UI tests - {0} {1}".format(engine, device)
    task_description = "Run UI tests for {0} build for Android.".format(engine, device)
    build_dir = "assemble{0}Debug".format(assemble_engine, device.capitalize())
    build_dir_test = "assemble{0}DebugAndroidTest".format(assemble_engine, device.capitalize())
    print('BUILD_DIR: {0}'.format(build_dir))
    print('BUILD_DIR_TEST: {0}'.format(build_dir_test))
    device = device.lower()

    return taskcluster.slugId(), generate_task(
        name=task_name,
        description=task_description,
        command=('echo "--" > .adjust_token'
                 ' && ./gradlew --no-daemon clean ' + build_dir + ' ' + build_dir_test + ' '
                 ' && ./tools/taskcluster/google-firebase-testlab-login.sh'
                 ' && tools/taskcluster/execute-firebase-tests.sh ' + device + ' ' + engine),
        dependencies=dependencies,
        scopes=['secrets:get:project/focus/firebase'],
        routes=['notify.irc-channel.#android-ci.on-any'],
        artifacts={
            "public": {
                "type": "directory",
                "path": "/opt/focus-android/test_artifacts",
                "expires": taskcluster.stringDate(taskcluster.fromNow('1 week'))
            }
        })


# For GeckoView, upload nightly (it has release config) by default, all Release builds have WV
def upload_apk_nimbledroid_task(dependencies):
    return taskcluster.slugId(), generate_task(
        name="(Focus for Android) Upload Debug APK to Nimbledroid",
        description="Upload APKs to Nimbledroid for performance measurement and tracking.",
        command=('echo "--" > .adjust_token'
                 ' && ./gradlew --no-daemon clean assembleKlarArmNightly'
                 ' && python tools/taskcluster/upload_apk_nimbledroid.py'),
        dependencies=dependencies,
        scopes=['secrets:get:project/focus/nimbledroid'],
    )


def generate_task(name, description, command, dependencies=[], artifacts={}, scopes=[], routes=[]):
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
        "dependencies": [TASK_ID] + dependencies,
        "routes": routes,
        "scopes": scopes,
        "requires": "all-completed",
        "payload": {
            "features": {
                "taskclusterProxy": True
            },
            "maxRunTime": 7200,
            "image": "mozillamobile/focus-android:1.2",
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


if __name__ == "__main__":

    queue = taskcluster.Queue({'baseUrl': 'http://taskcluster/queue/v1'})

    buildTaskId, buildTask = generate_build_task()
    schedule_task(queue, buildTaskId, buildTask)

    unitTestTaskId, unitTestTask = generate_unit_test_task(buildTaskId)
    schedule_task(queue, unitTestTaskId, unitTestTask)

    codeQualityTaskId, codeQualityTask = generate_code_quality_task(buildTaskId)
    schedule_task(queue, codeQualityTaskId, codeQualityTask)

    clTaskId, clTask = generate_compare_locales_task()
    schedule_task(queue, clTaskId, clTask)

    uiWebviewARMTestTaskId, uiWebviewARMTestTask = generate_ui_test_task( [unitTestTaskId, codeQualityTaskId], "Webview", "ARM")
    schedule_task(queue, uiWebviewARMTestTaskId, uiWebviewARMTestTask)

    # uiWebviewX86TestTaskId, uiWebviewX86TestTask = generate_ui_test_task([unitTestTaskId, codeQualityTaskId], "Webview", "X86")
    # schedule_task(queue, uiWebviewX86TestTaskId, uiWebviewX86TestTask)

    uploadNDTaskId, uploadNDTask = upload_apk_nimbledroid_task([unitTestTaskId, codeQualityTaskId])
    schedule_task(queue, uploadNDTaskId, uploadNDTask)
