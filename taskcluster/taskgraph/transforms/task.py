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
import time

from taskgraph.util.treeherder import split_symbol
from taskgraph.transforms.base import (
    validate_schema,
    TransformSequence
)
from voluptuous import Schema, Any, Required, Optional, Extra

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
        'product': Any('firefox', 'mobile'),

        # the names to use for this job in the TaskCluster index
        'job-name': Any(
            # Assuming the job is named "normally", this is the v2 job name,
            # and the v1 and buildbot routes will be determined appropriately.
            basestring,

            # otherwise, give separate names for each of the legacy index
            # routes; if a name is omitted, no corresponding route will be
            # created.
            {
                # the name as it appears in buildbot routes
                Optional('buildbot'): basestring,
                Required('gecko-v2'): basestring,
            }
        ),

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

    # the provisioner-id/worker-type for the task.  The following parameters will
    # be substituted in this string:
    #  {level} -- the scm level of this push
    'worker-type': basestring,

    # information specific to the worker implementation that will run this task
    'worker': Any({
        Required('implementation'): Any('docker-worker', 'docker-engine'),

        # For tasks that will run in docker-worker or docker-engine, this is the
        # name of the docker image or in-tree docker image to run the task in.  If
        # in-tree, then a dependency will be created automatically.  This is
        # generally `desktop-test`, or an image that acts an awful lot like it.
        Required('docker-image'): Any(
            # a raw Docker image path (repo/image:tag)
            basestring,
            # an in-tree generated docker image (from `testing/docker/<name>`)
            {'in-tree': basestring}
        ),

        # worker features that should be enabled
        Required('relengapi-proxy', default=False): bool,
        Required('chain-of-trust', default=False): bool,
        Required('taskcluster-proxy', default=False): bool,
        Required('allow-ptrace', default=False): bool,
        Required('loopback-video', default=False): bool,
        Required('loopback-audio', default=False): bool,

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

        # the command to run
        'command': [taskref_or_string],

        # the maximum time to run, in seconds
        'max-run-time': int,

        # the exit status code that indicates the task should be retried
        Optional('retry-exit-status'): int,

    }, {
        Required('implementation'): 'generic-worker',

        # command is a list of commands to run, sequentially
        'command': [taskref_or_string],

        # artifacts to extract from the task image after completion; note that artifacts
        # for the generic worker cannot have names
        Optional('artifacts'): [{
            # type of artifact -- simple file, or recursive directory
            'type': Any('file', 'directory'),

            # task image path from which to read artifact
            'path': basestring,
        }],

        # directories and/or files to be mounted
        Optional('mounts'): [{
            # a unique name for the cache volume
            'cache-name': basestring,

            # task image path for the cache
            'path': basestring,
        }],

        # environment variables
        Required('env', default={}): {basestring: taskref_or_string},

        # the maximum time to run, in seconds
        'max-run-time': int,

        # os user groups for test task workers
        Optional('os-groups', default=[]): [basestring],
    }, {
        Required('implementation'): 'buildbot-bridge',

        # see
        # https://github.com/mozilla/buildbot-bridge/blob/master/bbb/schemas/payload.yml
        'buildername': basestring,
        'sourcestamp': {
            'branch': basestring,
            Optional('revision'): basestring,
            Optional('repository'): basestring,
            Optional('project'): basestring,
        },
        'properties': {
            'product': basestring,
            Extra: basestring,  # additional properties are allowed
        },
    }, {
        'implementation': 'macosx-engine',

        # A link for an executable to download
        Optional('link'): basestring,

        # the command to run
        Required('command'): [taskref_or_string],

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
    }),

    # The "when" section contains descriptions of the circumstances
    # under which this task can be "optimized", that is, left out of the
    # task graph because it is unnecessary.
    Optional('when'): Any({
        # This task only needs to be run if a file matching one of the given
        # patterns has changed in the push.  The patterns use the mozpack
        # match function (python/mozbuild/mozpack/path.py).
        Optional('files-changed'): [basestring],
    }),
})

GROUP_NAMES = {
    'tc': 'Executed by TaskCluster',
    'tc-e10s': 'Executed by TaskCluster with e10s',
    'tc-Fxfn-l': 'Firefox functional tests (local) executed by TaskCluster',
    'tc-Fxfn-l-e10s': 'Firefox functional tests (local) executed by TaskCluster with e10s',
    'tc-Fxfn-r': 'Firefox functional tests (remote) executed by TaskCluster',
    'tc-Fxfn-r-e10s': 'Firefox functional tests (remote) executed by TaskCluster with e10s',
    'tc-M': 'Mochitests executed by TaskCluster',
    'tc-M-e10s': 'Mochitests executed by TaskCluster with e10s',
    'tc-R': 'Reftests executed by TaskCluster',
    'tc-R-e10s': 'Reftests executed by TaskCluster with e10s',
    'tc-VP': 'VideoPuppeteer tests executed by TaskCluster',
    'tc-W': 'Web platform tests executed by TaskCluster',
    'tc-W-e10s': 'Web platform tests executed by TaskCluster with e10s',
    'tc-X': 'Xpcshell tests executed by TaskCluster',
    'tc-X-e10s': 'Xpcshell tests executed by TaskCluster with e10s',
    'Aries': 'Aries Device Image',
    'Nexus 5-L': 'Nexus 5-L Device Image',
    'Cc': 'Toolchain builds',
    'SM-tc': 'Spidermonkey builds',
}
UNKNOWN_GROUP_NAME = "Treeherder group {} has no name; add it to " + __file__

