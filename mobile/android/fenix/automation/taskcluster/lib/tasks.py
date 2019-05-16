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

    def craft_assemble_release_task(self, architectures, track, is_staging, version_name):
        artifacts = {
            'public/target.{}.apk'.format(arch): {
                "type": 'file',
                "path": '/opt/fenix/app/build/outputs/apk/'
                        '{arch}/{track}/app-{arch}-{track}-unsigned.apk'.format(arch=arch, track=track),
                "expires": taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
            }
            for arch in architectures
        }

        def secret_index(name):
            if is_staging:
                return 'garbage/staging/project/mobile/fenix/{}'.format(name)
            elif track == 'nightly':
                # TODO: Move nightly secrets to "project/mobile/fenix/nightly/..."
                return 'project/mobile/fenix/{}'.format(name)
            else:
                return 'project/mobile/fenix/{}/{}'.format(track, name)

        sentry_secret = secret_index('sentry')
        leanplum_secret = secret_index('leanplum')
        adjust_secret = secret_index('adjust')

        pre_gradle_commands = (
            'python automation/taskcluster/helper/get-secret.py -s {} -k {} -f {}'.format(
                secret, key, target_file
            )
            for secret, key, target_file in (
                (sentry_secret, 'dsn', '.sentry_token'),
                (leanplum_secret, 'production', '.leanplum_token'),
                (adjust_secret, 'adjust', '.adjust_token'),
            )
        )

        capitalized_track = upper_case_first_letter(track)
        gradle_commands = (
            './gradlew --no-daemon -PcrashReports=true -Ptelemetry=true -PversionName={} clean test assemble{}'.format(
                version_name, capitalized_track),
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
            name='Build {} task'.format(capitalized_track),
            description='Build Fenix {} from source code'.format(capitalized_track),
            command=command,
            scopes=[
                "secrets:get:{}".format(secret) for secret in (sentry_secret, leanplum_secret, adjust_secret)
            ],
            artifacts=artifacts,
            routes=routes,
            treeherder={
                'jobKind': 'build',
                'machine': {
                    'platform': 'android-all',
                },
                'symbol': '{}-A'.format(track),
                'tier': 1,
            },
        )

    def craft_assemble_task(self, variant):
        return self._craft_clean_gradle_task(
            name='assemble: {}'.format(variant),
            description='Building and testing variant {}'.format(variant),
            gradle_task='assemble{}'.format(variant.capitalize()),
            artifacts=_craft_artifacts_from_variant(variant),
            treeherder={
                'groupSymbol': _craft_treeherder_group_symbol_from_variant(variant),
                'jobKind': 'build',
                'machine': {
                  'platform': _craft_treeherder_platform_from_variant(variant),
                },
                'symbol': 'A',
                'tier': 1,
            },
        )

    def craft_test_task(self, variant):
        return self._craft_clean_gradle_task(
            name='test: {}'.format(variant),
            description='Building and testing variant {}'.format(variant),
            gradle_task='test{}UnitTest'.format(variant.capitalize()),
            treeherder={
                'groupSymbol': _craft_treeherder_group_symbol_from_variant(variant),
                'jobKind': 'test',
                'machine': {
                  'platform': _craft_treeherder_platform_from_variant(variant),
                },
                'symbol': 'T',
                'tier': 1,
            },
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
        self, name, description, gradle_task, artifacts=None, routes=None, treeherder=None
    ):
        return self._craft_build_ish_task(
            name=name,
            description=description,
            command='./gradlew --no-daemon clean {}'.format(gradle_task),
            artifacts=artifacts,
            routes=routes,
            treeherder=treeherder,
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
        self, name, description, command, dependencies=None, artifacts=None, scopes=None,
        routes=None, treeherder=None
    ):
        dependencies = [] if dependencies is None else dependencies
        artifacts = {} if artifacts is None else artifacts
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes

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
            "maxRunTime": 7200,
            "image": "mozillamobile/fenix:1.3",
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
    ):
        dependencies = [] if dependencies is None else dependencies
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
        treeherder = {} if treeherder is None else treeherder

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
            "routes": routes + [
                "tc-treeherder.v2.fenix.{}".format(self.commit)
            ],
            "scopes": scopes,
            "payload": payload,
            "extra": {
                "treeherder": treeherder,
            },
            "metadata": {
                "name": "Fenix - {}".format(name),
                "description": description,
                "owner": self.owner,
                "source": self.source,
            },
        }

    def craft_raptor_signing_task(
        self, assemble_task_id, variant
    ):
        architecture, _ = get_architecture_and_build_type_from_variant(variant)
        routes = []
        if self.repo_url == _OFFICIAL_REPO_URL:
            routes = [
                'index.project.mobile.fenix.v2.branch.master.revision.{}.raptor.{}'.format(
                    self.commit, architecture
                ),
                'index.project.mobile.fenix.v2.branch.master.latest.raptor.{}'.format(
                    architecture
                ),
                'index.project.mobile.fenix.v2.branch.master.pushdate.{}.{}.{}.revision.{}.raptor.{}'.format(
                    self.date.year, self.date.month, self.date.day, self.commit, architecture
                ),
                'index.project.mobile.fenix.v2.branch.master.pushdate.{}.{}.{}.latest.raptor.{}'.format(
                    self.date.year, self.date.month, self.date.day, architecture
                ),
            ]

        return self._craft_signing_task(
            name='sign: {}'.format(variant),
            description='Dep-signing variant {}'.format(variant),
            signing_type="dep",
            assemble_task_id=assemble_task_id,
            apk_paths=["public/target.apk"],
            routes=routes,
            treeherder={
                'groupSymbol': _craft_treeherder_group_symbol_from_variant(variant),
                'jobKind': 'other',
                'machine': {
                    'platform': _craft_treeherder_platform_from_variant(variant),
                },
                'symbol': 'As',
                'tier': 1,
            },
        )

    def craft_release_signing_task(
        self, build_task_id, apk_paths, track, is_staging=False,
    ):
        capitalized_track = upper_case_first_letter(track)
        index_release = 'staging.{}'.format(track) if is_staging else track

        routes = [
            "index.project.mobile.fenix.v2.{}.{}.{}.{}.latest".format(
                index_release, self.date.year, self.date.month, self.date.day
            ),
            "index.project.mobile.fenix.v2.{}.{}.{}.{}.revision.{}".format(
                index_release, self.date.year, self.date.month, self.date.day, self.commit
            ),
            "index.project.mobile.fenix.v2.{}.latest".format(index_release),
        ]

        return self._craft_signing_task(
            name="Signing {} task".format(capitalized_track),
            description="Sign {} builds of Fenix".format(capitalized_track),
            signing_type="dep" if is_staging else track,
            assemble_task_id=build_task_id,
            apk_paths=apk_paths,
            routes=routes,
            treeherder={
                'jobKind': 'other',
                'machine': {
                  'platform': 'android-all',
                },
                'symbol': '{}-s'.format(track),
                'tier': 1,
            },
        )

    def craft_push_task(
        self, signing_task_id, apks, track, is_staging=False
    ):
        payload = {
            "commit": True,
            "google_play_track": track,
            "certificate_alias": 'fenix' if is_staging else 'fenix-{}'.format(track),
            "upstreamArtifacts": [
                {
                    "paths": apks,
                    "taskId": signing_task_id,
                    "taskType": "signing"
                }
            ]
        }

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
                'symbol': '{}-gp'.format(track),
                'tier': 1,
            },
        )

    def craft_raptor_tp6m_cold_task(self, for_suite):

        def craft_function(signing_task_id, mozharness_task_id, variant, gecko_revision, force_run_on_64_bit_device=False):
            return self._craft_raptor_task(
                signing_task_id,
                mozharness_task_id,
                variant,
                gecko_revision,
                name_prefix='raptor tp6m-cold-{}'.format(for_suite),
                description='Raptor tp6m cold on Fenix',
                test_name='raptor-tp6m-cold-{}'.format(for_suite),
                job_symbol='tp6m-c-{}'.format(for_suite),
                force_run_on_64_bit_device=force_run_on_64_bit_device,
            )
        return craft_function

    def _craft_raptor_task(
        self,
        signing_task_id,
        mozharness_task_id,
        variant,
        gecko_revision,
        name_prefix,
        description,
        test_name,
        job_symbol,
        group_symbol=None,
        extra_test_args=None,
        force_run_on_64_bit_device=False,
    ):
        extra_test_args = [] if extra_test_args is None else extra_test_args
        apk_location = '{}/{}/artifacts/{}'.format(
            _DEFAULT_TASK_URL, signing_task_id, DEFAULT_APK_ARTIFACT_LOCATION
        )
        architecture, _ = get_architecture_and_build_type_from_variant(variant)
        worker_type = 'gecko-t-ap-perf-p2' if force_run_on_64_bit_device or architecture == 'aarch64' else 'gecko-t-ap-perf-g5'

        if force_run_on_64_bit_device:
            treeherder_platform = 'android-hw-p2-8-0-arm7-api-16'
        elif architecture == 'arm':
            treeherder_platform = 'android-hw-g5-7-0-arm7-api-16'
        elif architecture == 'aarch64':
            treeherder_platform = 'android-hw-p2-8-0-aarch64'
        else:
            raise ValueError('Unsupported architecture "{}"'.format(architecture))

        task_name = '{}: {} {}'.format(
            name_prefix, variant, '(on 64-bit-device)' if force_run_on_64_bit_device else ''
        )

        return self._craft_default_task_definition(
            worker_type=worker_type,
            provisioner_id='proj-autophone',
            dependencies=[signing_task_id],
            name=task_name,
            description=description,
            payload={
                "artifacts": [{
                    'path': '/builds/worker/{}'.format(worker_path),
                    'expires': taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
                    'type': 'directory',
                    'name': 'public/{}/'.format(public_folder)
                } for worker_path, public_folder in (
                    ('artifacts', 'test'),
                    ('workspace/build/logs', 'logs'),
                    ('workspace/build/blobber_upload_dir', 'test_info'),
                )],
                "command": [
                    "./test-linux.sh",
                    '--installer-url={}'.format(apk_location),
                    "--test-packages-url={}/{}/artifacts/public/build/target.test_packages.json".format(_DEFAULT_TASK_URL, mozharness_task_id),
                    "--test={}".format(test_name),
                    "--app=fenix",
                    "--binary=org.mozilla.fenix.raptor",
                    "--activity=GeckoViewActivity",
                    "--download-symbols=ondemand"
                ] + extra_test_args,
                "env": {
                    "GECKO_HEAD_REPOSITORY": "https://hg.mozilla.org/mozilla-central",
                    "GECKO_HEAD_REV": gecko_revision,
                    "MOZ_AUTOMATION": "1",
                    "MOZ_HIDE_RESULTS_TABLE": "1",
                    "MOZ_NO_REMOTE": "1",
                    "MOZ_NODE_PATH": "/usr/local/bin/node",
                    "MOZHARNESS_CONFIG": "raptor/android_hw_config.py",
                    "MOZHARNESS_SCRIPT": "raptor_script.py",
                    "MOZHARNESS_URL": "{}/{}/artifacts/public/build/mozharness.zip".format(_DEFAULT_TASK_URL, mozharness_task_id),
                    "MOZILLA_BUILD_URL": apk_location,
                    "NEED_XVFB": "false",
                    "NO_FAIL_ON_TEST_ERRORS": "1",
                    "TASKCLUSTER_WORKER_TYPE": 'proj-autophone/{}'.format(worker_type),
                    "WORKING_DIR": "/builds/worker",
                    "WORKSPACE": "/builds/worker/workspace",
                    "XPCOM_DEBUG_BREAK": "warn",
                },
                "context": "https://hg.mozilla.org/mozilla-central/raw-file/{}/taskcluster/scripts/tester/test-linux.sh".format(gecko_revision)
            },
            treeherder={
                'jobKind': 'test',
                'groupSymbol': 'Rap' if group_symbol is None else group_symbol,
                'machine': {
                  'platform': treeherder_platform,
                },
                'symbol': job_symbol,
                'tier': 2,
            }
        )



