# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import json
import copy
import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


ROOT = os.path.dirname(os.path.realpath(__file__))
GECKO = os.path.realpath(os.path.join(ROOT, '..', '..'))
DOCKER_ROOT = os.path.join(ROOT, '..', 'docker')
MOZHARNESS_CONFIG = os.path.join(GECKO, 'testing', 'mozharness', 'mozharness.json')

# XXX: If/when we have the taskcluster queue use construct url instead
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
REGISTRY = open(os.path.join(DOCKER_ROOT, 'REGISTRY')).read().strip()

DEFINE_TASK = 'queue:define-task:aws-provisioner-v1/{}'

TREEHERDER_ROUTE_PREFIX = 'tc-treeherder-stage'
TREEHERDER_ROUTES = {
    'staging': 'tc-treeherder-stage',
    'production': 'tc-treeherder'
}

DEFAULT_TRY = 'try: -b do -p all -u all'
DEFAULT_JOB_PATH = os.path.join(
    ROOT, 'tasks', 'branches', 'base_jobs.yml'
)

def load_mozharness_info():
    with open(MOZHARNESS_CONFIG) as content:
        return json.load(content)

def docker_image(name):
    ''' Determine the docker tag/revision from an in tree docker file '''
    repository_path = os.path.join(DOCKER_ROOT, name, 'REGISTRY')
    repository = REGISTRY

    version = open(os.path.join(DOCKER_ROOT, name, 'VERSION')).read().strip()

    if os.path.isfile(repository_path):
        repository = open(repository_path).read().strip()

    return '{}/{}:{}'.format(repository, name, version)

def get_task(task_id):
    import urllib2
    return json.load(urllib2.urlopen("https://queue.taskcluster.net/v1/task/" + task_id))


def gaia_info():
    '''
    Fetch details from in tree gaia.json (which links this version of
    gecko->gaia) and construct the usual base/head/ref/rev pairing...
    '''
    gaia = json.load(open(os.path.join(GECKO, 'b2g', 'config', 'gaia.json')))

    if gaia['git'] is None or \
       gaia['git']['remote'] == '' or \
       gaia['git']['git_revision'] == '' or \
       gaia['git']['branch'] == '':

       # Just use the hg params...
       return {
         'gaia_base_repository': 'https://hg.mozilla.org/{}'.format(gaia['repo_path']),
         'gaia_head_repository': 'https://hg.mozilla.org/{}'.format(gaia['repo_path']),
         'gaia_ref': gaia['revision'],
         'gaia_rev': gaia['revision']
       }

    else:
        # Use git
        return {
            'gaia_base_repository': gaia['git']['remote'],
            'gaia_head_repository': gaia['git']['remote'],
            'gaia_rev': gaia['git']['git_revision'],
            'gaia_ref': gaia['git']['branch'],
        }

def decorate_task_treeherder_routes(task, suffix):
    """
    Decorate the given task with treeherder routes.

    Uses task.extra.treeherderEnv if available otherwise defaults to only
    staging.

    :param dict task: task definition.
    :param str suffix: The project/revision_hash portion of the route.
    """

    if 'extra' not in task:
        return

    if 'routes' not in task:
        task['routes'] = []

    treeheder_env = task['extra'].get('treeherderEnv', ['staging'])

    for env in treeheder_env:
        task['routes'].append('{}.{}'.format(TREEHERDER_ROUTES[env], suffix))

def configure_dependent_task(task_path, parameters, taskid, templates, build_treeherder_config):
    """
    Configure a build dependent task. This is shared between post-build and test tasks.

    :param task_path: location to the task yaml
    :param parameters: parameters to load the template
    :param taskid: taskid of the dependent task
    :param templates: reference to the template builder
    :param build_treeherder_config: parent treeherder config
    :return: the configured task
    """
    task = templates.load(task_path, parameters)
    task['taskId'] = taskid

    if 'requires' not in task:
        task['requires'] = []

    task['requires'].append(parameters['build_slugid'])

    if 'treeherder' not in task['task']['extra']:
        task['task']['extra']['treeherder'] = {}

    # Copy over any treeherder configuration from the build so
    # tests show up under the same platform...
    treeherder_config = task['task']['extra']['treeherder']

    treeherder_config['collection'] = \
        build_treeherder_config.get('collection', {})

    treeherder_config['build'] = \
        build_treeherder_config.get('build', {})

    treeherder_config['machine'] = \
        build_treeherder_config.get('machine', {})

    if 'routes' not in task['task']:
        task['task']['routes'] = []

    if 'scopes' not in task['task']:
        task['task']['scopes'] = []

    return task