BUILDBOT_ROUTE_TEMPLATES = [
    "index.buildbot.branches.{project}.{job-name-buildbot}",
    "index.buildbot.revisions.{head_rev}.{project}.{job-name-buildbot}",
]

V2_ROUTE_TEMPLATES = [
    "index.gecko.v2.{project}.latest.{product}.{job-name-gecko-v2}",
    "index.gecko.v2.{project}.pushdate.{build_date_long}.{product}.{job-name-gecko-v2}",
    "index.gecko.v2.{project}.revision.{head_rev}.{product}.{job-name-gecko-v2}",
]

# the roots of the treeherder routes, keyed by treeherder environment
TREEHERDER_ROUTE_ROOTS = {
    'production': 'tc-treeherder',
    'staging': 'tc-treeherder-stage',
}

COALESCE_KEY = 'builds.{project}.{name}'

# define a collection of payload builders, depending on the worker implementation
payload_builders = {}


def payload_builder(name):
    def wrap(func):
        payload_builders[name] = func
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

    capabilities = {}

    for lo in 'audio', 'video':
        if worker.get('loopback-' + lo):
            capitalized = 'loopback' + lo.capitalize()
            devices = capabilities.setdefault('devices', {})
            devices[capitalized] = True
            task_def['scopes'].append('docker-worker:capability:device:' + capitalized)

    task_def['payload'] = payload = {
        'command': worker['command'],
        'image': image,
        'env': worker['env'],
    }

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
        artifacts.append({
            'path': artifact['path'],
            'type': artifact['type'],
            'expires': task_def['expires'],  # always expire with the task
        })

    mounts = []

    for mount in worker.get('mounts', []):
        mounts.append({
            'cacheName': mount['cache-name'],
            'directory': mount['path']
        })

    task_def['payload'] = {
        'command': worker['command'],
        'artifacts': artifacts,
        'env': worker.get('env', {}),
        'mounts': mounts,
        'maxRunTime': worker['max-run-time'],
        'osGroups': worker.get('os-groups', []),
    }

    if 'retry-exit-status' in worker:
        raise Exception("retry-exit-status not supported in generic-worker")


@payload_builder('macosx-engine')
def build_macosx_engine_payload(config, task, task_def):
    worker = task['worker']
    artifacts = map(lambda artifact: {
        'name': artifact['name'],
        'path': artifact['path'],
        'type': artifact['type'],
        'expires': task_def['expires'],
    }, worker['artifacts'])

    task_def['payload'] = {
        'link': worker['link'],
        'command': worker['command'],
        'env': worker['env'],
        'artifacts': artifacts,
    }


@payload_builder('buildbot-bridge')
def build_buildbot_bridge_payload(config, task, task_def):
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


@transforms.add
def add_index_routes(config, tasks):
    for task in tasks:
        index = task.get('index')
        routes = task.setdefault('routes', [])

        if not index:
            yield task
            continue

        job_name = index['job-name']
        # unpack the v2 name to v1 and buildbot names
        if isinstance(job_name, basestring):
            base_name, type_name = job_name.rsplit('-', 1)
            job_name = {
                'buildbot': base_name,
                'gecko-v2': '{}-{}'.format(base_name, type_name),
            }

        if job_name['gecko-v2'] not in JOB_NAME_WHITELIST:
            raise Exception(JOB_NAME_WHITELIST_ERROR.format(job_name['gecko-v2']))

        subs = config.params.copy()
        for n in job_name:
            subs['job-name-' + n] = job_name[n]
        subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                                time.gmtime(config.params['build_date']))
        subs['product'] = index['product']

        if 'buildbot' in job_name:
            for tpl in BUILDBOT_ROUTE_TEMPLATES:
                routes.append(tpl.format(**subs))
        if 'gecko-v2' in job_name:
            for tpl in V2_ROUTE_TEMPLATES:
                routes.append(tpl.format(**subs))

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
            'tags': {'createdForUser': config.params['owner']},
        }

        # add the payload and adjust anything else as required (e.g., scopes)
        payload_builders[task['worker']['implementation']](config, task, task_def)

        attributes = task.get('attributes', {})
        attributes['run_on_projects'] = task.get('run-on-projects', ['all'])

        yield {
            'label': task['label'],
            'task': task_def,
            'dependencies': task.get('dependencies', {}),
            'attributes': attributes,
            'when': task.get('when', {}),
        }


# Check that the v2 route templates match those used by Mozharness.  This can
# go away once Mozharness builds are no longer performed in Buildbot, and the
# Mozharness code referencing routes.json is deleted.
def check_v2_routes():
    with open("testing/mozharness/configs/routes.json", "rb") as f:
        routes_json = json.load(f)

    # we only deal with the 'routes' key here
    routes = routes_json['routes']

    # we use different variables than mozharness
    for mh, tg in [
            ('{index}', 'index'),
            ('{build_product}', '{product}'),
            ('{build_name}-{build_type}', '{job-name-gecko-v2}'),
            ('{year}.{month}.{day}.{pushdate}', '{build_date_long}')]:
        routes = [r.replace(mh, tg) for r in routes]

    if sorted(routes) != sorted(V2_ROUTE_TEMPLATES):
        raise Exception("V2_ROUTE_TEMPLATES does not match Mozharness's routes.json: "
                        "%s vs %s" % (V2_ROUTE_TEMPLATES, routes))

check_v2_routes()
