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

import hashlib
import os
import re
import time
from copy import deepcopy

from mozbuild.util import memoize
from taskgraph.util.attributes import TRUNK_PROJECTS
from taskgraph.util.hash import hash_path
from taskgraph.util.treeherder import split_symbol
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import (
    validate_schema,
    Schema,
    optionally_keyed_by,
    resolve_keyed_by,
    OptimizationSchema,
)
from taskgraph.util.scriptworker import (
    BALROG_ACTIONS,
    get_release_config,
    add_scope_prefix,
)
from voluptuous import Any, Required, Optional, Extra
from taskgraph import GECKO, MAX_DEPENDENCIES
from ..util import docker as dockerutil

RUN_TASK = os.path.join(GECKO, 'taskcluster', 'scripts', 'run-task')


@memoize
def _run_task_suffix():
    """String to append to cache names under control of run-task."""
    return hash_path(RUN_TASK)[0:20]


# shortcut for a string where task references are allowed
taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring},
)

# A task description is a general description of a TaskCluster task
task_description_schema = Schema({
    # the label for this task
    Required('label'): basestring,

    # description of the task (for metadata)
    Required('description'): basestring,

    # attributes for this task
    Optional('attributes'): {basestring: object},

    # relative path (from config.path) to the file task was defined in
    Optional('job-from'): basestring,

    # dependencies of this task, keyed by name; these are passed through
    # verbatim and subject to the interpretation of the Task's get_dependencies
    # method.
    Optional('dependencies'): {basestring: object},

    Optional('requires'): Any('all-completed', 'all-resolved'),

    # expiration and deadline times, relative to task creation, with units
    # (e.g., "14 days").  Defaults are set based on the project.
    Optional('expires-after'): basestring,
    Optional('deadline-after'): basestring,

    # custom routes for this task; the default treeherder routes will be added
    # automatically
    Optional('routes'): [basestring],

    # custom scopes for this task; any scopes required for the worker will be
    # added automatically. The following parameters will be substituted in each
    # scope:
    #  {level} -- the scm level of this push
    #  {project} -- the project of this push
    Optional('scopes'): [basestring],

    # Tags
    Optional('tags'): {basestring: basestring},

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
    },

    # information for indexing this build so its artifacts can be discovered;
    # if omitted, the build will not be indexed.
    Optional('index'): {
        # the name of the product this build produces
        'product': basestring,

        # the names to use for this job in the TaskCluster index
        'job-name': basestring,

        # Type of gecko v2 index to use
        'type': Any('generic', 'nightly', 'l10n', 'nightly-with-multi-l10n',
                    'release', 'nightly-l10n'),

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

        'channel': optionally_keyed_by('project', basestring),
    },

    # The `run_on_projects` attribute, defaulting to "all".  This dictates the
    # projects on which this task should be included in the target task set.
    # See the attributes documentation for details.
    Optional('run-on-projects'): optionally_keyed_by('build-platform', [basestring]),

    # The `shipping_phase` attribute, defaulting to None. This specifies the
    # release promotion phase that this task belongs to.
    Required('shipping-phase'): Any(
        None,
        'build',
        'promote',
        'push',
        'ship',
    ),

    # The `shipping_product` attribute, defaulting to None. This specifies the
    # release promotion product that this task belongs to.
    Required('shipping-product'): Any(
        None,
        basestring
    ),

    # Coalescing provides the facility for tasks to be superseded by the same
    # task in a subsequent commit, if the current task backlog reaches an
    # explicit threshold. Both age and size thresholds need to be met in order
    # for coalescing to be triggered.
    Optional('coalesce'): {
        # A unique identifier per job (typically a hash of the job label) in
        # order to partition tasks into appropriate sets for coalescing. This
        # is combined with the project in order to generate a unique coalescing
        # key for the coalescing service.
        'job-identifier': basestring,

        # The minimum amount of time in seconds between two pending tasks with
        # the same coalescing key, before the coalescing service will return
        # tasks.
        'age': int,

        # The minimum number of backlogged tasks with the same coalescing key,
        # before the coalescing service will return tasks.
        'size': int,
    },

    # The `always-target` attribute will cause the task to be included in the
    # target_task_graph regardless of filtering. Tasks included in this manner
    # will be candidates for optimization even when `optimize_target_tasks` is
    # False, unless the task was also explicitly chosen by the target_tasks
    # method.
    Required('always-target'): bool,

    # Optimization to perform on this task during the optimization phase.
    # Optimizations are defined in taskcluster/taskgraph/optimize.py.
    Required('optimization'): OptimizationSchema,

    # the provisioner-id/worker-type for the task.  The following parameters will
    # be substituted in this string:
    #  {level} -- the scm level of this push
    'worker-type': basestring,

    # Whether the job should use sccache compiler caching.
    Required('needs-sccache'): bool,

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
            {'in-tree': basestring},
            # an indexed docker image
            {'indexed': basestring},
        ),

        # worker features that should be enabled
        Required('relengapi-proxy'): bool,
        Required('chain-of-trust'): bool,
        Required('taskcluster-proxy'): bool,
        Required('allow-ptrace'): bool,
        Required('loopback-video'): bool,
        Required('loopback-audio'): bool,
        Required('docker-in-docker'): bool,  # (aka 'dind')

        # Paths to Docker volumes.
        #
        # For in-tree Docker images, volumes can be parsed from Dockerfile.
        # This only works for the Dockerfile itself: if a volume is defined in
        # a base image, it will need to be declared here. Out-of-tree Docker
        # images will also require explicit volume annotation.
        #
        # Caches are often mounted to the same path as Docker volumes. In this
        # case, they take precedence over a Docker volume. But a volume still
        # needs to be declared for the path.
        Optional('volumes'): [basestring],

        # caches to set up for the task
        Optional('caches'): [{
            # only one type is supported by any of the workers right now
            'type': 'persistent',

            # name of the cache, allowing re-use by subsequent tasks naming the
            # same cache
            'name': basestring,

            # location in the task image where the cache will be mounted
            'mount-point': basestring,

            # Whether the cache is not used in untrusted environments
            # (like the Try repo).
            Optional('skip-untrusted'): bool,
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
        Required('env'): {basestring: taskref_or_string},

        # the command to run; if not given, docker-worker will default to the
        # command in the docker image
        Optional('command'): [taskref_or_string],

        # the maximum time to run, in seconds
        Required('max-run-time'): int,

        # the exit status code(s) that indicates the task should be retried
        Optional('retry-exit-status'): [int],

        # the exit status code(s) that indicates the caches used by the task
        # should be purged
        Optional('purge-caches-exit-status'): [int],

        # Wether any artifacts are assigned to this worker
        Optional('skip-artifacts'): bool,
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
        Required('env'): {basestring: taskref_or_string},

        # the maximum time to run, in seconds
        Required('max-run-time'): int,

        # os user groups for test task workers
        Optional('os-groups'): [basestring],

        # optional features
        Required('chain-of-trust'): bool,
        Optional('taskcluster-proxy'): bool,

        # Wether any artifacts are assigned to this worker
        Optional('skip-artifacts'): bool,
    }, {
        Required('implementation'): 'native-engine',
        Required('os'): Any('macosx', 'linux'),

        # the maximum time to run, in seconds
        Required('max-run-time'): int,

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
        # Wether any artifacts are assigned to this worker
        Optional('skip-artifacts'): bool,
    }, {
        Required('implementation'): 'script-engine-autophone',
        Required('os'): Any('macosx', 'linux'),

        # A link for an executable to download
        Optional('context'): basestring,

        # Tells the worker whether machine should reboot
        # after the task is finished.
        Optional('reboot'):
            Any(False, 'always', 'never', 'on-exception', 'on-failure'),

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

        # the maximum time to run, in seconds
        Required('max-run-time'): int,

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
        Required('implementation'): 'binary-transparency',
    }, {
        Required('implementation'): 'beetmover',

        # the maximum time to run, in seconds
        Required('max-run-time', default=600): int,

        # locale key, if this is a locale beetmover job
        Optional('locale'): basestring,

        Optional('partner-public'): bool,

        Required('release-properties'): {
            'app-name': basestring,
            'app-version': basestring,
            'branch': basestring,
            'build-id': basestring,
            'hash-type': basestring,
            'platform': basestring,
        },

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
        Required('implementation'): 'beetmover-push-to-release',

        # the maximum time to run, in seconds
        Required('max-run-time'): int,
        Required('product'): basestring,
    }, {
        Required('implementation'): 'balrog',
        Required('balrog-action'): Any(*BALROG_ACTIONS),
        Optional('product'): basestring,
        Optional('platforms'): [basestring],
        Optional('release-eta'): basestring,
        Optional('channel-names'): optionally_keyed_by('project', [basestring]),
        Optional('require-mirrors'): bool,
        Optional('publish-rules'): optionally_keyed_by('project', [int]),
        Optional('rules-to-update'): optionally_keyed_by('project', [basestring]),
        Optional('archive-domain'): optionally_keyed_by('project', basestring),
        Optional('download-domain'): optionally_keyed_by('project', basestring),

        # list of artifact URLs for the artifacts that should be beetmoved
        Optional('upstream-artifacts'): [{
            # taskId of the task with the artifact
            Required('taskId'): taskref_or_string,

            # type of signing task (for CoT)
            Required('taskType'): basestring,

            # Paths to the artifacts to sign
            Required('paths'): [basestring],
        }],
    }, {
        Required('implementation'): 'bouncer-aliases',
        Required('entries'): object,
    }, {
        Required('implementation'): 'bouncer-submission',
        Required('locales'): [basestring],
        Required('entries'): object,
    }, {
        Required('implementation'): 'invalid',
        # an invalid task is one which should never actually be created; this is used in
        # release automation on branches where the task just doesn't make sense
        Extra: object,

    }, {
        Required('implementation'): 'always-optimized',
        Extra: object,

    }, {
        Required('implementation'): 'push-apk',
        Required('upstream-artifacts'): [{
            Required('taskId'): taskref_or_string,
            Required('taskType'): basestring,
            Required('paths'): [basestring],
            Optional('optional', default=False): bool,
        }],

        # "Invalid" is a noop for try and other non-supported branches
        Required('google-play-track'): Any('production', 'beta', 'alpha', 'rollout', 'invalid'),
        Required('commit'): bool,
        Optional('rollout-percentage'): Any(int, None),
    }, {
        Required('implementation'): 'push-snap',
        Required('upstream-artifacts'): [{
            Required('taskId'): taskref_or_string,
            Required('taskType'): basestring,
            Required('paths'): [basestring],
        }],
    }, {
        Required('implementation'): 'sign-and-push-addons',
        Required('channel'): Any('listed', 'unlisted'),
        Required('upstream-artifacts'): [{
            Required('taskId'): taskref_or_string,
            Required('taskType'): basestring,
            Required('paths'): [basestring],
        }],
    }, {
        Required('implementation'): 'shipit-shipped',
        Required('release-name'): basestring,
    }, {
        Required('implementation'): 'treescript',
        Required('tags'): [Any('buildN', 'release', None)],
        Required('bump'): bool,
        Optional('bump-files'): [basestring],
        Optional('repo-param-prefix'): basestring,
        Required('force-dry-run', default=True): bool,
        Required('push', default=False): bool
    }),
})

