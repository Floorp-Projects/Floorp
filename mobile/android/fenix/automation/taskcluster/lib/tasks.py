# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import arrow
import datetime
import json
import os
import taskcluster

from ..lib.util import upper_case_first_letter, convert_camel_case_into_kebab_case, lower_case_first_letter

DEFAULT_EXPIRES_IN = '1 year'
DEFAULT_APK_ARTIFACT_LOCATION = 'public/target.apk'
_OFFICIAL_REPO_URL = 'https://github.com/mozilla-mobile/fenix'
_DEFAULT_TASK_URL = 'https://queue.taskcluster.net/v1/task'
GOOGLE_APPLICATION_CREDENTIALS = '.firebase_token.json'
# Bug 1558456 - Stop tracking youtube-playback-test on motoG5 for >1080p cases
ARM_RAPTOR_URL_PARAMS = [
    "exclude=1,2,9,10,17,18,21,22,26,28,30,32,39,40,47,"
    "48,55,56,63,64,71,72,79,80,83,84,89,90,95,96",
]


class TaskBuilder(object):
    def __init__(
        self,
        task_id,
        repo_url,
        git_ref,
        short_head_branch, commit, owner, source, scheduler_id, date_string,
        tasks_priority='lowest',
        trust_level=1
    ):
        self.task_id = task_id
        self.repo_url = repo_url
        self.git_ref = git_ref
        self.short_head_branch = short_head_branch
        self.commit = commit
        self.owner = owner
        self.source = source
        self.scheduler_id = scheduler_id
        self.trust_level = trust_level
        self.tasks_priority = tasks_priority
        self.date = arrow.get(date_string, 'YYYYMMDDHHmmss')
        self.trust_level = trust_level

    def craft_assemble_release_task(self, variant, channel, is_staging, version_name):
        if is_staging:
            secret_index = 'garbage/staging/project/mobile/fenix'
        else:
            secret_index = 'project/mobile/fenix/{}'.format(channel)

        pre_gradle_commands = (
            'python automation/taskcluster/helper/get-secret.py -s {} -k {} -f {}'.format(
                secret_index, key, target_file
            )
            for key, target_file in (
                ('sentry_dsn', '.sentry_token'),
                ('leanplum', '.leanplum_token'),
                ('adjust', '.adjust_token'),
                ('digital_asset_links', '.digital_asset_links_token'),
                ('firebase', 'app/src/{}/res/values/firebase.xml'.format(variant.build_type)),
            )
        )

        capitalized_build_type = upper_case_first_letter(variant.build_type)
        gradle_commands = (
            './gradlew --no-daemon -PversionName="{}" clean test assemble{}'.format(
                version_name, capitalized_build_type),
        )

        command = ' && '.join(
            cmd
            for commands in (pre_gradle_commands, gradle_commands)
            for cmd in commands
            if cmd
        )

        routes = [] if is_staging else [
            "notify.email.fenix-eng-notifications@mozilla.com.on-failed"
        ]

        return self._craft_build_ish_task(
            name='Build {} task'.format(capitalized_build_type),
            description='Build Fenix {} from source code'.format(capitalized_build_type),
            command=command,
            scopes=[
                "secrets:get:{}".format(secret_index)
            ],
            artifacts=variant.artifacts(),
            routes=routes,
            treeherder={
                'jobKind': 'build',
                'machine': {
                    'platform': 'android-all',
                },
                'collection': {
                    'opt': True,
                },
                'symbol': '{}-A'.format(variant.build_type),
                'tier': 1,
            },
            attributes={
                'apks': variant.upstream_artifacts_per_abi,
            }
        )

    def craft_assemble_raptor_task(self, variant):
        command = ' && '.join((
            'echo "https://fake@sentry.prod.mozaws.net/368" > .sentry_token',
            'echo "--" > .adjust_token',
            'echo "-:-" > .leanplum_token',
            'touch .digital_asset_links_token',
            './gradlew --no-daemon clean assemble{}'.format(variant.name),
        ))

        return self._craft_build_ish_task(
            name='assemble: {}'.format(variant.name),
            description='Building and testing variant {}'.format(variant.name),
            command=command,
            artifacts=variant.artifacts(),
            treeherder={
                'groupSymbol': variant.build_type,
                'jobKind': 'build',
                'machine': {
                    'platform': 'android-all',
                },
                'symbol': 'A',
                'tier': 1,
            },
            attributes={
                'build-type': 'raptor',
                'apks': variant.upstream_artifacts_per_abi,
            },
        )

    def craft_assemble_pr_task(self, variant):
        return self._craft_clean_gradle_task(
            name='assemble: {}'.format(variant.name),
            description='Building and testing variant {}'.format(variant.name),
            gradle_task='assemble{}'.format(variant.name),
            artifacts=variant.artifacts(),
            treeherder={
                'groupSymbol': variant.build_type,
                'jobKind': 'build',
                'machine': {
                    'platform': 'android-all',
                },
                'symbol': 'A',
                'tier': 1,
            },
        )

    def craft_test_pr_task(self, variant):
        # upload coverage only once, if the variant is arm64
        test_gradle_command = \
            '-Pcoverage jacocoGeckoNightlyDebugTestReport && automation/taskcluster/upload_coverage_report.sh'

        return self._craft_clean_gradle_task(
            name='test: {}'.format(variant.name),
            description='Building and testing variant {}'.format(variant.name),
            gradle_task=test_gradle_command,
            treeherder={
                'groupSymbol': variant.build_type,
                'jobKind': 'test',
                'machine': {
                  'platform': 'android-all',
                },
                'symbol': 'T',
                'tier': 1,
            },
            scopes=[
                'secrets:get:project/mobile/fenix/public-tokens'
            ]
        )

    def craft_ui_tests_task(self):
        artifacts = {
            "public": {
                "type": "directory",
                "path": "/build/fenix/results",
                "expires": taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN))
            }
        }

        env_vars = {
            "GOOGLE_PROJECT": "moz-fenix",
            "GOOGLE_APPLICATION_CREDENTIALS": ".firebase_token.json"
        }

        gradle_commands = (
            './gradlew --no-daemon clean assemble assembleAndroidTest',
        )

        test_commands = (
            'automation/taskcluster/androidTest/ui-test.sh arm64-v8a -1',
            'automation/taskcluster/androidTest/ui-test.sh armeabi-v7a -1',
        )

        command = ' && '.join(
            cmd
            for commands in (gradle_commands, test_commands)
            for cmd in commands
            if cmd
        )

        treeherder = {
            'jobKind': 'test',
            'machine': {
                'platform': 'ui-test',
            },
            'symbol': 'ui-test',
            'tier': 2,
        }

        return self._craft_build_ish_task(
            name='Fenix - UI test',
            description='Execute Gradle tasks for UI tests',
            command=command,
            scopes=[
                'secrets:get:project/mobile/fenix/firebase'
            ],
            artifacts=artifacts,
            env_vars=env_vars,
            treeherder=treeherder,
        )

    def craft_upload_apk_nimbledroid_task(self, assemble_task_label):
        # For GeckoView, upload nightly (it has release config) by default, all Release builds have WV
        return self._craft_build_ish_task(
            name="Upload Release APK to Nimbledroid",
            description='Upload APKs to Nimbledroid for performance measurement and tracking.',
            command=' && '.join([
                'curl --location "{}/<build>/artifacts/public/build/armeabi-v7a/geckoNightly/target.apk" > target.apk'.format(_DEFAULT_TASK_URL),
                'python automation/taskcluster/upload_apk_nimbledroid.py',
            ]),
            treeherder={
                'jobKind': 'test',
                'machine': {
                  'platform': 'android-all',
                },
                'symbol': 'compare-locale',
                'tier': 2,
            },
            scopes=["secrets:get:project/mobile/fenix/nimbledroid"],
            dependencies={'build': assemble_task_label},
        )

    def craft_detekt_task(self):
        return self._craft_clean_gradle_task(
            name='detekt',
            description='Running detekt code quality checks',
            gradle_task='detekt',
            treeherder={
                'jobKind': 'test',
                'machine': {
                  'platform': 'lint',
                },
                'symbol': 'detekt',
                'tier': 1,
            }

        )

    def craft_ktlint_task(self):
        return self._craft_clean_gradle_task(
            name='ktlint',
            description='Running ktlint code quality checks',
            gradle_task='ktlint',
            treeherder={
                'jobKind': 'test',
                'machine': {
                  'platform': 'lint',
                },
                'symbol': 'ktlint',
                'tier': 1,
            }
        )

    def craft_lint_task(self):
        return self._craft_clean_gradle_task(
            name='lint',
            description='Running lint for aarch64 release variant',
            gradle_task='lintDebug',
            treeherder={
                'jobKind': 'test',
                'machine': {
                  'platform': 'lint',
                },
                'symbol': 'lint',
                'tier': 1,
            },
        )

    def _craft_clean_gradle_task(
        self, name, description, gradle_task, artifacts=None, routes=None, treeherder=None, scopes=None
    ):
        return self._craft_build_ish_task(
            name=name,
            description=description,
            command='./gradlew --no-daemon clean {}'.format(gradle_task),
            artifacts=artifacts,
            routes=routes,
            treeherder=treeherder,
            scopes=scopes,
        )

    def craft_compare_locales_task(self):
        return self._craft_build_ish_task(
            name='compare-locales',
            description='Validate strings.xml with compare-locales',
            command=(
                'pip install "compare-locales>=5.0.2,<6.0" && '
                'compare-locales --validate l10n.toml .'
            ),
            treeherder={
                'jobKind': 'test',
                'machine': {
                  'platform': 'lint',
                },
                'symbol': 'compare-locale',
                'tier': 2,
            }
        )

    def _craft_build_ish_task(
        self, name, description, command, dependencies=None, artifacts=None,
        routes=None, treeherder=None, env_vars=None, scopes=None, attributes=None
    ):
        artifacts = {} if artifacts is None else artifacts
        scopes = [] if scopes is None else scopes
        env_vars = {} if env_vars is None else env_vars

        checkout_command = ' && '.join([
            "export TERM=dumb",
            "git fetch {} {}".format(self.repo_url, self.git_ref),
            "git config advice.detachedHead false",
            "git checkout FETCH_HEAD",
        ])

        command = '{} && {}'.format(checkout_command, command)

        features = {}
        if artifacts:
            features['chainOfTrust'] = True
        if any(scope.startswith('secrets:') for scope in scopes):
            features['taskclusterProxy'] = True
        payload = {
            "features": features,
            "env": env_vars,
            "maxRunTime": 7200,
            "image": "mozillamobile/fenix:1.4",
            "command": [
                "/bin/bash",
                "--login",
                "-cx",
                # Some tasks like nimbledroid do have tasks references
                {'task-reference': command},
            ],
            "artifacts": artifacts,
        }

        return self._craft_default_task_definition(
            worker_type='mobile-{}-b-fenix'.format(self.trust_level),
            provisioner_id='aws-provisioner-v1',
            name=name,
            description=description,
            payload=payload,
            dependencies=dependencies,
            routes=routes,
            scopes=scopes,
            treeherder=treeherder,
            attributes=attributes,
        )

    def _craft_default_task_definition(
        self,
        worker_type,
        provisioner_id,
        name,
        description,
        payload,
        dependencies=None,
        routes=None,
        scopes=None,
        treeherder=None,
        notify=None,
        attributes=None
    ):
        dependencies = {} if dependencies is None else dependencies
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
        treeherder = {} if treeherder is None else treeherder
        attributes = {} if attributes is None else attributes

        created = datetime.datetime.now()
        deadline = taskcluster.fromNow('1 day')
        expires = taskcluster.fromNow(DEFAULT_EXPIRES_IN)

        routes.append('checks')
        if self.trust_level == 3:
            routes.append('tc-treeherder.v2.fenix.{}'.format(self.commit))

        extra = {
            "treeherder": treeherder,
        }
        if notify:
            extra['notify'] = notify

        return {
            "attributes": attributes,
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
                "extra": extra,
                "metadata": {
                    "name": "Fenix - {}".format(name),
                    "description": description,
                    "owner": self.owner,
                    "source": self.source,
                },
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


def fetch_mozharness_task_id():
    # We now want to use the latest available raptor
    raptor_index = 'gecko.v2.mozilla-central.nightly.latest.mobile.android-x86_64-opt'
    return taskcluster.Index({
      'rootUrl': os.environ.get('TASKCLUSTER_PROXY_URL', 'https://taskcluster.net'),
    }).findTask(raptor_index)['taskId']
