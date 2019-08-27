# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import arrow
import datetime
import json
import taskcluster

from lib.util import upper_case_first_letter, convert_camel_case_into_kebab_case, lower_case_first_letter

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
        self.date = arrow.get(date_string)
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
                'symbol': '{}-A'.format(variant.build_type),
                'tier': 1,
            },
        )

    def craft_assemble_raptor_task(self, variant):
        command = ' && '.join((
            'echo "https://fake@sentry.prod.mozaws.net/368" > .sentry_token',
            'echo "--" > .adjust_token',
            'echo "-:-" > .leanplum_token',
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
            }
        )

    def craft_test_pr_task(self, variant):
        command = 'test{}UnitTest'.format(variant.name)
        return self._craft_clean_gradle_task(
            name='test: {}'.format(variant.name),
            description='Building and testing variant {}'.format(variant.name),
            gradle_task=command,
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
            './gradlew --no-daemon clean assembleArmDebug assembleArmDebugAndroidTest',
        )

        test_commands = (
            'automation/taskcluster/androidTest/ui-test.sh arm -1',
        )

        command = ' && '.join(
            cmd
            for commands in (gradle_commands, test_commands)
            for cmd in commands
            if cmd
        )

        return self._craft_build_ish_task(
            name='Fenix - UI test',
            description='Execute Gradle tasks for UI tests',
            command=command,
            scopes=[
                'secrets:get:project/mobile/fenix/firebase'
            ],
            artifacts=artifacts,
            env_vars=env_vars,
        )

    def craft_upload_apk_nimbledroid_task(self, assemble_task_id):
        # For GeckoView, upload nightly (it has release config) by default, all Release builds have WV
        return self._craft_build_ish_task(
            name="Upload Release APK to Nimbledroid",
            description='Upload APKs to Nimbledroid for performance measurement and tracking.',
            command=' && '.join([
                'curl --location "{}/{}/artifacts/public/build/armeabi-v7a/geckoNightly/target.apk" > target.apk'.format(_DEFAULT_TASK_URL, assemble_task_id),
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
            dependencies=[assemble_task_id],
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

    def craft_dependencies_task(self):
        # Output the dependencies to an artifact.  This is used by the
        # telemetry probe scraper to determine all of the metrics that
        # Fenix might send (both from itself and any of its dependent
        # libraries that use Glean).
        return self._craft_clean_gradle_task(
            name='dependencies',
            description='Write dependencies to a build artifact',
            gradle_task='app:dependencies --configuration implementation > dependencies.txt',
            treeherder={
                'jobKind': 'test',
                'machine': {
                    'platform': 'lint',
                },
                'symbol': 'dependencies',
                'tier': 1,
            },
            routes=[
                'index.project.mobile.fenix.v2.branch.master.revision.{}'.format(self.commit)
            ],
            artifacts={
                'public/dependencies.txt': {
                    "type": 'file',
                    "path": '/opt/fenix/dependencies.txt',
                    "expires": taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
                }
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
        routes=None, treeherder=None, env_vars=None, scopes=None
    ):
        dependencies = [] if dependencies is None else dependencies
        artifacts = {} if artifacts is None else artifacts
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
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
                command
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
        )

    def _craft_signing_task(self, name, description, signing_type, assemble_task_id, apk_paths, routes, treeherder):
        signing_format = "autograph_apk"
        payload = {
            'upstreamArtifacts': [{
                'paths': apk_paths,
                'formats': [signing_format],
                'taskId': assemble_task_id,
                'taskType': 'build'
            }]
        }

        return self._craft_default_task_definition(
            worker_type='mobile-signing-dep-v1' if signing_type == 'dep' else 'mobile-signing-v1',
            provisioner_id='scriptworker-prov-v1',
            dependencies=[assemble_task_id],
            routes=routes,
            scopes=[
                "project:mobile:fenix:releng:signing:format:{}".format(signing_format),
                "project:mobile:fenix:releng:signing:cert:{}-signing".format(signing_type),
            ],
            name=name,
            description=description,
            payload=payload,
            treeherder=treeherder,
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
    ):
        dependencies = [] if dependencies is None else dependencies
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
        treeherder = {} if treeherder is None else treeherder

        created = datetime.datetime.now()
        deadline = taskcluster.fromNow('1 day')
        expires = taskcluster.fromNow(DEFAULT_EXPIRES_IN)

        if self.trust_level == 3:
            routes.append('tc-treeherder.v2.fenix.{}'.format(self.commit))

        extra = {
            "treeherder": treeherder,
        }
        if notify:
            extra['notify'] = notify

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
            "extra": extra,
            "metadata": {
                "name": "Fenix - {}".format(name),
                "description": description,
                "owner": self.owner,
                "source": self.source,
            },
        }

    def craft_raptor_signing_task(
        self, assemble_task_id, variant, is_staging,
    ):
        staging_prefix = '.staging' if is_staging else ''
        routes = [
            "index.project.mobile.fenix.v2{}.performance-test.{}.{}.{}.latest".format(
                staging_prefix, self.date.year, self.date.month, self.date.day
            ),
            "index.project.mobile.fenix.v2{}.performance-test.{}.{}.{}.revision.{}".format(
                staging_prefix, self.date.year, self.date.month, self.date.day, self.commit
            ),
            "index.project.mobile.fenix.v2{}.performance-test.latest".format(staging_prefix),
        ]

        return self._craft_signing_task(
            name='sign: {}'.format('forPerformanceTest'),
            description='Dep-signing variant {}'.format('forPerformanceTest'),
            signing_type="dep",
            assemble_task_id=assemble_task_id,
            apk_paths=variant.upstream_artifacts(),
            routes=routes,
            treeherder={
                'groupSymbol': 'forPerformanceTest',
                'jobKind': 'other',
                'machine': {
                    'platform': 'android-all',
                },
                'symbol': 'As',
                'tier': 1,
            },
        )

    def craft_release_signing_task(
        self, build_task_id, apk_paths, channel, is_staging, index_channel=None
    ):
        index_channel = index_channel or channel
        staging_prefix = '.staging' if is_staging else ''

        routes = [
            "index.project.mobile.fenix.v2{}.{}.{}.{}.{}.latest".format(
                staging_prefix, index_channel, self.date.year, self.date.month, self.date.day
            ),
            "index.project.mobile.fenix.v2{}.{}.{}.{}.{}.revision.{}".format(
                staging_prefix, index_channel, self.date.year, self.date.month, self.date.day, self.commit
            ),
            "index.project.mobile.fenix.v2{}.{}.latest".format(staging_prefix, index_channel),
        ]

        capitalized_channel = upper_case_first_letter(channel)
        return self._craft_signing_task(
            name="Signing {} task".format(capitalized_channel),
            description="Sign {} builds of Fenix".format(capitalized_channel),
            signing_type="dep" if is_staging else channel,
            assemble_task_id=build_task_id,
            apk_paths=apk_paths,
            routes=routes,
            treeherder={
                'jobKind': 'other',
                'machine': {
                  'platform': 'android-all',
                },
                'symbol': '{}-s'.format(channel),
                'tier': 1,
            },
        )

    def craft_push_task(
        self, signing_task_id, apk_paths, channel, is_staging=False, override_google_play_track=None
    ):
        payload = {
            "commit": True,
            "channel": channel,
            "certificate_alias": 'fenix' if is_staging else 'fenix-{}'.format(channel),
            "upstreamArtifacts": [
                {
                    "paths": apk_paths,
                    "taskId": signing_task_id,
                    "taskType": "signing"
                }
            ]
        }

        if override_google_play_track:
            payload['google_play_track'] = override_google_play_track

        return self._craft_default_task_definition(
            worker_type='mobile-pushapk-dep-v1' if is_staging else 'mobile-pushapk-v1',
            provisioner_id='scriptworker-prov-v1',
            dependencies=[signing_task_id],
            routes=[],
            scopes=[
                "project:mobile:fenix:releng:googleplay:product:fenix{}".format(
                    ':dep' if is_staging else ''
                )
            ],
            name="Push task",
            description="Upload signed release builds of Fenix to Google Play",
            payload=payload,
            treeherder={
                'jobKind': 'other',
                'machine': {
                  'platform': 'android-all',
                },
                'symbol': '{}-gp'.format(channel),
                'tier': 1,
            },
        )

    def craft_raptor_tp6m_cold_task(self, for_suite):

        def craft_function(signing_task_id, mozharness_task_id, abi, gecko_revision, force_run_on_64_bit_device=False):
            return self._craft_raptor_task(
                signing_task_id,
                mozharness_task_id,
                abi,
                gecko_revision,
                name_prefix='raptor tp6m-cold-{}'.format(for_suite),
                description='Raptor tp6m cold on Fenix',
                test_name='raptor-tp6m-cold-{}'.format(for_suite),
                job_symbol='tp6m-c-{}'.format(for_suite),
                force_run_on_64_bit_device=force_run_on_64_bit_device,
            )
        return craft_function

    def craft_raptor_youtube_playback_task(self, signing_task_id, mozharness_task_id, abi, gecko_revision,
                                           force_run_on_64_bit_device=False):
        return self._craft_raptor_task(
            signing_task_id,
            mozharness_task_id,
            abi,
            gecko_revision,
            name_prefix='raptor youtube playback',
            description='Raptor YouTube Playback on Fenix',
            test_name='raptor-youtube-playback',
            job_symbol='ytp',
            group_symbol='Rap',
            force_run_on_64_bit_device=force_run_on_64_bit_device,
        )

    def _craft_raptor_task(
        self,
        signing_task_id,
        mozharness_task_id,
        abi,
        gecko_revision,
        name_prefix,
        description,
        test_name,
        job_symbol,
        group_symbol=None,
        force_run_on_64_bit_device=False,
    ):
        worker_type = 'gecko-t-bitbar-gw-perf-p2' if force_run_on_64_bit_device or abi == 'aarch64' else 'gecko-t-bitbar-gw-perf-g5'

        if force_run_on_64_bit_device:
            treeherder_platform = 'android-hw-p2-8-0-arm7-api-16'
        elif abi == 'arm':
            treeherder_platform = 'android-hw-g5-7-0-arm7-api-16'
        elif abi == 'aarch64':
            treeherder_platform = 'android-hw-p2-8-0-android-aarch64'
        else:
            raise ValueError('Unsupported architecture "{}"'.format(abi))

        task_name = '{}: forPerformanceTest {}'.format(
            name_prefix, '(on 64-bit-device)' if force_run_on_64_bit_device else ''
        )

        apk_url = '{}/{}/artifacts/{}'.format(_DEFAULT_TASK_URL, signing_task_id,
                                                        DEFAULT_APK_ARTIFACT_LOCATION)
        command = [[
            "/builds/taskcluster/script.py",
            "bash",
            "./test-linux.sh",
            "--cfg=mozharness/configs/raptor/android_hw_config.py",
            "--test={}".format(test_name),
            "--app=fenix",
            "--binary=org.mozilla.fenix.performancetest",
            "--activity=org.mozilla.fenix.browser.BrowserPerformanceTestActivity",
            "--download-symbols=ondemand",
        ]]
        # Bug 1558456 - Stop tracking youtube-playback-test on motoG5 for >1080p cases
        if abi == 'arm' and test_name == 'raptor-youtube-playback':
            params_query = '&'.join(ARM_RAPTOR_URL_PARAMS)
            add_extra_params_option = "--test-url-params={}".format(params_query)
            command[0].append(add_extra_params_option)

        return self._craft_default_task_definition(
            worker_type=worker_type,
            provisioner_id='proj-autophone',
            dependencies=[signing_task_id],
            name=task_name,
            description=description,
            routes=['notify.email.perftest-alerts@mozilla.com.on-failed'],
            payload={
                "maxRunTime": 2700,
                "artifacts": [{
                    'path': '{}'.format(worker_path),
                    'expires': taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
                    'type': 'directory',
                    'name': 'public/{}/'.format(public_folder)
                } for worker_path, public_folder in (
                    ('artifacts/public', 'test'),
                    ('workspace/logs', 'logs'),
                    ('workspace/build/blobber_upload_dir', 'test_info'),
                )],
                "command": command,
                "env": {
                    "EXTRA_MOZHARNESS_CONFIG": json.dumps({
                        "test_packages_url": "{}/{}/artifacts/public/build/en-US/target.test_packages.json".format(_DEFAULT_TASK_URL, mozharness_task_id),
                        "installer_url": apk_url,
                    }),
                    "GECKO_HEAD_REPOSITORY": "https://hg.mozilla.org/mozilla-central",
                    "GECKO_HEAD_REV": gecko_revision,
                    "MOZ_AUTOMATION": "1",
                    "MOZ_HIDE_RESULTS_TABLE": "1",
                    "MOZ_NO_REMOTE": "1",
                    "MOZ_NODE_PATH": "/usr/local/bin/node",
                    "MOZHARNESS_CONFIG": "raptor/android_hw_config.py",
                    "MOZHARNESS_SCRIPT": "raptor_script.py",
                    "MOZHARNESS_URL": "{}/{}/artifacts/public/build/en-US/mozharness.zip".format(_DEFAULT_TASK_URL, mozharness_task_id),
                    "MOZILLA_BUILD_URL": apk_url,
                    "NEED_XVFB": "false",
                    "NO_FAIL_ON_TEST_ERRORS": "1",
                    "SCCACHE_DISABLE": "1",
                    "TASKCLUSTER_WORKER_TYPE": worker_type[len('gecko-'):],
                    "TRY_COMMIT_MSG": "",
                    "TRY_SELECTOR": "fuzzy",
                    "XPCOM_DEBUG_BREAK": "warn",
                },
                "mounts": [{
                    "content": {
                        "url": "https://hg.mozilla.org/mozilla-central/raw-file/{}/taskcluster/scripts/tester/test-linux.sh".format(gecko_revision),
                    },
                    "file": "test-linux.sh",
                }]
            },
            treeherder={
                'jobKind': 'test',
                'groupSymbol': 'Rap' if group_symbol is None else group_symbol,
                'machine': {
                    'platform': treeherder_platform,
                },
                'symbol': job_symbol,
                'tier': 2,
            },
            notify={
                'email': {
                    'link': {
                        'text': "Treeherder Job",
                        'href': "https://treeherder.mozilla.org/#/jobs?repo=fenix&revision={}".format(self.commit),
                    },
                    'subject': '[fenix] Raptor job "{}" failed'.format(task_name),
                    'content': "This calls for an action of the Performance team. Use the link to view it on Treeherder.",
                },
            },
        )


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
    return taskcluster.Index().findTask(raptor_index)['taskId']