TC_TREEHERDER_SCHEMA_URL = 'https://github.com/taskcluster/taskcluster-treeherder/' \
                           'blob/master/schemas/task-treeherder-config.yml'


UNKNOWN_GROUP_NAME = "Treeherder group {} (from {}) has no name; " \
                     "add it to taskcluster/ci/config.yml"

V2_ROUTE_TEMPLATES = [
    "index.{trust-domain}.v2.{project}.latest.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.pushdate.{build_date_long}.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.pushlog-id.{pushlog_id}.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.revision.{branch_rev}.{product}.{job-name}",
]

# {central, inbound, autoland} write to a "trunk" index prefix. This facilitates
# walking of tasks with similar configurations.
V2_TRUNK_ROUTE_TEMPLATES = [
    "index.{trust-domain}.v2.trunk.revision.{branch_rev}.{product}.{job-name}",
]

V2_NIGHTLY_TEMPLATES = [
    "index.{trust-domain}.v2.{project}.nightly.latest.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.nightly.{build_date}.revision.{branch_rev}.{product}.{job-name}",  # noqa - too long
    "index.{trust-domain}.v2.{project}.nightly.{build_date}.latest.{product}.{job-name}",
    "index.{trust-domain}.v2.{project}.nightly.revision.{branch_rev}.{product}.{job-name}",
]