@CommandProvider
class DecisionTask(object):
    @Command('taskcluster-decision', category="ci",
        description="Build a decision task")
    @CommandArgument('--project',
        required=True,
        help='Treeherder project name')
    @CommandArgument('--url',
        required=True,
        help='Gecko repository to use as head repository.')
    @CommandArgument('--revision',
        required=True,
        help='Revision for this project')
    @CommandArgument('--revision-hash',
        help='Treeherder revision hash')
    @CommandArgument('--comment',
        required=True,
        help='Commit message for this revision')
    @CommandArgument('--owner',
        required=True,
        help='email address of who owns this graph')
    @CommandArgument('task', help="Path to decision task to run.")
    def run_task(self, **params):
        from taskcluster_graph.slugidjar import SlugidJar
        from taskcluster_graph.from_now import (
            json_time_from_now,
            current_json_time,
        )
        from taskcluster_graph.templates import Templates

        templates = Templates(ROOT)
        # Template parameters used when expanding the graph
        parameters = dict(gaia_info().items() + {
            'source': 'http://todo.com/soon',
            'project': params['project'],
            'comment': params['comment'],
            'url': params['url'],
            'revision': params['revision'],
            'revision_hash': params.get('revision_hash', ''),
            'owner': params['owner'],
            'as_slugid': SlugidJar(),
            'from_now': json_time_from_now,
            'now': current_json_time()
        }.items())
        task = templates.load(params['task'], parameters)
        print(json.dumps(task, indent=4))

