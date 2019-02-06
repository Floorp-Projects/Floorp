# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import datetime
import json
import taskcluster

DEFAULT_EXPIRES_IN = '1 year'

DOCKER_IMAGE_ROUTE_BASE_PATTERN = 'index.project.mobile.android-components.cache.level-{trust_level}.docker-images.v1.{image_name}'
DOCKER_IMAGE_ROUTE_HASH_PATTERN = DOCKER_IMAGE_ROUTE_BASE_PATTERN + '.hash.{hash}'
DOCKER_IMAGE_ROUTE_PUSH_DATE_PATTERN = DOCKER_IMAGE_ROUTE_BASE_PATTERN + '.pushdate.{year}.{month}.{day}.{build_id}'
DOCKER_IMAGE_ROUTE_LATEST_PATTERN = DOCKER_IMAGE_ROUTE_BASE_PATTERN + '.latest'

TASKCLUSTER_DATE_ISO_FORMAT = '%Y-%m-%dT%H:%M:%S.%fZ'


class TaskBuilder(object):
    def __init__(
        self,
        beetmover_worker_type,
        branch,
        commit,
        owner,
        push_date_time,
        repo_url,
        scheduler_id,
        source,
        task_id,
        tasks_priority,
        trust_level,
    ):
        self.beetmover_worker_type = beetmover_worker_type
        self.branch = branch
        self.commit = commit
        self.owner = owner
        self.push_date_time = _parse_push_date_time(push_date_time)
        self.repo_url = repo_url
        self.scheduler_id = scheduler_id
        self.source = source
        self.task_id = task_id
        self.tasks_priority = tasks_priority
        self.trust_level = trust_level

    def craft_build_task(self, module_name, gradle_tasks, build_docker_image_task_id, subtitle='', run_coverage=False, is_snapshot=False, artifact_info=None):
        artifacts = {} if artifact_info is None else {
            artifact_info['artifact']: {
                'type': 'file',
                'expires': taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
                'path': artifact_info['path']
            }
        }

        scopes = [
            "secrets:get:project/mobile/android-components/public-tokens"
        ] if run_coverage else []

        snapshot_flag = '-Psnapshot ' if is_snapshot else ''
        coverage_flag = '-Pcoverage ' if run_coverage else ''
        gradle_command = (
            './gradlew --no-daemon clean ' + coverage_flag + snapshot_flag + gradle_tasks
        )

        post_gradle_command = 'automation/taskcluster/action/upload_coverage_report.sh' if run_coverage else ''

        command = ' && '.join(cmd for cmd in (gradle_command, post_gradle_command) if cmd)

        features = {}
        if artifact_info is not None:
            features['chainOfTrust'] = True
        elif any(scope.startswith('secrets:') for scope in scopes):
            features['taskclusterProxy'] = True

        return self._craft_build_ish_task(
            name='Android Components - Module {} {}'.format(module_name, subtitle),
            description='Execure Gradle tasks for module {}'.format(module_name),
            command=command,
            features=features,
            scopes=scopes,
            artifacts=artifacts,
            build_docker_image_task_id=build_docker_image_task_id,
        )

    def craft_wait_on_builds_task(self, dependencies, build_docker_image_task_id):
        return self._craft_build_ish_task(
            name='Android Components - Wait on all builds to be completed',
            description='Dummy tasks that ensures all builds are correctly done before publishing them',
            dependencies=dependencies,
            command="exit 0",
            build_docker_image_task_id=build_docker_image_task_id,
        )

    def craft_detekt_task(self, build_docker_image_task_id):
        return self._craft_build_ish_task(
            name='Android Components - detekt',
            description='Running detekt over all modules',
            command='./gradlew --no-daemon clean detekt',
            build_docker_image_task_id=build_docker_image_task_id,
        )

    def craft_ktlint_task(self, build_docker_image_task_id):
        return self._craft_build_ish_task(
            name='Android Components - ktlint',
            description='Running ktlint over all modules',
            command='./gradlew --no-daemon clean ktlint',
            build_docker_image_task_id=build_docker_image_task_id,
        )

    def craft_compare_locales_task(self, build_docker_image_task_id):
        return self._craft_build_ish_task(
            name='Android Components - compare-locales',
            description='Validate strings.xml with compare-locales',
            command='pip install "compare-locales>=5.0.2,<6.0" && compare-locales --validate l10n.toml .',
            build_docker_image_task_id=build_docker_image_task_id,
        )

    def craft_beetmover_task(
        self, build_task_id, wait_on_builds_task_id, version, artifact, artifact_name, is_snapshot, is_staging
    ):
        if is_snapshot:
            if is_staging:
                bucket_name = 'maven-snapshot-staging'
                bucket_public_url = 'https://maven-snapshots.stage.mozaws.net/'
            else:
                bucket_name = 'maven-snapshot-production'
                bucket_public_url = 'https://snapshots.maven.mozilla.org/'
        else:
            if is_staging:
                bucket_name = 'maven-staging'
                bucket_public_url = 'https://maven-default.stage.mozaws.net/'
            else:
                bucket_name = 'maven-production'
                bucket_public_url = 'https://maven.mozilla.org/'

        version = version.lstrip('v')
        payload = {
            "maxRunTime": 600,
            "upstreamArtifacts": [{
                'paths': [artifact],
                'taskId': build_task_id,
                'taskType': 'build',
                'zipExtract': True,
            }],
            "releaseProperties": {
                "appName": "snapshot_components" if is_snapshot else "components",
            },
            "version": "{}-SNAPSHOT".format(version) if is_snapshot else version,
            "artifact_id": artifact_name
        }

        return self._craft_default_task_definition(
            self.beetmover_worker_type,
            'scriptworker-prov-v1',
            dependencies=[build_task_id, wait_on_builds_task_id],
            routes=[],
            scopes=[
                "project:mobile:android-components:releng:beetmover:bucket:{}".format(bucket_name),
                "project:mobile:android-components:releng:beetmover:action:push-to-maven",
            ],
            name="Android Components - Publish Module :{} via beetmover".format(artifact_name),
            description="Publish release module {} to {}".format(artifact_name, bucket_public_url),
            payload=payload
        )

    def craft_docker_image_task(self, image_name, folder_hash):
        routes = [
            DOCKER_IMAGE_ROUTE_HASH_PATTERN.format(
                trust_level=self.trust_level, image_name=image_name, hash=folder_hash
            ),
            DOCKER_IMAGE_ROUTE_LATEST_PATTERN.format(
                trust_level=self.trust_level, image_name=image_name
            ),
            DOCKER_IMAGE_ROUTE_PUSH_DATE_PATTERN.format(
                trust_level=self.trust_level, image_name=image_name,
                year=self.push_date_time.year,
                month=self.push_date_time.month,
                day=self.push_date_time.day,
                build_id=self.push_date_time.strftime('%Y%m%d%H%M%S'),
            ),
        ]

        return self._craft_default_task_definition(
            'mobile-{}-images'.format(self.trust_level),
            'aws-provisioner-v1',
            dependencies=[],
            routes=routes,
            scopes=[],
            name='Android Components - Build docker image "{}"'.format(image_name),
            description='',
            payload={
                # This sha points to the tag docker:1.6-dind. 1.6 is what's run on taskcluter.
                'image': 'docker@sha256:db593745f76eeb9a58b8679e85bf8651b8efc0e9ff1132fb75bc613e24e16bfc',
                'maxRunTime': 7200,     # The build docker image takes more than an hour to complete
                'command': [
                    '/bin/sh',
                    '--login',
                    '-xec',
                    (
                        # Needed to install zstd
                        "sed -i -e 's/v[[:digit:]]\.[[:digit:]]/edge/g' /etc/apk/repositories"
                        " && apk add --update zstd git"
                        " && git clone {repo_url} repo"
                        " && cd repo"
                        " && git config advice.detachedHead false"
                        " && git checkout {commit}"
                        " && cd automation/docker/{image_folder}"
                        " && docker build -t taskcluster-built ."
                        " && docker save taskcluster-built | zstd > /image.tar.zst".format(
                            repo_url=self.repo_url,
                            commit=self.commit,
                            image_folder=image_name
                        )
                    )
                ],
                'features': {
                    'chainOfTrust': True,   # Needed to ensure the Docker image is trusted
                    'dind': True,
                },
                'artifacts': {
                    'public/image.tar.zst': {
                        'type': 'file',
                        'expires': taskcluster.stringDate(taskcluster.fromNow(DEFAULT_EXPIRES_IN)),
                        'path': '/image.tar.zst',
                  }
                }
            },
        )

    def _craft_build_ish_task(
        self, name, description, command, build_docker_image_task_id, dependencies=None,
        artifacts=None, scopes=None, routes=None, features=None,
    ):
        dependencies = [] if dependencies is None else list(dependencies)
        dependencies.append(build_docker_image_task_id)

        artifacts = {} if artifacts is None else artifacts
        scopes = [] if scopes is None else scopes
        routes = [] if routes is None else routes
        features = {} if features is None else features

        checkout_command = (
            "git fetch {} {} --tags && "
            "git config advice.detachedHead false && "
            "git checkout {}".format(
                self.repo_url, self.branch, self.commit
            )
        )

        command = '{} && {}'.format(checkout_command, command)

        payload = {
            'features': features,
            'maxRunTime': 7200,
            'image': {
                'path': 'public/image.tar.zst',
                'taskId': build_docker_image_task_id,
                'type': 'task-image'
            },
            'command': [
                '/bin/bash',
                '--login',
                '-cx',
                command
            ],
            'artifacts': artifacts,
        }

        extra = {
            'chainOfTrust': {
                'inputs': {
                    'docker-image': build_docker_image_task_id,
                }
            }
        }

        return self._craft_default_task_definition(
            'mobile-{}-b-andrcmp'.format(self.trust_level),
            'aws-provisioner-v1',
            dependencies,
            routes,
            scopes,
            name,
            description,
            payload,
            extra,
        )

    def _craft_default_task_definition(
        self, worker_type, provisioner_id, dependencies, routes, scopes, name, description, payload, extra=None
    ):
        created = datetime.datetime.now()
        deadline = taskcluster.fromNow('1 day')
        expires = taskcluster.fromNow(DEFAULT_EXPIRES_IN)

        task_definition = {
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

        if extra:
            task_definition['extra'] = extra

        return task_definition


def _parse_push_date_time(push_date_time):
    try:
        # GitHub usually provides ISO 8601 format, but without milliseconds
        return datetime.datetime.strptime(push_date_time, '%Y-%m-%dT%H:%M:%SZ')
    except ValueError:
        try:
            # However GitHub PushEvents don't comply to the ISO format :(
            return datetime.datetime.utcfromtimestamp(int(push_date_time))
        except ValueError:
            # Taskcluster (for instance in cron tasks) provides ISO 8601 format too,
            # *with* milliseconds.
            return datetime.datetime.strptime(push_date_time, TASKCLUSTER_DATE_ISO_FORMAT)


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


def craft_new_task_id():
    slug_id = taskcluster.slugId()

    # slugId() returns bytes in Python 3. This is not expected by the rest of the API.
    # That's why we have to convert it to str.
    if not isinstance(slug_id, str):
        return slug_id.decode('utf-8')

    return slug_id