V2_NIGHTLY_L10N_TEMPLATES = [
    "index.{trust-domain}.v2.{project}.nightly.latest.{product}-l10n.{job-name}.{locale}",
    "index.{trust-domain}.v2.{project}.nightly.{build_date}.revision.{branch_rev}.{product}-l10n.{job-name}.{locale}",  # noqa - too long
    "index.{trust-domain}.v2.{project}.nightly.{build_date}.latest.{product}-l10n.{job-name}.{locale}",  # noqa - too long
    "index.{trust-domain}.v2.{project}.nightly.revision.{branch_rev}.{product}-l10n.{job-name}.{locale}",  # noqa - too long
]

V2_L10N_TEMPLATES = [
    "index.{trust-domain}.v2.{project}.revision.{branch_rev}.{product}-l10n.{job-name}.{locale}",
    "index.{trust-domain}.v2.{project}.pushdate.{build_date_long}.{product}-l10n.{job-name}.{locale}",  # noqa - too long
    "index.{trust-domain}.v2.{project}.pushlog-id.{pushlog_id}.{product}-l10n.{job-name}.{locale}",
    "index.{trust-domain}.v2.{project}.latest.{product}-l10n.{job-name}.{locale}",
]

# the roots of the treeherder routes
TREEHERDER_ROUTE_ROOT = 'tc-treeherder'


def get_branch_rev(config):
    return config.params['{}head_rev'.format(
        config.graph_config['project-repo-param-prefix']
    )]


def get_branch_repo(config):
    return config.params['{}head_repository'.format(
        config.graph_config['project-repo-param-prefix'],
    )]


COALESCE_KEY = '{project}.{job-identifier}'
SUPERSEDER_URL = 'https://coalesce.mozilla-releng.net/v1/list/{age}/{size}/{key}'

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


def coalesce_key(config, task):
    return COALESCE_KEY.format(**{
               'project': config.params['project'],
               'job-identifier': task['coalesce']['job-identifier'],
           })


def superseder_url(config, task):
    key = coalesce_key(config, task)
    age = task['coalesce']['age']
    size = task['coalesce']['size']
    return SUPERSEDER_URL.format(
        age=age,
        size=size,
        key=key
    )


UNSUPPORTED_INDEX_PRODUCT_ERROR = """\
The gecko-v2 product {product} is not in the list of configured products in
`taskcluster/ci/config.yml'.
"""


def verify_index(config, index):
    product = index['product']
    if product not in config.graph_config['index']['products']:
        raise Exception(UNSUPPORTED_INDEX_PRODUCT_ERROR.format(product=product))