def _craft_treeherder_platform_from_variant(variant):
    architecture, build_type = get_architecture_and_build_type_from_variant(variant)
    return 'android-{}-{}'.format(architecture, build_type)


def _craft_treeherder_group_symbol_from_variant(variant):
    _, build_type = get_architecture_and_build_type_from_variant(variant)
    return build_type


def _craft_artifacts_from_variant(variant):
    return {
        DEFAULT_APK_ARTIFACT_LOCATION: {
            'type': 'file',
            'path': _craft_apk_full_path_from_variant(variant),
            'expires': taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
        }
    }


def _craft_apk_full_path_from_variant(variant):
    architecture, build_type = get_architecture_and_build_type_from_variant(variant)
    postfix = '' if build_type == 'debug' else '-unsigned'
    return '/opt/fenix/app/build/outputs/apk/{architecture}/{build_type}/app-{architecture}-{build_type}{postfix}.apk'.format(     # noqa: E501
        architecture=architecture,
        build_type=build_type,
        postfix=postfix
    )


_SUPPORTED_ARCHITECTURES = ('aarch64', 'arm', 'x86')


def get_architecture_and_build_type_from_variant(variant):
    for supported_architecture in _SUPPORTED_ARCHITECTURES:
        if variant.startswith(supported_architecture):
            architecture = supported_architecture
            break
    else:
        raise ValueError(
            'Cannot identify architecture in "{}". '
            'Expected to find one of these supported ones: {}'.format(
                variant, _SUPPORTED_ARCHITECTURES
            )
        )

    build_type = variant[len(architecture):]
    build_type = lower_case_first_letter(build_type)
    return architecture, build_type


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


def fetch_mozharness_task_id(geckoview_nightly_version):
    nightly_build_id = geckoview_nightly_version.split('.')[-1]
    nightly_date = arrow.get(nightly_build_id, 'YYYYMMDDHHmmss')

    raptor_index = 'gecko.v2.mozilla-central.pushdate.{}.{:02}.{:02}.{}.firefox.linux64-debug'.format(
        nightly_date.year, nightly_date.month, nightly_date.day, nightly_build_id
    )
    return taskcluster.Index().findTask(raptor_index)['taskId']
