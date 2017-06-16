# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
These transformations take a task description and turn it into a TaskCluster
task definition (along with attributes, label, etc.).  The input to these
transformations is generic to any kind of task, but abstracts away some of the
complexities of worker implementations, scopes, and treeherder annotations.
"""

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import time
from copy import deepcopy

from taskgraph.util.treeherder import split_symbol
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import get_release_config
from voluptuous import Any, Required, Optional, Extra
from taskgraph import GECKO

from .gecko_v2_whitelist import JOB_NAME_WHITELIST, JOB_NAME_WHITELIST_ERROR


# shortcut for a string where task references are allowed
taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

# A task description is a general description of a TaskCluster task
task_description_schema = Schema({
    # the label for this task
    Required('label'): basestring,

    # description of the task (for metadata)
    Required('description'): basestring,

    # attributes for this task
    Optional('attributes'): {basestring: object},

    # dependencies of this task, keyed by name; these are passed through
    # verbatim and subject to the interpretation of the Task's get_dependencies
    # method.
    Optional('dependencies'): {basestring: object},

    # expiration and deadline times, relative to task creation, with units
    # (e.g., "14 days").  Defaults are set based on the project.
    Optional('expires-after'): basestring,
    Optional('deadline-after'): basestring,

    # custom routes for this task; the default treeherder routes will be added
    # automatically
    Optional('routes'): [basestring],

    # custom scopes for this task; any scopes required for the worker will be
    # added automatically
    Optional('scopes'): [basestring],

    # Tags
    Optional('tags'): {basestring: object},

    # custom "task.extra" content
    Optional('extra'): {basestring: object},

    # treeherder-related information; see
    # https://schemas.taskcluster.net/taskcluster-treeherder/v1/task-treeherder-config.json
    # If not specified, no treeherder extra information or routes will be
    # added to the task
    Optional('treeherder'): {
        # either a bare symbol, or "grp(sym)".
        'symbol': basestring,

        # the job kind
        'kind': Any('build', 'test', 'other'),

        # tier for this task
        'tier': int,

        # task platform, in the form platform/collection, used to set
        # treeherder.machine.platform and treeherder.collection or
        # treeherder.labels
        'platform': basestring,

        # treeherder environments (defaults to both staging and production)
        Required('environments', default=['production', 'staging']): ['production', 'staging'],
    },

    # information for indexing this build so its artifacts can be discovered;
    # if omitted, the build will not be indexed.
    Optional('index'): {
        # the name of the product this build produces
        'product': Any('firefox', 'mobile', 'static-analysis', 'devedition'),

        # the names to use for this job in the TaskCluster index
        'job-name': basestring,

        # Type of gecko v2 index to use
        'type': Any('generic', 'nightly', 'l10n', 'nightly-with-multi-l10n'),

        # The rank that the task will receive in the TaskCluster
        # index.  A newly completed task supercedes the currently
        # indexed task iff it has a higher rank.  If unspecified,
        # 'by-tier' behavior will be used.
        'rank': Any(
            # Rank is equal the timestamp of the build_date for tier-1
            # tasks, and zero for non-tier-1.  This sorts tier-{2,3}
            # builds below tier-1 in the index.
            'by-tier',

            # Rank is given as an integer constant (e.g. zero to make
            # sure a task is last in the index).
            int,

            # Rank is equal to the timestamp of the build_date.  This
            # option can be used to override the 'by-tier' behavior
            # for non-tier-1 tasks.
            'build_date',
        ),
    },

    # The `run_on_projects` attribute, defaulting to "all".  This dictates the
    # projects on which this task should be included in the target task set.
    # See the attributes documentation for details.
    Optional('run-on-projects'): [basestring],

    # If the task can be coalesced, this is the name used in the coalesce key
    # the project, etc. will be added automatically.  Note that try (level 1)
    # tasks are never coalesced
    Optional('coalesce-name'): basestring,

    # Optimizations to perform on this task during the optimization phase,
    # specified in order.  These optimizations are defined in
    # taskcluster/taskgraph/optimize.py.
    Optional('optimizations'): [Any(
        # search the index for the given index namespace, and replace this task if found
        ['index-search', basestring],
        # consult SETA and skip this task if it is low-value
        ['seta'],
        # skip this task if none of the given file patterns match
        ['skip-unless-changed', [basestring]],
    )],

    # the provisioner-id/worker-type for the task.  The following parameters will
    # be substituted in this string:
    #  {level} -- the scm level of this push
    'worker-type': basestring,

    # Whether the job should use sccache compiler caching.
    Required('needs-sccache', default=False): bool,

    # information specific to the worker implementation that will run this task
    'worker': Any({
        Required('implementation'): Any('docker-worker', 'docker-engine'),
        Required('os'): 'linux',

        # For tasks that will run in docker-worker or docker-engine, this is the
        # name of the docker image or in-tree docker image to run the task in.  If
        # in-tree, then a dependency will be created automatically.  This is
        # generally `desktop-test`, or an image that acts an awful lot like it.
        Required('docker-image'): Any(
            # a raw Docker image path (repo/image:tag)
            basestring,
            # an in-tree generated docker image (from `taskcluster/docker/<name>`)
            {'in-tree': basestring}
        ),

        # worker features that should be enabled
        Required('relengapi-proxy', default=False): bool,
        Required('chain-of-trust', default=False): bool,
        Required('taskcluster-proxy', default=False): bool,
        Required('allow-ptrace', default=False): bool,
        Required('loopback-video', default=False): bool,
        Required('loopback-audio', default=False): bool,
        Required('docker-in-docker', default=False): bool,  # (aka 'dind')

        # caches to set up for the task
        Optional('caches'): [{
            # only one type is supported by any of the workers right now
            'type': 'persistent',

            # name of the cache, allowing re-use by subsequent tasks naming the
            # same cache
            'name': basestring,

            # location in the task image where the cache will be mounted
            'mount-point': basestring,
        }],

        # artifacts to extract from the task image after completion
        Optional('artifacts'): [{
            # type of artifact -- simple file, or recursive directory
            'type': Any('file', 'directory'),

            # task image path from which to read artifact
            'path': basestring,

            # name of the produced artifact (root of the names for
            # type=directory)
            'name': basestring,
        }],

        # environment variables
        Required('env', default={}): {basestring: taskref_or_string},

        # the command to run; if not given, docker-worker will default to the
        # command in the docker image
        Optional('command'): [taskref_or_string],

        # the maximum time to run, in seconds
        Required('max-run-time'): int,

        # the exit status code that indicates the task should be retried
        Optional('retry-exit-status'): int,

    }, {
        Required('implementation'): 'generic-worker',
        Required('os'): Any('windows', 'macosx'),
        # see http://schemas.taskcluster.net/generic-worker/v1/payload.json
        # and https://docs.taskcluster.net/reference/workers/generic-worker/payload

        # command is a list of commands to run, sequentially
        # on Windows, each command is a string, on OS X and Linux, each command is
        # a string array
        Required('command'): Any(
            [taskref_or_string],   # Windows
            [[taskref_or_string]]  # Linux / OS X
        ),

        # artifacts to extract from the task image after completion; note that artifacts
        # for the generic worker cannot have names
        Optional('artifacts'): [{
            # type of artifact -- simple file, or recursive directory
            'type': Any('file', 'directory'),

            # filesystem path from which to read artifact
            'path': basestring,

            # if not specified, path is used for artifact name
            Optional('name'): basestring
        }],

        # Directories and/or files to be mounted.
        # The actual allowed combinations are stricter than the model below,
        # but this provides a simple starting point.
        # See https://docs.taskcluster.net/reference/workers/generic-worker/payload
        Optional('mounts'): [{
            # A unique name for the cache volume, implies writable cache directory
            # (otherwise mount is a read-only file or directory).
            Optional('cache-name'): basestring,
            # Optional content for pre-loading cache, or mandatory content for
            # read-only file or directory. Pre-loaded content can come from either
            # a task artifact or from a URL.
            Optional('content'): {

                # *** Either (artifact and task-id) or url must be specified. ***

                # Artifact name that contains the content.
                Optional('artifact'): basestring,
                # Task ID that has the artifact that contains the content.
                Optional('task-id'): taskref_or_string,
                # URL that supplies the content in response to an unauthenticated
                # GET request.
                Optional('url'): basestring
            },

            # *** Either file or directory must be specified. ***

            # If mounting a cache or read-only directory, the filesystem location of
            # the directory should be specified as a relative path to the task
            # directory here.
            Optional('directory'): basestring,
            # If mounting a file, specify the relative path within the task
            # directory to mount the file (the file will be read only).
            Optional('file'): basestring,
            # Required if and only if `content` is specified and mounting a
            # directory (not a file). This should be the archive format of the
            # content (either pre-loaded cache or read-only directory).
            Optional('format'): Any('rar', 'tar.bz2', 'tar.gz', 'zip')
        }],

        # environment variables
        Required('env', default={}): {basestring: taskref_or_string},

        # the maximum time to run, in seconds
        Required('max-run-time'): int,

        # os user groups for test task workers
        Optional('os-groups', default=[]): [basestring],

        # optional features
        Required('chain-of-trust', default=False): bool,
    }, {
        Required('implementation'): 'buildbot-bridge',

        # see
        # https://github.com/mozilla/buildbot-bridge/blob/master/bbb/schemas/payload.yml
        Required('buildername'): basestring,
        Required('sourcestamp'): {
            'branch': basestring,
            Optional('revision'): basestring,
            Optional('repository'): basestring,
            Optional('project'): basestring,
        },
        Required('properties'): {
            'product': basestring,
            Extra: basestring,  # additional properties are allowed
        },
    }, {
        Required('implementation'): 'native-engine',
        Required('os'): Any('macosx', 'linux'),

        # A link for an executable to download
        Optional('context'): basestring,

        # Tells the worker whether machine should reboot
        # after the task is finished.
        Optional('reboot'):
            Any('always', 'on-exception', 'on-failure'),

        # the command to run
        Optional('command'): [taskref_or_string],

        # environment variables
        Optional('env'): {basestring: taskref_or_string},

        # artifacts to extract from the task image after completion
        Optional('artifacts'): [{
            # type of artifact -- simple file, or recursive directory
            Required('type'): Any('file', 'directory'),

            # task image path from which to read artifact
            Required('path'): basestring,

            # name of the produced artifact (root of the names for
            # type=directory)
            Required('name'): basestring,
        }],
    }, {
        Required('implementation'): 'scriptworker-signing',

        # the maximum time to spend signing, in seconds
        Required('max-run-time', default=600): int,

        # list of artifact URLs for the artifacts that should be signed
        Required('upstream-artifacts'): [{
            # taskId of the task with the artifact
            Required('taskId'): taskref_or_string,

            # type of signing task (for CoT)
            Required('taskType'): basestring,

            # Paths to the artifacts to sign
            Required('paths'): [basestring],

            # Signing formats to use on each of the paths
            Required('formats'): [basestring],
        }],
    }, {
        Required('implementation'): 'beetmover',

        # the maximum time to spend signing, in seconds
        Required('max-run-time', default=600): int,

        # locale key, if this is a locale beetmover job
        Optional('locale'): basestring,

        # list of artifact URLs for the artifacts that should be beetmoved
        Required('upstream-artifacts'): [{
            # taskId of the task with the artifact
            Required('taskId'): taskref_or_string,

            # type of signing task (for CoT)
            Required('taskType'): basestring,

            # Paths to the artifacts to sign
            Required('paths'): [basestring],

            # locale is used to map upload path and allow for duplicate simple names
            Required('locale'): basestring,
        }],
    }, {
        Required('implementation'): 'balrog',

        # list of artifact URLs for the artifacts that should be beetmoved
        Required('upstream-artifacts'): [{
            # taskId of the task with the artifact
            Required('taskId'): taskref_or_string,

            # type of signing task (for CoT)
            Required('taskType'): basestring,

            # Paths to the artifacts to sign
            Required('paths'): [basestring],
        }],
    }, {
        Required('implementation'): 'push-apk-breakpoint',
        Required('payload'): object,

    }, {
        Required('implementation'): 'invalid',
        # an invalid task is one which should never actually be created; this is used in
        # release automation on branches where the task just doesn't make sense
        Extra: object,

    }, {
        Required('implementation'): 'push-apk',

        # list of artifact URLs for the artifacts that should be beetmoved
        Required('upstream-artifacts'): [{
            # taskId of the task with the artifact
            Required('taskId'): taskref_or_string,

            # type of signing task (for CoT)
            Required('taskType'): basestring,

            # Paths to the artifacts to sign
            Required('paths'): [basestring],
        }],

        # "Invalid" is a noop for try and other non-supported branches
        Required('google-play-track'): Any('production', 'beta', 'alpha', 'rollout', 'invalid'),
        Required('dry-run', default=True): bool,
        Optional('rollout-percentage'): int,
    }),
})

GROUP_NAMES = {
    'py': 'Python unit tests',
    'tc': 'Executed by TaskCluster',
    'tc-e10s': 'Executed by TaskCluster with e10s',
    'tc-Fxfn-l': 'Firefox functional tests (local) executed by TaskCluster',
    'tc-Fxfn-l-e10s': 'Firefox functional tests (local) executed by TaskCluster with e10s',
    'tc-Fxfn-r': 'Firefox functional tests (remote) executed by TaskCluster',
    'tc-Fxfn-r-e10s': 'Firefox functional tests (remote) executed by TaskCluster with e10s',
    'tc-M': 'Mochitests executed by TaskCluster',
    'tc-M-e10s': 'Mochitests executed by TaskCluster with e10s',
    'tc-M-V': 'Mochitests on Valgrind executed by TaskCluster',
    'tc-R': 'Reftests executed by TaskCluster',
    'tc-R-e10s': 'Reftests executed by TaskCluster with e10s',
    'tc-T': 'Talos performance tests executed by TaskCluster',
    'tc-T-e10s': 'Talos performance tests executed by TaskCluster with e10s',
    'tc-SY-e10s': 'Are we slim yet tests by TaskCluster with e10s',
    'tc-VP': 'VideoPuppeteer tests executed by TaskCluster',
    'tc-W': 'Web platform tests executed by TaskCluster',
    'tc-W-e10s': 'Web platform tests executed by TaskCluster with e10s',
    'tc-X': 'Xpcshell tests executed by TaskCluster',
    'tc-X-e10s': 'Xpcshell tests executed by TaskCluster with e10s',
    'tc-L10n': 'Localised Repacks executed by Taskcluster',
    'tc-L10n-Rpk': 'Localized Repackaged Repacks executed by Taskcluster',
    'tc-BM-L10n': 'Beetmover for locales executed by Taskcluster',
    'tc-Up': 'Balrog submission of updates, executed by Taskcluster',
    'tc-cs': 'Checksum signing executed by Taskcluster',
    'tc-BMcs': 'Beetmover checksums, executed by Taskcluster',
    'Aries': 'Aries Device Image',
    'Nexus 5-L': 'Nexus 5-L Device Image',
    'I': 'Docker Image Builds',
    'TL': 'Toolchain builds for Linux 64-bits',
    'TM': 'Toolchain builds for OSX',
    'TW32': 'Toolchain builds for Windows 32-bits',
    'TW64': 'Toolchain builds for Windows 64-bits',
    'SM-tc': 'Spidermonkey builds',
    'pub': 'APK publishing',
}
UNKNOWN_GROUP_NAME = "Treeherder group {} has no name; add it to " + __file__

V2_ROUTE_TEMPLATES = [
    "index.gecko.v2.{project}.latest.{product}.{job-name}",
    "index.gecko.v2.{project}.pushdate.{build_date_long}.{product}.{job-name}",
    "index.gecko.v2.{project}.revision.{head_rev}.{product}.{job-name}",
]

V2_NIGHTLY_TEMPLATES = [
    "index.gecko.v2.{project}.nightly.latest.{product}.{job-name}",
    "index.gecko.v2.{project}.nightly.{build_date}.revision.{head_rev}.{product}.{job-name}",
    "index.gecko.v2.{project}.nightly.{build_date}.latest.{product}.{job-name}",
    "index.gecko.v2.{project}.nightly.revision.{head_rev}.{product}.{job-name}",
]

V2_L10N_TEMPLATES = [
    "index.gecko.v2.{project}.revision.{head_rev}.{product}-l10n.{job-name}.{locale}",
    "index.gecko.v2.{project}.pushdate.{build_date_long}.{product}-l10n.{job-name}.{locale}",
    "index.gecko.v2.{project}.latest.{product}-l10n.{job-name}.{locale}",
]

# the roots of the treeherder routes, keyed by treeherder environment
TREEHERDER_ROUTE_ROOTS = {
    'production': 'tc-treeherder',
    'staging': 'tc-treeherder-stage',
}

COALESCE_KEY = 'builds.{project}.{name}'

DEFAULT_BRANCH_PRIORITY = 'low'
BRANCH_PRIORITIES = {
    'mozilla-release': 'highest',
    'comm-esr45': 'highest',
    'comm-esr52': 'highest',
    'mozilla-esr45': 'very-high',
    'mozilla-esr52': 'very-high',
    'mozilla-beta': 'high',
    'comm-beta': 'high',
    'mozilla-central': 'medium',
    'comm-central': 'medium',
    'mozilla-aurora': 'medium',
    'comm-aurora': 'medium',
    'autoland': 'low',
    'mozilla-inbound': 'low',
    'try': 'very-low',
    'try-comm-central': 'very-low',
    'alder': 'very-low',
    'ash': 'very-low',
    'birch': 'very-low',
    'cedar': 'very-low',
    'cypress': 'very-low',
    'date': 'very-low',
    'elm': 'very-low',
    'fig': 'very-low',
    'gum': 'very-low',
    'holly': 'very-low',
    'jamun': 'very-low',
    'larch': 'very-low',
    'maple': 'very-low',
    'oak': 'very-low',
    'pine': 'very-low',
    'graphics': 'very-low',
    'ux': 'very-low',
}

# define a collection of payload builders, depending on the worker implementation
payload_builders = {}


def payload_builder(name):
    def wrap(func):
        payload_builders[name] = func
        return func
    return wrap


# define a collection of index builders, depending on the type implementation
index_builders = {}


def index_builder(name):
    def wrap(func):
        index_builders[name] = func
        return func
    return wrap


@payload_builder('docker-worker')
def build_docker_worker_payload(config, task, task_def):
    worker = task['worker']

    image = worker['docker-image']
    if isinstance(image, dict):
        docker_image_task = 'build-docker-image-' + image['in-tree']
        task.setdefault('dependencies', {})['docker-image'] = docker_image_task
        image = {
            "path": "public/image.tar.zst",
            "taskId": {"task-reference": "<docker-image>"},
            "type": "task-image",
        }

    features = {}

    if worker.get('relengapi-proxy'):
        features['relengAPIProxy'] = True

    if worker.get('taskcluster-proxy'):
        features['taskclusterProxy'] = True

    if worker.get('allow-ptrace'):
        features['allowPtrace'] = True
        task_def['scopes'].append('docker-worker:feature:allowPtrace')

    if worker.get('chain-of-trust'):
        features['chainOfTrust'] = True

    if worker.get('docker-in-docker'):
        features['dind'] = True

    if task.get('needs-sccache'):
        features['taskclusterProxy'] = True
        task_def['scopes'].append(
            'assume:project:taskcluster:level-{level}-sccache-buckets'.format(
                level=config.params['level'])
        )
        worker['env']['USE_SCCACHE'] = '1'
    else:
        worker['env']['SCCACHE_DISABLE'] = '1'

    capabilities = {}

    for lo in 'audio', 'video':
        if worker.get('loopback-' + lo):
            capitalized = 'loopback' + lo.capitalize()
            devices = capabilities.setdefault('devices', {})
            devices[capitalized] = True
            task_def['scopes'].append('docker-worker:capability:device:' + capitalized)

    task_def['payload'] = payload = {
        'image': image,
        'env': worker['env'],
    }
    if 'command' in worker:
        payload['command'] = worker['command']

    if 'max-run-time' in worker:
        payload['maxRunTime'] = worker['max-run-time']

    if 'retry-exit-status' in worker:
        payload['onExitStatus'] = {'retry': [worker['retry-exit-status']]}

    if 'artifacts' in worker:
        artifacts = {}
        for artifact in worker['artifacts']:
            artifacts[artifact['name']] = {
                'path': artifact['path'],
                'type': artifact['type'],
                'expires': task_def['expires'],  # always expire with the task
            }
        payload['artifacts'] = artifacts

    if 'caches' in worker:
        caches = {}
        for cache in worker['caches']:
            caches[cache['name']] = cache['mount-point']
            task_def['scopes'].append('docker-worker:cache:' + cache['name'])
        payload['cache'] = caches

    if features:
        payload['features'] = features
    if capabilities:
        payload['capabilities'] = capabilities

    # coalesce / superseding
    if 'coalesce-name' in task and int(config.params['level']) > 1:
        key = COALESCE_KEY.format(
            project=config.params['project'],
            name=task['coalesce-name'])
        payload['supersederUrl'] = "https://coalesce.mozilla-releng.net/v1/list/" + key


@payload_builder('generic-worker')
def build_generic_worker_payload(config, task, task_def):
    worker = task['worker']

    artifacts = []

    for artifact in worker['artifacts']:
        a = {
            'path': artifact['path'],
            'type': artifact['type'],
            'expires': task_def['expires'],  # always expire with the task
        }
        if 'name' in artifact:
            a['name'] = artifact['name']
        artifacts.append(a)

    # Need to copy over mounts, but rename keys to respect naming convention
    #   * 'cache-name' -> 'cacheName'
    #   * 'task-id'    -> 'taskId'
    # All other key names are already suitable, and don't need renaming.
    mounts = deepcopy(worker.get('mounts', []))
    for mount in mounts:
        if 'cache-name' in mount:
            mount['cacheName'] = mount.pop('cache-name')
        if 'content' in mount:
            if 'task-id' in mount['content']:
                mount['content']['taskId'] = mount['content'].pop('task-id')

    task_def['payload'] = {
        'command': worker['command'],
        'artifacts': artifacts,
        'env': worker.get('env', {}),
        'mounts': mounts,
        'maxRunTime': worker['max-run-time'],
        'osGroups': worker.get('os-groups', []),
    }

    # needs-sccache is handled in mozharness_on_windows

    if 'retry-exit-status' in worker:
        raise Exception("retry-exit-status not supported in generic-worker")

    # currently only support one feature (chain of trust) but this will likely grow
    features = {}

    if worker.get('chain-of-trust'):
        features['chainOfTrust'] = True

    if features:
        task_def['payload']['features'] = features


@payload_builder('scriptworker-signing')
def build_scriptworker_signing_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'maxRunTime': worker['max-run-time'],
        'upstreamArtifacts':  worker['upstream-artifacts']
    }


@payload_builder('beetmover')
def build_beetmover_payload(config, task, task_def):
    worker = task['worker']
    release_config = get_release_config(config)

    task_def['payload'] = {
        'maxRunTime': worker['max-run-time'],
        'upload_date': config.params['build_date'],
        'upstreamArtifacts':  worker['upstream-artifacts']
    }
    if worker.get('locale'):
        task_def['payload']['locale'] = worker['locale']
    if release_config:
        task_def['payload'].update(release_config)


@payload_builder('balrog')
def build_balrog_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'upstreamArtifacts':  worker['upstream-artifacts']
    }


@payload_builder('push-apk')
def build_push_apk_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'dry_run': worker['dry-run'],
        'upstreamArtifacts':  worker['upstream-artifacts'],
        'google_play_track': worker['google-play-track'],
    }

    if worker.get('rollout-percentage', None):
        task_def['payload']['rollout_percentage'] = worker['rollout-percentage']


@payload_builder('push-apk-breakpoint')
def build_push_apk_breakpoint_payload(config, task, task_def):
    task_def['payload'] = task['worker']['payload']


@payload_builder('invalid')
def build_invalid_payload(config, task, task_def):
    task_def['payload'] = 'invalid task - should never be created'


@payload_builder('native-engine')
def build_macosx_engine_payload(config, task, task_def):
    worker = task['worker']
    artifacts = map(lambda artifact: {
        'name': artifact['name'],
        'path': artifact['path'],
        'type': artifact['type'],
        'expires': task_def['expires'],
    }, worker.get('artifacts', []))

    task_def['payload'] = {
        'context': worker['context'],
        'command': worker['command'],
        'env': worker['env'],
        'artifacts': artifacts,
    }
    if worker.get('reboot'):
        task_def['payload'] = worker['reboot']

    if task.get('needs-sccache'):
        raise Exception('needs-sccache not supported in native-engine')


@payload_builder('buildbot-bridge')
def build_buildbot_bridge_payload(config, task, task_def):
    del task['extra']['treeherder']
    del task['extra']['treeherderEnv']
    worker = task['worker']
    task_def['payload'] = {
        'buildername': worker['buildername'],
        'sourcestamp': worker['sourcestamp'],
        'properties': worker['properties'],
    }


transforms = TransformSequence()


@transforms.add
def validate(config, tasks):
    for task in tasks:
        yield validate_schema(
            task_description_schema, task,
            "In task {!r}:".format(task.get('label', '?no-label?')))


@index_builder('generic')
def add_generic_index_routes(config, task):
    index = task.get('index')
    routes = task.setdefault('routes', [])

    job_name = index['job-name']
    if job_name not in JOB_NAME_WHITELIST:
        raise Exception(JOB_NAME_WHITELIST_ERROR.format(job_name))

    subs = config.params.copy()
    subs['job-name'] = job_name
    subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                            time.gmtime(config.params['build_date']))
    subs['product'] = index['product']

    for tpl in V2_ROUTE_TEMPLATES:
        routes.append(tpl.format(**subs))

    return task


@index_builder('nightly')
def add_nightly_index_routes(config, task):
    index = task.get('index')
    routes = task.setdefault('routes', [])

    job_name = index['job-name']
    if job_name not in JOB_NAME_WHITELIST:
        raise Exception(JOB_NAME_WHITELIST_ERROR.format(job_name))

    subs = config.params.copy()
    subs['job-name'] = job_name
    subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                            time.gmtime(config.params['build_date']))
    subs['build_date'] = time.strftime("%Y.%m.%d",
                                       time.gmtime(config.params['build_date']))
    subs['product'] = index['product']

    for tpl in V2_NIGHTLY_TEMPLATES:
        routes.append(tpl.format(**subs))

    # Also add routes for en-US
    task = add_l10n_index_routes(config, task, force_locale="en-US")

    return task


@index_builder('nightly-with-multi-l10n')
def add_nightly_multi_index_routes(config, task):
    task = add_nightly_index_routes(config, task)
    task = add_l10n_index_routes(config, task, force_locale="multi")
    return task


@index_builder('l10n')
def add_l10n_index_routes(config, task, force_locale=None):
    index = task.get('index')
    routes = task.setdefault('routes', [])

    job_name = index['job-name']
    if job_name not in JOB_NAME_WHITELIST:
        raise Exception(JOB_NAME_WHITELIST_ERROR.format(job_name))

    subs = config.params.copy()
    subs['job-name'] = job_name
    subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                            time.gmtime(config.params['build_date']))
    subs['product'] = index['product']

    locales = task['attributes'].get('chunk_locales',
                                     task['attributes'].get('all_locales'))

    if force_locale:
        # Used for en-US and multi-locale
        locales = [force_locale]

    if not locales:
        raise Exception("Error: Unable to use l10n index for tasks without locales")

    # If there are too many locales, we can't write a route for all of them
    # See Bug 1323792
    if len(locales) > 18:  # 18 * 3 = 54, max routes = 64
        return task

    for locale in locales:
        for tpl in V2_L10N_TEMPLATES:
            routes.append(tpl.format(locale=locale, **subs))

    return task


@transforms.add
def add_index_routes(config, tasks):
    for task in tasks:
        index = task.get('index')

        if not index:
            yield task
            continue

        index_type = index.get('type', 'generic')
        task = index_builders[index_type](config, task)

        # The default behavior is to rank tasks according to their tier
        extra_index = task.setdefault('extra', {}).setdefault('index', {})
        rank = index.get('rank', 'by-tier')

        if rank == 'by-tier':
            # rank is zero for non-tier-1 tasks and based on pushid for others;
            # this sorts tier-{2,3} builds below tier-1 in the index
            tier = task.get('treeherder', {}).get('tier', 3)
            extra_index['rank'] = 0 if tier > 1 else int(config.params['build_date'])
        elif rank == 'build_date':
            extra_index['rank'] = int(config.params['build_date'])
        else:
            extra_index['rank'] = rank

        del task['index']
        yield task


@transforms.add
def build_task(config, tasks):
    for task in tasks:
        worker_type = task['worker-type'].format(level=str(config.params['level']))
        provisioner_id, worker_type = worker_type.split('/', 1)

        routes = task.get('routes', [])
        scopes = task.get('scopes', [])

        # set up extra
        extra = task.get('extra', {})
        task_th = task.get('treeherder')
        if task_th:
            extra['treeherderEnv'] = task_th['environments']

            treeherder = extra.setdefault('treeherder', {})

            machine_platform, collection = task_th['platform'].split('/', 1)
            treeherder['machine'] = {'platform': machine_platform}
            treeherder['collection'] = {collection: True}

            groupSymbol, symbol = split_symbol(task_th['symbol'])
            if groupSymbol != '?':
                treeherder['groupSymbol'] = groupSymbol
                if groupSymbol not in GROUP_NAMES:
                    raise Exception(UNKNOWN_GROUP_NAME.format(groupSymbol))
                treeherder['groupName'] = GROUP_NAMES[groupSymbol]
            treeherder['symbol'] = symbol
            treeherder['jobKind'] = task_th['kind']
            treeherder['tier'] = task_th['tier']

            routes.extend([
                '{}.v2.{}.{}.{}'.format(TREEHERDER_ROUTE_ROOTS[env],
                                        config.params['project'],
                                        config.params['head_rev'],
                                        config.params['pushlog_id'])
                for env in task_th['environments']
            ])

        if 'expires-after' not in task:
            task['expires-after'] = '28 days' if config.params['project'] == 'try' else '1 year'

        if 'deadline-after' not in task:
            task['deadline-after'] = '1 day'

        if 'coalesce-name' in task and int(config.params['level']) > 1:
            key = COALESCE_KEY.format(
                project=config.params['project'],
                name=task['coalesce-name'])
            routes.append('coalesce.v1.' + key)

        if 'priority' not in task:
            task['priority'] = BRANCH_PRIORITIES.get(
                config.params['project'],
                DEFAULT_BRANCH_PRIORITY)

        tags = task.get('tags', {})
        tags.update({'createdForUser': config.params['owner']})

        task_def = {
            'provisionerId': provisioner_id,
            'workerType': worker_type,
            'routes': routes,
            'created': {'relative-datestamp': '0 seconds'},
            'deadline': {'relative-datestamp': task['deadline-after']},
            'expires': {'relative-datestamp': task['expires-after']},
            'scopes': scopes,
            'metadata': {
                'description': task['description'],
                'name': task['label'],
                'owner': config.params['owner'],
                'source': '{}/file/{}/{}'.format(
                    config.params['head_repository'],
                    config.params['head_rev'],
                    config.path),
            },
            'extra': extra,
            'tags': tags,
            'priority': task['priority'],
        }

        if task_th:
            # link back to treeherder in description
            th_push_link = 'https://treeherder.mozilla.org/#/jobs?repo={}&revision={}'.format(
                config.params['project'], config.params['head_rev'])
            task_def['metadata']['description'] += ' ([Treeherder push]({}))'.format(
                th_push_link)

        # add the payload and adjust anything else as required (e.g., scopes)
        payload_builders[task['worker']['implementation']](config, task, task_def)

        attributes = task.get('attributes', {})
        attributes['run_on_projects'] = task.get('run-on-projects', ['all'])

        # Set MOZ_AUTOMATION on all jobs.
        if task['worker']['implementation'] in (
            'generic-worker',
            'docker-engine',
            'native-engine',
            'docker-worker',
        ):
            payload = task_def.get('payload')
            if payload:
                env = payload.setdefault('env', {})
                env['MOZ_AUTOMATION'] = '1'

        yield {
            'label': task['label'],
            'task': task_def,
            'dependencies': task.get('dependencies', {}),
            'attributes': attributes,
            'optimizations': task.get('optimizations', []),
        }


# Check that the v2 route templates match those used by Mozharness.  This can
# go away once Mozharness builds are no longer performed in Buildbot, and the
# Mozharness code referencing routes.json is deleted.
def check_v2_routes():
    with open(os.path.join(GECKO, "testing/mozharness/configs/routes.json"), "rb") as f:
        routes_json = json.load(f)

    for key in ('routes', 'nightly', 'l10n'):
        if key == 'routes':
            tc_template = V2_ROUTE_TEMPLATES
        elif key == 'nightly':
            tc_template = V2_NIGHTLY_TEMPLATES
        elif key == 'l10n':
            tc_template = V2_L10N_TEMPLATES

        routes = routes_json[key]

        # we use different variables than mozharness
        for mh, tg in [
                ('{index}', 'index'),
                ('{build_product}', '{product}'),
                ('{build_name}-{build_type}', '{job-name}'),
                ('{year}.{month}.{day}.{pushdate}', '{build_date_long}'),
                ('{year}.{month}.{day}', '{build_date}')]:
            routes = [r.replace(mh, tg) for r in routes]

        if sorted(routes) != sorted(tc_template):
            raise Exception("V2 TEMPLATES do not match Mozharness's routes.json: "
                            "(tc):%s vs (mh):%s" % (tc_template, routes))


check_v2_routes()