@payload_builder('docker-worker')
def build_docker_worker_payload(config, task, task_def):
    worker = task['worker']
    level = int(config.params['level'])

    image = worker['docker-image']
    if isinstance(image, dict):
        if 'in-tree' in image:
            name = image['in-tree']
            docker_image_task = 'build-docker-image-' + image['in-tree']
            task.setdefault('dependencies', {})['docker-image'] = docker_image_task

            image = {
                "path": "public/image.tar.zst",
                "taskId": {"task-reference": "<docker-image>"},
                "type": "task-image",
            }

            # Find VOLUME in Dockerfile.
            volumes = dockerutil.parse_volumes(name)
            for v in sorted(volumes):
                if v in worker['volumes']:
                    raise Exception('volume %s already defined; '
                                    'if it is defined in a Dockerfile, '
                                    'it does not need to be specified in the '
                                    'worker definition' % v)

                worker['volumes'].append(v)

        elif 'indexed' in image:
            image = {
                "path": "public/image.tar.zst",
                "namespace": image['indexed'],
                "type": "indexed-image",
            }
        else:
            raise Exception("unknown docker image type")

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
            'assume:project:taskcluster:{trust_domain}:level-{level}-sccache-buckets'.format(
                trust_domain=config.graph_config['trust-domain'],
                level=config.params['level'])
        )
        worker['env']['USE_SCCACHE'] = '1'
        # Disable sccache idle shutdown.
        worker['env']['SCCACHE_IDLE_TIMEOUT'] = '0'
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

    run_task = payload.get('command', [''])[0].endswith('run-task')

    # run-task exits EXIT_PURGE_CACHES if there is a problem with caches.
    # Automatically retry the tasks and purge caches if we see this exit
    # code.
    # TODO move this closer to code adding run-task once bug 1469697 is
    # addressed.
    if run_task:
        worker.setdefault('retry-exit-status', []).append(72)
        worker.setdefault('purge-caches-exit-status', []).append(72)

    payload['onExitStatus'] = {}
    if 'retry-exit-status' in worker:
        payload['onExitStatus']['retry'] = worker['retry-exit-status']
    if 'purge-caches-exit-status' in worker:
        payload['onExitStatus']['purgeCaches'] = worker['purge-caches-exit-status']

    if 'artifacts' in worker:
        artifacts = {}
        for artifact in worker['artifacts']:
            artifacts[artifact['name']] = {
                'path': artifact['path'],
                'type': artifact['type'],
                'expires': task_def['expires'],  # always expire with the task
            }
        payload['artifacts'] = artifacts

    if isinstance(worker.get('docker-image'), basestring):
        out_of_tree_image = worker['docker-image']
        run_task = run_task or out_of_tree_image.startswith(
            'taskcluster/image_builder')
    else:
        out_of_tree_image = None
        image = worker.get('docker-image', {}).get('in-tree')
        run_task = run_task or image == 'image_builder'

    if 'caches' in worker:
        caches = {}

        # run-task knows how to validate caches.
        #
        # To help ensure new run-task features and bug fixes don't interfere
        # with existing caches, we seed the hash of run-task into cache names.
        # So, any time run-task changes, we should get a fresh set of caches.
        # This means run-task can make changes to cache interaction at any time
        # without regards for backwards or future compatibility.
        #
        # But this mechanism only works for in-tree Docker images that are built
        # with the current run-task! For out-of-tree Docker images, we have no
        # way of knowing their content of run-task. So, in addition to varying
        # cache names by the contents of run-task, we also take the Docker image
        # name into consideration. This means that different Docker images will
        # never share the same cache. This is a bit unfortunate. But it is the
        # safest thing to do. Fortunately, most images are defined in-tree.
        #
        # For out-of-tree Docker images, we don't strictly need to incorporate
        # the run-task content into the cache name. However, doing so preserves
        # the mechanism whereby changing run-task results in new caches
        # everywhere.

        # As an additional mechanism to force the use of different caches, the
        # string literal in the variable below can be changed. This is
        # preferred to changing run-task because it doesn't require images
        # to be rebuilt.
        cache_version = 'v3'

        if run_task:
            suffix = '-%s-%s' % (cache_version, _run_task_suffix())

            if out_of_tree_image:
                name_hash = hashlib.sha256(out_of_tree_image).hexdigest()
                suffix += name_hash[0:12]

        else:
            suffix = '-%s' % cache_version

        skip_untrusted = config.params.is_try() or level == 1

        for cache in worker['caches']:
            # Some caches aren't enabled in environments where we can't
            # guarantee certain behavior. Filter those out.
            if cache.get('skip-untrusted') and skip_untrusted:
                continue

            name = '%s%s' % (cache['name'], suffix)
            caches[name] = cache['mount-point']
            task_def['scopes'].append('docker-worker:cache:%s' % name)

        # Assertion: only run-task is interested in this.
        if run_task:
            payload['env']['TASKCLUSTER_CACHES'] = ';'.join(sorted(
                caches.values()))

        payload['cache'] = caches

    # And send down volumes information to run-task as well.
    if run_task and worker.get('volumes'):
        payload['env']['TASKCLUSTER_VOLUMES'] = ';'.join(
            sorted(worker['volumes']))

    if payload.get('cache') and skip_untrusted:
        payload['env']['TASKCLUSTER_UNTRUSTED_CACHES'] = '1'

    if features:
        payload['features'] = features
    if capabilities:
        payload['capabilities'] = capabilities

    # coalesce / superseding
    if 'coalesce' in task:
        payload['supersederUrl'] = superseder_url(config, task)

    check_caches_are_volumes(task)


@payload_builder('generic-worker')
def build_generic_worker_payload(config, task, task_def):
    worker = task['worker']

    artifacts = []

    for artifact in worker.get('artifacts', []):
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

    if task.get('needs-sccache'):
        worker['env']['USE_SCCACHE'] = '1'
    else:
        worker['env']['SCCACHE_DISABLE'] = '1'

    # currently only support one feature (chain of trust) but this will likely grow
    features = {}

    if worker.get('chain-of-trust'):
        features['chainOfTrust'] = True

    if worker.get('taskcluster-proxy'):
        features['taskclusterProxy'] = True

    if features:
        task_def['payload']['features'] = features

    # coalesce / superseding
    if 'coalesce' in task:
        task_def['payload']['supersederUrl'] = superseder_url(config, task)


@payload_builder('scriptworker-signing')
def build_scriptworker_signing_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'maxRunTime': worker['max-run-time'],
        'upstreamArtifacts':  worker['upstream-artifacts']
    }


@payload_builder('binary-transparency')
def build_binary_transparency_payload(config, task, task_def):
    release_config = get_release_config(config)

    task_def['payload'] = {
        'version': release_config['version'],
        'chain': 'TRANSPARENCY.pem',
        'contact': task_def['metadata']['owner'],
        'maxRunTime': 600,
        'stage-product': task['shipping-product'],
        'summary': (
            'https://archive.mozilla.org/pub/{}/candidates/'
            '{}-candidates/build{}/SHA256SUMMARY'
        ).format(
            task['shipping-product'],
            release_config['version'],
            release_config['build_number'],
        ),
    }