@CommandProvider
class Graph(object):
    @Command('taskcluster-graph', category="ci",
        description="Create taskcluster task graph")
    @CommandArgument('--base-repository',
        default=os.environ.get('GECKO_BASE_REPOSITORY'),
        help='URL for "base" repository to clone')
    @CommandArgument('--head-repository',
        default=os.environ.get('GECKO_HEAD_REPOSITORY'),
        help='URL for "head" repository to fetch revision from')
    @CommandArgument('--head-ref',
        default=os.environ.get('GECKO_HEAD_REF'),
        help='Reference (this is same as rev usually for hg)')
    @CommandArgument('--head-rev',
        default=os.environ.get('GECKO_HEAD_REV'),
        help='Commit revision to use from head repository')
    @CommandArgument('--message',
        help='Commit message to be parsed. Example: "try: -b do -p all -u all"')
    @CommandArgument('--revision-hash',
            required=False,
            help='Treeherder revision hash to attach results to')
    @CommandArgument('--project',
        required=True,
        help='Project to use for creating task graph. Example: --project=try')
    @CommandArgument('--pushlog-id',
        dest='pushlog_id',
        required=False,
        default=0)
    @CommandArgument('--owner',
        required=True,
        help='email address of who owns this graph')
    @CommandArgument('--extend-graph',
        action="store_true", dest="ci", help='Omit create graph arguments')
    def create_graph(self, **params):
        from taskcluster_graph.commit_parser import parse_commit
        from taskcluster_graph.slugid import slugid
        from taskcluster_graph.from_now import (
            json_time_from_now,
            current_json_time,
        )
        from taskcluster_graph.templates import Templates
        import taskcluster_graph.build_task

        project = params['project']
        message = params.get('message', '') if project == 'try' else DEFAULT_TRY

        # Message would only be blank when not created from decision task
        if project == 'try' and not message:
            sys.stderr.write(
                    "Must supply commit message when creating try graph. " \
                    "Example: --message='try: -b do -p all -u all'"
            )
            sys.exit(1)

        templates = Templates(ROOT)
        job_path = os.path.join(ROOT, 'tasks', 'branches', project, 'job_flags.yml')
        job_path = job_path if os.path.exists(job_path) else DEFAULT_JOB_PATH

        jobs = templates.load(job_path, {})

        job_graph = parse_commit(message, jobs)
        mozharness = load_mozharness_info()

        # Template parameters used when expanding the graph
        parameters = dict(gaia_info().items() + {
            'project': project,
            'pushlog_id': params.get('pushlog_id', 0),
            'docker_image': docker_image,
            'base_repository': params['base_repository'] or \
                params['head_repository'],
            'head_repository': params['head_repository'],
            'head_ref': params['head_ref'] or params['head_rev'],
            'head_rev': params['head_rev'],
            'owner': params['owner'],
            'from_now': json_time_from_now,
            'now': current_json_time(),
            'mozharness_repository': mozharness['repo'],
            'mozharness_rev': mozharness['revision'],
            'mozharness_ref':mozharness.get('reference', mozharness['revision']),
            'revision_hash': params['revision_hash']
        }.items())

        treeherder_route = '{}.{}'.format(
            params['project'],
            params.get('revision_hash', '')
        )

        # Task graph we are generating for taskcluster...
        graph = {
            'tasks': [],
            'scopes': []
        }

        if params['revision_hash']:
            for env in TREEHERDER_ROUTES:
                graph['scopes'].append('queue:route:{}.{}'.format(TREEHERDER_ROUTES[env], treeherder_route))

        graph['metadata'] = {
            'source': 'http://todo.com/what/goes/here',
            'owner': params['owner'],
            # TODO: Add full mach commands to this example?
            'description': 'Task graph generated via ./mach taskcluster-graph',
            'name': 'task graph local'
        }

        for build in job_graph:
            build_parameters = dict(parameters)
            build_parameters['build_slugid'] = slugid()
            build_task = templates.load(build['task'], build_parameters)

            if 'routes' not in build_task['task']:
                build_task['task']['routes'] = []

            if params['revision_hash']:
                decorate_task_treeherder_routes(build_task['task'],
                                                treeherder_route)

            # Ensure each build graph is valid after construction.
            taskcluster_graph.build_task.validate(build_task)
            graph['tasks'].append(build_task)

            test_packages_url, tests_url = None, None

            if 'test_packages' in build_task['task']['extra']['locations']:
                test_packages_url = ARTIFACT_URL.format(
                    build_parameters['build_slugid'],
                    build_task['task']['extra']['locations']['test_packages']
                )

            if 'tests' in build_task['task']['extra']['locations']:
                tests_url = ARTIFACT_URL.format(
                    build_parameters['build_slugid'],
                    build_task['task']['extra']['locations']['tests']
                )

            build_url = ARTIFACT_URL.format(
                build_parameters['build_slugid'],
                build_task['task']['extra']['locations']['build']
            )

            # img_url is only necessary for device builds
            img_url = ARTIFACT_URL.format(
                build_parameters['build_slugid'],
                build_task['task']['extra']['locations'].get('img', '')
            )

            define_task = DEFINE_TASK.format(build_task['task']['workerType'])

            graph['scopes'].append(define_task)
            graph['scopes'].extend(build_task['task'].get('scopes', []))
            route_scopes = map(lambda route: 'queue:route:' + route, build_task['task'].get('routes', []))
            graph['scopes'].extend(route_scopes)

            # Treeherder symbol configuration for the graph required for each
            # build so tests know which platform they belong to.
            build_treeherder_config = build_task['task']['extra']['treeherder']

            if 'machine' not in build_treeherder_config:
                message = '({}), extra.treeherder.machine required for all builds'
                raise ValueError(message.format(build['task']))

            if 'build' not in build_treeherder_config:
                build_treeherder_config['build'] = \
                    build_treeherder_config['machine']

            if 'collection' not in build_treeherder_config:
                build_treeherder_config['collection'] = { 'opt': True }

            if len(build_treeherder_config['collection'].keys()) != 1:
                message = '({}), extra.treeherder.collection must contain one type'
                raise ValueError(message.fomrat(build['task']))

            if 'post-build' in build:
                # copy over the old parameters to update the template
                post_parameters = copy.copy(build_parameters)
                post_task = configure_dependent_task(build['post-build']['task'],
                                                     post_parameters,
                                                     slugid(),
                                                     templates,
                                                     build_treeherder_config)
                graph['tasks'].append(post_task)

            for test in build['dependents']:
                test = test['allowed_build_tasks'][build['task']]
                test_parameters = copy.copy(build_parameters)
                test_parameters['build_url'] = build_url
                test_parameters['img_url'] = img_url
                if tests_url:
                    test_parameters['tests_url'] = tests_url
                if test_packages_url:
                    test_parameters['test_packages_url'] = test_packages_url
                test_definition = templates.load(test['task'], {})['task']
                chunk_config = test_definition['extra']['chunks']

                # Allow branch configs to override task level chunking...
                if 'chunks' in test:
                    chunk_config['total'] = test['chunks']

                test_parameters['total_chunks'] = chunk_config['total']

                for chunk in range(1, chunk_config['total'] + 1):
                    if 'only_chunks' in test and \
                        chunk not in test['only_chunks']:
                        continue

                    test_parameters['chunk'] = chunk
                    test_task = configure_dependent_task(test['task'],
                                                         test_parameters,
                                                         slugid(),
                                                         templates,
                                                         build_treeherder_config)

                    if params['revision_hash']:
                        decorate_task_treeherder_routes(
                                test_task['task'], treeherder_route)

                    graph['tasks'].append(test_task)

                    define_task = DEFINE_TASK.format(
                        test_task['task']['workerType']
                    )

                    graph['scopes'].append(define_task)
                    graph['scopes'].extend(test_task['task'].get('scopes', []))

        graph['scopes'] = list(set(graph['scopes']))

        # When we are extending the graph remove extra fields...
        if params['ci'] is True:
            graph.pop('scopes', None)
            graph.pop('metadata', None)

        print(json.dumps(graph, indent=4))