@payload_builder('beetmover')
def build_beetmover_payload(config, task, task_def):
    worker = task['worker']
    release_config = get_release_config(config)
    release_properties = worker['release-properties']

    task_def['payload'] = {
        'maxRunTime': worker['max-run-time'],
        'releaseProperties': {
            'appName': release_properties['app-name'],
            'appVersion': release_properties['app-version'],
            'branch': release_properties['branch'],
            'buildid': release_properties['build-id'],
            'hashType': release_properties['hash-type'],
            'platform': release_properties['platform'],
        },
        'upload_date': config.params['build_date'],
        'upstreamArtifacts':  worker['upstream-artifacts'],
    }
    if worker.get('locale'):
        task_def['payload']['locale'] = worker['locale']
    if worker.get('partner-public'):
        task_def['payload']['is_partner_repack_public'] = worker['partner-public']
    if release_config:
        task_def['payload'].update(release_config)


@payload_builder('beetmover-push-to-release')
def build_beetmover_push_to_release_payload(config, task, task_def):
    worker = task['worker']
    release_config = get_release_config(config)

    task_def['payload'] = {
        'maxRunTime': worker['max-run-time'],
        'product': worker['product'],
        'version': release_config['version'],
        'build_number': release_config['build_number'],
    }


@payload_builder('balrog')
def build_balrog_payload(config, task, task_def):
    worker = task['worker']
    release_config = get_release_config(config)

    if worker['balrog-action'] == 'submit-locale':
        task_def['payload'] = {
            'upstreamArtifacts':  worker['upstream-artifacts']
        }
    else:
        for prop in ('archive-domain', 'channel-names', 'download-domain',
                     'publish-rules', 'rules-to-update'):
            if prop in worker:
                resolve_keyed_by(
                    worker, prop, task['description'],
                    **config.params
                )
        task_def['payload'] = {
            'build_number': release_config['build_number'],
            'product': worker['product'],
            'version': release_config['version'],
        }
        if worker['balrog-action'] == 'submit-toplevel':
            task_def['payload'].update({
                'app_version': release_config['appVersion'],
                'archive_domain': worker['archive-domain'],
                'channel_names': worker['channel-names'],
                'download_domain': worker['download-domain'],
                'partial_versions': release_config.get('partial_versions', ""),
                'platforms': worker['platforms'],
                'rules_to_update': worker['rules-to-update'],
                'require_mirrors': worker['require-mirrors'],
            })
        else:  # schedule / ship
            task_def['payload'].update({
                'publish_rules': worker['publish-rules'],
                'release_eta': worker.get('release-eta', config.params.get('release_eta')) or '',
            })


@payload_builder('bouncer-aliases')
def build_bouncer_aliases_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'aliases_entries': worker['entries']
    }


@payload_builder('bouncer-submission')
def build_bouncer_submission_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'locales':  worker['locales'],
        'submission_entries': worker['entries']
    }


@payload_builder('push-apk')
def build_push_apk_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'commit': worker['commit'],
        'upstreamArtifacts': worker['upstream-artifacts'],
        'google_play_track': worker['google-play-track'],
    }

    if worker.get('rollout-percentage', None):
        task_def['payload']['rollout_percentage'] = worker['rollout-percentage']


@payload_builder('push-snap')
def build_push_snap_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'upstreamArtifacts':  worker['upstream-artifacts'],
    }


@payload_builder('shipit-shipped')
def build_ship_it_shipped_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'release_name': worker['release-name']
    }


@payload_builder('sign-and-push-addons')
def build_sign_and_push_addons_payload(config, task, task_def):
    worker = task['worker']

    task_def['payload'] = {
        'channel': worker['channel'],
        'upstreamArtifacts': worker['upstream-artifacts'],
    }


@payload_builder('treescript')
def build_treescript_payload(config, task, task_def):
    worker = task['worker']
    release_config = get_release_config(config)

    task_def['payload'] = {}
    task_def.setdefault('scopes', [])
    if worker['tags']:
        tag_names = []
        product = task['shipping-product'].upper()
        version = release_config['version'].replace('.', '_')
        buildnum = release_config['build_number']
        if 'buildN' in worker['tags']:
            tag_names.extend([
                "{}_{}_BUILD{}".format(product, version, buildnum),
            ])
        if 'release' in worker['tags']:
            tag_names.extend([
              "{}_{}_RELEASE".format(product, version)
            ])
        tag_info = {
            'tags': tag_names,
            'revision': config.params['{}head_rev'.format(worker.get('repo-param-prefix', ''))],
        }
        task_def['payload']['tag_info'] = tag_info
        task_def['scopes'].append(add_scope_prefix(config, 'treescript:action:tagging'))

    if worker['bump']:
        if not worker['bump-files']:
            raise Exception("Version Bump requested without bump-files")

        bump_info = {}
        bump_info['next_version'] = release_config['next_version']
        bump_info['files'] = worker['bump-files']
        task_def['payload']['version_bump_info'] = bump_info
        task_def['scopes'].append(add_scope_prefix(config, 'treescript:action:version_bump'))

    if worker['push']:
        task_def['scopes'].append(add_scope_prefix(config, 'treescript:action:push'))

    if worker.get('force-dry-run'):
        task_def['payload']['dry_run'] = True


@payload_builder('invalid')
def build_invalid_payload(config, task, task_def):
    task_def['payload'] = 'invalid task - should never be created'


@payload_builder('always-optimized')
def build_always_optimized_payload(config, task, task_def):
    task_def['payload'] = {}


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
        'maxRunTime': worker['max-run-time'],
    }
    if worker.get('reboot'):
        task_def['payload'] = worker['reboot']

    if task.get('needs-sccache'):
        raise Exception('needs-sccache not supported in native-engine')


@payload_builder('script-engine-autophone')
def build_script_engine_autophone_payload(config, task, task_def):
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
        raise Exception('needs-sccache not supported in taskcluster-worker')


transforms = TransformSequence()


@transforms.add
def set_defaults(config, tasks):
    for task in tasks:
        task.setdefault('shipping-phase', None)
        task.setdefault('shipping-product', None)
        task.setdefault('always-target', False)
        task.setdefault('optimization', None)
        task.setdefault('needs-sccache', False)

        worker = task['worker']
        if worker['implementation'] in ('docker-worker', 'docker-engine'):
            worker.setdefault('relengapi-proxy', False)
            worker.setdefault('chain-of-trust', False)
            worker.setdefault('taskcluster-proxy', False)
            worker.setdefault('allow-ptrace', False)
            worker.setdefault('loopback-video', False)
            worker.setdefault('loopback-audio', False)
            worker.setdefault('docker-in-docker', False)
            worker.setdefault('volumes', [])
            worker.setdefault('env', {})
            if 'caches' in worker:
                for c in worker['caches']:
                    c.setdefault('skip-untrusted', False)
        elif worker['implementation'] == 'generic-worker':
            worker.setdefault('env', {})
            worker.setdefault('os-groups', [])
            worker.setdefault('chain-of-trust', False)
        elif worker['implementation'] == 'scriptworker-signing':
            worker.setdefault('max-run-time', 600)
        elif worker['implementation'] == 'beetmover':
            worker.setdefault('max-run-time', 600)
        elif worker['implementation'] == 'beetmover-push-to-release':
            worker.setdefault('max-run-time', 600)
        elif worker['implementation'] == 'push-apk':
            worker.setdefault('commit', False)

        yield task


@transforms.add
def task_name_from_label(config, tasks):
    for task in tasks:
        if 'label' not in task:
            if 'name' not in task:
                raise Exception("task has neither a name nor a label")
            task['label'] = '{}-{}'.format(config.kind, task['name'])
        if task.get('name'):
            del task['name']
        yield task


UNSUPPORTED_SHIPPING_PRODUCT_ERROR = """\
The shipping product {product} is not in the list of configured products in
`taskcluster/ci/config.yml'.
"""


def validate_shipping_product(config, product):
    if product not in config.graph_config['release-promotion']['products']:
        raise Exception(UNSUPPORTED_SHIPPING_PRODUCT_ERROR.format(product=product))


@transforms.add
def validate(config, tasks):
    for task in tasks:
        validate_schema(
            task_description_schema, task,
            "In task {!r}:".format(task.get('label', '?no-label?')))
        if task['shipping-product'] is not None:
            validate_shipping_product(config, task['shipping-product'])
        yield task


@index_builder('generic')
def add_generic_index_routes(config, task):
    index = task.get('index')
    routes = task.setdefault('routes', [])

    verify_index(config, index)

    subs = config.params.copy()
    subs['job-name'] = index['job-name']
    subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                            time.gmtime(config.params['build_date']))
    subs['product'] = index['product']
    subs['trust-domain'] = config.graph_config['trust-domain']
    subs['branch_rev'] = get_branch_rev(config)

    project = config.params.get('project')

    for tpl in V2_ROUTE_TEMPLATES:
        routes.append(tpl.format(**subs))

    # Additionally alias all tasks for "trunk" repos into a common
    # namespace.
    if project and project in TRUNK_PROJECTS:
        for tpl in V2_TRUNK_ROUTE_TEMPLATES:
            routes.append(tpl.format(**subs))

    return task


@index_builder('nightly')
def add_nightly_index_routes(config, task):
    index = task.get('index')
    routes = task.setdefault('routes', [])

    verify_index(config, index)

    subs = config.params.copy()
    subs['job-name'] = index['job-name']
    subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                            time.gmtime(config.params['build_date']))
    subs['build_date'] = time.strftime("%Y.%m.%d",
                                       time.gmtime(config.params['build_date']))
    subs['product'] = index['product']
    subs['trust-domain'] = config.graph_config['trust-domain']
    subs['branch_rev'] = get_branch_rev(config)

    for tpl in V2_NIGHTLY_TEMPLATES:
        routes.append(tpl.format(**subs))

    # Also add routes for en-US
    task = add_l10n_index_routes(config, task, force_locale="en-US")

    return task


@index_builder('release')
def add_release_index_routes(config, task):
    index = task.get('index')
    routes = []
    release_config = get_release_config(config)

    subs = config.params.copy()
    subs['build_number'] = str(release_config['build_number'])
    subs['revision'] = subs['head_rev']
    subs['underscore_version'] = release_config['version'].replace('.', '_')
    subs['product'] = index['product']
    subs['trust-domain'] = config.graph_config['trust-domain']
    subs['branch_rev'] = get_branch_rev(config)
    subs['branch'] = subs['project']
    if 'channel' in index:
        resolve_keyed_by(
            index, 'channel', item_name=task['label'], project=config.params['project']
        )
        subs['channel'] = index['channel']

    for rt in task.get('routes', []):
        routes.append(rt.format(**subs))

    task['routes'] = routes

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

    verify_index(config, index)

    subs = config.params.copy()
    subs['job-name'] = index['job-name']
    subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                            time.gmtime(config.params['build_date']))
    subs['product'] = index['product']
    subs['trust-domain'] = config.graph_config['trust-domain']
    subs['branch_rev'] = get_branch_rev(config)

    locales = task['attributes'].get('chunk_locales',
                                     task['attributes'].get('all_locales'))
    # Some tasks has only one locale set
    if task['attributes'].get('locale'):
        locales = [task['attributes']['locale']]

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