@CommandProvider
class CIBuild(object):
    @Command('taskcluster-build', category='ci',
        description="Create taskcluster try server build task")
    @CommandArgument('--base-repository',
        help='URL for "base" repository to clone')
    @CommandArgument('--head-repository',
        required=True,
        help='URL for "head" repository to fetch revision from')
    @CommandArgument('--head-ref',
        help='Reference (this is same as rev usually for hg)')
    @CommandArgument('--head-rev',
        required=True,
        help='Commit revision to use')
    @CommandArgument('--mozharness-repository',
        help='URL for custom mozharness repo')
    @CommandArgument('--mozharness-rev',
        help='Commit revision to use from mozharness repository')
    @CommandArgument('--owner',
        default='foobar@mozilla.com',
        help='email address of who owns this graph')
    @CommandArgument('build_task',
        help='path to build task definition')
    def create_ci_build(self, **params):
        from taskcluster_graph.templates import Templates
        import taskcluster_graph.build_task

        templates = Templates(ROOT)
        # TODO handle git repos
        head_repository = params['head_repository']
        if not head_repository:
            head_repository = get_hg_url()

        head_rev = params['head_rev']
        if not head_rev:
            head_rev = get_latest_hg_revision(head_repository)

        head_ref = params['head_ref'] or head_rev

        mozharness = load_mozharness_info()

        mozharness_repo = params['mozharness_repository']
        if mozharness_repo is None:
            mozharness_repo = mozharness['repo']

        mozharness_rev = params['mozharness_rev']
        if mozharness_rev is None:
            mozharness_rev = mozharness['revision']

        from taskcluster_graph.from_now import (
            json_time_from_now,
            current_json_time,
        )
        build_parameters = dict(gaia_info().items() + {
            'docker_image': docker_image,
            'owner': params['owner'],
            'from_now': json_time_from_now,
            'now': current_json_time(),
            'base_repository': params['base_repository'] or head_repository,
            'head_repository': head_repository,
            'head_rev': head_rev,
            'head_ref': head_ref,
            'mozharness_repository': mozharness_repo,
            'mozharness_ref': mozharness_rev,
            'mozharness_rev': mozharness_rev
        }.items())

        try:
            build_task = templates.load(params['build_task'], build_parameters)
        except IOError:
            sys.stderr.write(
                "Could not load build task file.  Ensure path is a relative " \
                "path from testing/taskcluster"
            )
            sys.exit(1)

        taskcluster_graph.build_task.validate(build_task)

        print(json.dumps(build_task['task'], indent=4))