@index_builder('nightly-l10n')
def add_nightly_l10n_index_routes(config, task, force_locale=None):
    index = task.get('index')
    routes = task.setdefault('routes', [])

    verify_index(config, index)

    subs = config.params.copy()
    subs['job-name'] = index['job-name']
    subs['build_date_long'] = time.strftime("%Y.%m.%d.%Y%m%d%H%M%S",
                                            time.gmtime(config.params['build_date']))
    subs['build_date'] = time.strftime("%Y.%m.%d",
                                       time.gmtime(config.params['build_date']))
    subs['product'] = index['product']
    subs['trust-domain'] = config.graph_config['trust-domain']
    subs['branch_rev'] = get_branch_rev(config)

    locales = task['attributes'].get('chunk_locales',
                                     task['attributes'].get('all_locales'))
    # Some tasks has only one locale set
    if task['attributes'].get('locale'):
        locales = [task['attributes']['locale']]

    if force_locale:
        # Used for en-US and multi-locale
        locales = [force_locale]

    if not locales:
        raise Exception("Error: Unable to use l10n index for tasks without locales")

    for locale in locales:
        for tpl in V2_NIGHTLY_L10N_TEMPLATES:
            routes.append(tpl.format(locale=locale, **subs))

    return task


@transforms.add
def add_index_routes(config, tasks):
    for task in tasks:
        index = task.get('index', {})

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

        if not index:
            yield task
            continue

        index_type = index.get('type', 'generic')
        task = index_builders[index_type](config, task)

        del task['index']
        yield task


@transforms.add
def build_task(config, tasks):
    for task in tasks:
        level = str(config.params['level'])
        worker_type = task['worker-type'].format(level=level)
        provisioner_id, worker_type = worker_type.split('/', 1)
        project = config.params['project']

        routes = task.get('routes', [])
        scopes = [s.format(level=level, project=project) for s in task.get('scopes', [])]

        # set up extra
        extra = task.get('extra', {})
        extra['parent'] = os.environ.get('TASK_ID', '')
        task_th = task.get('treeherder')
        if task_th:
            extra.setdefault('treeherder-platform', task_th['platform'])
            treeherder = extra.setdefault('treeherder', {})

            machine_platform, collection = task_th['platform'].split('/', 1)
            treeherder['machine'] = {'platform': machine_platform}
            treeherder['collection'] = {collection: True}

            group_names = config.graph_config['treeherder']['group-names']
            groupSymbol, symbol = split_symbol(task_th['symbol'])
            if groupSymbol != '?':
                treeherder['groupSymbol'] = groupSymbol
                if groupSymbol not in group_names:
                    path = os.path.join(config.path, task.get('job-from', ''))
                    raise Exception(UNKNOWN_GROUP_NAME.format(groupSymbol, path))
                treeherder['groupName'] = group_names[groupSymbol]
            treeherder['symbol'] = symbol
            if len(symbol) > 25 or len(groupSymbol) > 25:
                raise RuntimeError("Treeherder group and symbol names must not be longer than "
                                   "25 characters: {} (see {})".format(
                                       task_th['symbol'],
                                       TC_TREEHERDER_SCHEMA_URL,
                                       ))
            treeherder['jobKind'] = task_th['kind']
            treeherder['tier'] = task_th['tier']

            branch_rev = get_branch_rev(config)

            routes.append(
                '{}.v2.{}.{}.{}'.format(TREEHERDER_ROUTE_ROOT,
                                        config.params['project'],
                                        branch_rev,
                                        config.params['pushlog_id'])
            )

        if 'expires-after' not in task:
            task['expires-after'] = '28 days' if config.params.is_try() else '1 year'

        if 'deadline-after' not in task:
            task['deadline-after'] = '1 day'

        if 'coalesce' in task:
            key = coalesce_key(config, task)
            routes.append('coalesce.v1.' + key)

        if 'priority' not in task:
            task['priority'] = BRANCH_PRIORITIES.get(
                config.params['project'],
                DEFAULT_BRANCH_PRIORITY)

        tags = task.get('tags', {})
        tags.update({
            'createdForUser': config.params['owner'],
            'kind': config.kind,
            'label': task['label'],
        })

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
                'source': config.params.file_url(config.path),
            },
            'extra': extra,
            'tags': tags,
            'priority': task['priority'],
        }

        if task.get('requires', None):
            task_def['requires'] = task['requires']

        if task_th:
            # link back to treeherder in description
            th_push_link = 'https://treeherder.mozilla.org/#/jobs?repo={}&revision={}'.format(
                config.params['project'], branch_rev)
            task_def['metadata']['description'] += ' ([Treeherder push]({}))'.format(
                th_push_link)

        # add the payload and adjust anything else as required (e.g., scopes)
        payload_builders[task['worker']['implementation']](config, task, task_def)

        attributes = task.get('attributes', {})
        # Resolve run-on-projects
        build_platform = attributes.get('build_platform')
        resolve_keyed_by(task, 'run-on-projects', item_name=task['label'],
                         **{'build-platform': build_platform})
        attributes['run_on_projects'] = task.get('run-on-projects', ['all'])
        attributes['always_target'] = task['always-target']
        # This logic is here since downstream tasks don't always match their
        # upstream dependency's shipping_phase.
        # A basestring task['shipping-phase'] takes precedence, then
        # an existing attributes['shipping_phase'], then fall back to None.
        if task.get('shipping-phase') is not None:
            attributes['shipping_phase'] = task['shipping-phase']
        else:
            attributes.setdefault('shipping_phase', None)
        # shipping_product will always match the upstream task's
        # shipping_product, so a pre-set existing attributes['shipping_product']
        # takes precedence over task['shipping-product']. However, make sure
        # we don't have conflicting values.
        if task.get('shipping-product') and \
                attributes.get('shipping_product') not in (None, task['shipping-product']):
            raise Exception(
                "{} shipping_product {} doesn't match task shipping-product {}!".format(
                    task['label'], attributes['shipping_product'], task['shipping-product']
                )
            )
        attributes.setdefault('shipping_product', task['shipping-product'])

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
            'optimization': task.get('optimization', None),
        }


@transforms.add
def chain_of_trust(config, tasks):
    for task in tasks:
        if task['task'].get('payload', {}).get('features', {}).get('chainOfTrust'):
            image = task.get('dependencies', {}).get('docker-image')
            if image:
                cot = task['task'].setdefault('extra', {}).setdefault('chainOfTrust', {})
                cot.setdefault('inputs', {})['docker-image'] = {
                    'task-reference': '<docker-image>'
                }
        yield task


@transforms.add
def check_task_identifiers(config, tasks):
    """Ensures that all tasks have well defined identifiers:
       ^[a-zA-Z0-9_-]{1,22}$
    """
    e = re.compile("^[a-zA-Z0-9_-]{1,22}$")
    for task in tasks:
        for attr in ('workerType', 'provisionerId'):
            if not e.match(task['task'][attr]):
                raise Exception(
                    'task {}.{} is not a valid identifier: {}'.format(
                        task['label'], attr, task['task'][attr]))
        yield task


@transforms.add
def check_task_dependencies(config, tasks):
    """Ensures that tasks don't have more than 100 dependencies."""
    for task in tasks:
        if len(task['dependencies']) > MAX_DEPENDENCIES:
            raise Exception(
                    'task {}/{} has too many dependencies ({} > {})'.format(
                        config.kind, task['label'], len(task['dependencies']),
                        MAX_DEPENDENCIES))
        yield task


def check_caches_are_volumes(task):
    """Ensures that all cache paths are defined as volumes.

    Caches and volumes are the only filesystem locations whose content
    isn't defined by the Docker image itself. Some caches are optional
    depending on the job environment. We want paths that are potentially
    caches to have as similar behavior regardless of whether a cache is
    used. To help enforce this, we require that all paths used as caches
    to be declared as Docker volumes. This check won't catch all offenders.
    But it is better than nothing.
    """
    volumes = set(task['worker']['volumes'])
    paths = set(c['mount-point'] for c in task['worker'].get('caches', []))
    missing = paths - volumes

    if not missing:
        return

    raise Exception('task %s (image %s) has caches that are not declared as '
                    'Docker volumes: %s'
                    'Have you added them as VOLUMEs in the Dockerfile?'
                    % (task['label'], task['worker']['docker-image'],
                       ', '.join(sorted(missing))))


@transforms.add
def check_run_task_caches(config, tasks):
    """Audit for caches requiring run-task.

    run-task manages caches in certain ways. If a cache managed by run-task
    is used by a non run-task task, it could cause problems. So we audit for
    that and make sure certain cache names are exclusive to run-task.

    IF YOU ARE TEMPTED TO MAKE EXCLUSIONS TO THIS POLICY, YOU ARE LIKELY
    CONTRIBUTING TECHNICAL DEBT AND WILL HAVE TO SOLVE MANY OF THE PROBLEMS
    THAT RUN-TASK ALREADY SOLVES. THINK LONG AND HARD BEFORE DOING THAT.
    """
    re_reserved_caches = re.compile('''^
        (level-\d+-checkouts|level-\d+-tooltool-cache)
    ''', re.VERBOSE)

    re_sparse_checkout_cache = re.compile('^level-\d+-checkouts-sparse')

    suffix = _run_task_suffix()

    for task in tasks:
        payload = task['task'].get('payload', {})
        command = payload.get('command') or ['']

        main_command = command[0] if isinstance(command[0], basestring) else ''
        run_task = main_command.endswith('run-task')

        require_sparse_cache = False
        have_sparse_cache = False

        if run_task:
            for arg in command[1:]:
                if not isinstance(arg, basestring):
                    continue

                if arg == '--':
                    break

                if arg.startswith('--sparse-profile'):
                    require_sparse_cache = True
                    break

        for cache in payload.get('cache', {}):
            if re_sparse_checkout_cache.match(cache):
                have_sparse_cache = True

            if not re_reserved_caches.match(cache):
                continue

            if not run_task:
                raise Exception(
                    '%s is using a cache (%s) reserved for run-task '
                    'change the task to use run-task or use a different '
                    'cache name' % (task['label'], cache))

            if not cache.endswith(suffix):
                raise Exception(
                    '%s is using a cache (%s) reserved for run-task '
                    'but the cache name is not dependent on the contents '
                    'of run-task; change the cache name to conform to the '
                    'naming requirements' % (task['label'], cache))

        if require_sparse_cache and not have_sparse_cache:
            raise Exception('%s is using a sparse checkout but not using '
                            'a sparse checkout cache; change the checkout '
                            'cache name so it is sparse aware' % task['label'])

        yield task
