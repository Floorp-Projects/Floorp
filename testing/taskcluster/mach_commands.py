# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import os.path
import json
import copy
import datetime
import subprocess
import sys
import urllib2

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from taskcluster_graph.commit_parser import parse_commit
from taskcluster_graph.slugid import slugid
from taskcluster_graph.slugidjar import SlugidJar
from taskcluster_graph.from_now import json_time_from_now, current_json_time
from taskcluster_graph.templates import Templates

import taskcluster_graph.build_task

ROOT = os.path.dirname(os.path.realpath(__file__))
GECKO = os.path.realpath(os.path.join(ROOT, '..', '..'))
DOCKER_ROOT = os.path.join(ROOT, '..', 'docker')

# XXX: If/when we have the taskcluster queue use construct url instead
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
REGISTRY = open(os.path.join(DOCKER_ROOT, 'REGISTRY')).read().strip()

DEFINE_TASK = 'queue:define-task:aws-provisioner/{}'

TREEHERDER_ROUTE_PREFIX = 'tc-treeherder-stage'

DEFAULT_TRY = 'try: -b do -p all -u all'
DEFAULT_JOB_PATH = os.path.join(
    ROOT, 'tasks', 'branches', 'mozilla-central', 'job_flags.yml'
)

def get_hg_url():
    ''' Determine the url for the mercurial repository'''
    try:
        url = subprocess.check_output(
            ['hg', 'path', 'default'],
            stderr=subprocess.PIPE
        )
    except subprocess.CalledProcessError:
        sys.stderr.write(
            "Error: Could not determine the current hg repository url. " \
            "Ensure command is executed within a hg respository"
        )
        sys.exit(1)

    return url

def get_latest_hg_revision(repository):
    ''' Retrieves the revision number of the latest changed head'''
    try:
        revision = subprocess.check_output(
            ['hg', 'id', '-r', 'tip', repository, '-i'],
            stderr=subprocess.PIPE
        ).strip('\n')
    except subprocess.CalledProcessError:
        sys.stderr.write(
            "Error: Could not determine the latest hg revision at {} " \
            "Ensure command is executed within a cloned hg respository and " \
            "remote default remote repository is accessible".format(repository)
        )
        sys.exit(1)

    return revision

def docker_image(name):
    ''' Determine the docker tag/revision from an in tree docker file '''
    repository_path = os.path.join(DOCKER_ROOT, name, 'REPOSITORY')
    repository = REGISTRY

    version = open(os.path.join(DOCKER_ROOT, name, 'VERSION')).read().strip()

    if os.path.isfile(repository_path):
        repository = open(repository_path).read().strip()

    return '{}/{}:{}'.format(repository, name, version)

def get_task(task_id):
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
            'now': datetime.datetime.now().isoformat()
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
    @CommandArgument('--mozharness-repository',
        default='https://github.com/lightsofapollo/build-mozharness',
        help='URL for custom mozharness repo')
    @CommandArgument('--head-repository',
        default=os.environ.get('GECKO_HEAD_REPOSITORY'),
        help='URL for "head" repository to fetch revision from')
    @CommandArgument('--head-ref',
        default=os.environ.get('GECKO_HEAD_REF'),
        help='Reference (this is same as rev usually for hg)')
    @CommandArgument('--head-rev',
        default=os.environ.get('GECKO_HEAD_REV'),
        help='Commit revision to use from head repository')
    @CommandArgument('--mozharness-rev',
        default='emulator-perf',
        help='Commit revision to use from mozharness repository')
    @CommandArgument('--message',
        help='Commit message to be parsed. Example: "try: -b do -p all -u all"')
    @CommandArgument('--revision-hash',
            required=False,
            help='Treeherder revision hash to attach results to')
    @CommandArgument('--project',
        required=True,
        help='Project to use for creating task graph. Example: --project=try')
    @CommandArgument('--owner',
        required=True,
        help='email address of who owns this graph')
    @CommandArgument('--extend-graph',
        action="store_true", dest="ci", help='Omit create graph arguments')
    def create_graph(self, **params):
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
        # Template parameters used when expanding the graph
        parameters = dict(gaia_info().items() + {
            'docker_image': docker_image,
            'base_repository': params['base_repository'] or \
                params['head_repository'],
            'head_repository': params['head_repository'],
            'head_ref': params['head_ref'] or params['head_rev'],
            'head_rev': params['head_rev'],
            'owner': params['owner'],
            'from_now': json_time_from_now,
            'now': datetime.datetime.now().isoformat(),
            'mozharness_repository': params['mozharness_repository'],
            'mozharness_rev': params['mozharness_rev'],
            'revision_hash': params['revision_hash']
        }.items())

        treeherder_route = '{}.{}.{}'.format(
            TREEHERDER_ROUTE_PREFIX,
            params['project'],
            params.get('revision_hash', '')
        )

        # Task graph we are generating for taskcluster...
        graph = {
            'tasks': [],
            'scopes': []
        }

        if params['revision_hash']:
            graph['scopes'].append('queue:route:{}'.format(treeherder_route))

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
                build_task['task']['routes'] = [];

            if params['revision_hash']:
                build_task['task']['routes'].append(treeherder_route)

            # Ensure each build graph is valid after construction.
            taskcluster_graph.build_task.validate(build_task)
            graph['tasks'].append(build_task)

            tests_url = ARTIFACT_URL.format(
                build_parameters['build_slugid'],
                build_task['task']['extra']['locations']['tests']
            )

            build_url = ARTIFACT_URL.format(
                build_parameters['build_slugid'],
                build_task['task']['extra']['locations']['build']
            )

            define_task = DEFINE_TASK.format(build_task['task']['workerType'])

            graph['scopes'].append(define_task)
            graph['scopes'].extend(build_task['task'].get('scopes', []))

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

            for test in build['dependents']:
                test = test['allowed_build_tasks'][build['task']]
                test_parameters = copy.copy(build_parameters)
                test_parameters['build_url'] = build_url
                test_parameters['tests_url'] = tests_url
                test_parameters['total_chunks'] = 1

                if 'chunks' in test:
                    test_parameters['total_chunks'] = test['chunks']

                for chunk in range(1, test_parameters['total_chunks'] + 1):
                    if 'only_chunks' in test and \
                        chunk not in test['only_chunks']:
                        continue;

                    test_parameters['chunk'] = chunk
                    test_task = templates.load(test['task'], test_parameters)
                    test_task['taskId'] = slugid()

                    if 'requires' not in test_task:
                        test_task['requires'] = []

                    test_task['requires'].append(test_parameters['build_slugid'])

                    if 'treeherder' not in test_task['task']['extra']:
                        test_task['task']['extra']['treeherder'] = {}

                    # Copy over any treeherder configuration from the build so
                    # tests show up under the same platform...
                    test_treeherder_config = test_task['task']['extra']['treeherder']

                    test_treeherder_config['collection'] = \
                        build_treeherder_config.get('collection', {})

                    test_treeherder_config['build'] = \
                        build_treeherder_config.get('build', {})

                    test_treeherder_config['machine'] = \
                        build_treeherder_config.get('machine', {})

                    if 'routes' not in test_task['task']:
                        test_task['task']['routes'] = []

                    if 'scopes' not in test_task['task']:
                        test_task['task']['scopes'] = []

                    if params['revision_hash']:
                        test_task['task']['routes'].append(treeherder_route)
                        test_task['task']['scopes'].append('queue:route:{}'.format(treeherder_route))

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
    @CommandArgument('--mozharness-repository',
        default='http://hg.mozilla.org/build/mozharness',
        help='URL for custom mozharness repo')
    @CommandArgument('--head-repository',
        required=True,
        help='URL for "head" repository to fetch revision from')
    @CommandArgument('--head-ref',
        help='Reference (this is same as rev usually for hg)')
    @CommandArgument('--head-rev',
        required=True,
        help='Commit revision to use')
    @CommandArgument('--mozharness-rev',
        default='tip',
        help='Commit revision to use from mozharness repository')
    @CommandArgument('--owner',
        required=True,
        help='email address of who owns this graph')
    @CommandArgument('build_task',
        help='path to build task definition')
    def create_ci_build(self, **params):
        templates = Templates(ROOT)
        # TODO handle git repos
        head_repository = params['head_repository']
        if not head_repository:
            head_repository = get_hg_url()

        head_rev = params['head_rev']
        if not head_rev:
            head_rev = get_latest_hg_revision(head_repository)

        head_ref = params['head_ref'] or head_rev

        build_parameters = {
            'docker_image': docker_image,
            'owner': params['owner'],
            'from_now': json_time_from_now,
            'now': current_json_time(),
            'base_repository': params['base_repository'] or head_repository,
            'head_repository': head_repository,
            'head_rev': head_rev,
            'head_ref': head_ref,
            'mozharness_repository': params['mozharness_repository'],
            'mozharness_rev': params['mozharness_rev']
        }

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

@CommandProvider
class CITest(object):
    @Command('taskcluster-test', category='ci',
        description='Create taskcluster try server test task')
    @CommandArgument('--task-id',
        help='the task id to pick the correct build and tests')
    @CommandArgument('--total-chunks', type=int,
        help='total number of chunks')
    @CommandArgument('--chunk', type=int,
        help='current chunk')
    @CommandArgument('--owner',
        help='email address of who owns this graph')
    @CommandArgument('test_task',
        help='path to the test task definition')
    def create_ci_test(self, test_task, task_id='', total_chunks=1, chunk=1, owner=''):
        if total_chunks is None:
            total_chunks = 1

        if chunk is None:
            chunk = 1

        if chunk < 1 or chunk > total_chunks:
            raise ValueError(
                '"chunk" must be a value between 1 and "total_chunks (default 1)"')

        build_url, tests_url = self._get_build_and_tests_url(task_id)

        test_parameters = {
            'docker_image': docker_image,
            'build_url': ARTIFACT_URL.format(task_id, build_url),
            'tests_url': ARTIFACT_URL.format(task_id, tests_url),
            'total_chunks': total_chunks,
            'chunk': chunk,
            'owner': owner,
            'from_now': json_time_from_now,
            'now': current_json_time()
        }

        try:
            test_task = import_yaml(test_task, test_parameters)
        except IOError:
            sys.stderr.write(
                "Could not load test task file.  Ensure path is a relative " \
                "path from testing/taskcluster"
            )
            sys.exit(1)

        print(json.dumps(test_task['task'], indent=4))

    def _get_build_and_tests_url(self, task_id):
        task = get_task(task_id)
        locations = task['extra']['locations']
        return locations['build'], locations['tests']

@CommandProvider
class CIDockerRun(object):
    @Command('taskcluster-docker-run', category='ci',
        description='Run a docker image and optionally mount local hg repos. ' \
                    'Repos will be mounted to /home/worker/x/source accordingly. ' \
                    'For example, to run a centos image and mount local gecko ' \
                    'and gaia repos: mach ci-docker-run --local-gecko-repo ' \
                    '/home/user/mozilla-central/ --local-gaia-repo /home/user/gaia/ '\
                    '--docker-flags="-t -i" centos:centos7 /bin/bash')
    @CommandArgument('--local-gecko-repo',
        action='store', dest='local_gecko_repo',
        help='local gecko hg repository for volume mount')
    @CommandArgument('--gecko-revision',
        action='store', dest='gecko_revision',
        help='local gecko repo revision (defaults to latest)')
    @CommandArgument('--local-gaia-repo',
        action='store', dest='local_gaia_repo',
        help='local gaia hg repository for volume mount')
    @CommandArgument('--mozconfig',
        help='The mozconfig file for building gecko')
    @CommandArgument('--docker-flags',
        action='store', dest='flags',
        help='string of run flags (i.e. --docker-flags="-i -t")')
    @CommandArgument('image',
        help='name of docker image to run')
    @CommandArgument('command',
        nargs='*',
        help='command to run inside the docker image')
    def ci_docker_run(self, local_gecko_repo='', gecko_revision='',
                      local_gaia_repo='', mozconfig="", flags="", **kwargs):
        ''' Run docker image and optionally volume mount specified local repos '''
        gecko_mount_point='/home/worker/mozilla-central/source/'
        gaia_mount_point='/home/worker/gaia/source/'
        cmd_out = ['docker', 'run']
        if flags:
            cmd_out.extend(flags.split())
        if local_gecko_repo:
            if not os.path.exists(local_gecko_repo):
                print("Gecko repository path doesn't exist: %s" % local_gecko_repo)
                sys.exit(1)
            if not gecko_revision:
                gecko_revision = get_latest_hg_revision(local_gecko_repo)
            cmd_out.extend(['-v', '%s:%s' % (local_gecko_repo, gecko_mount_point)])
            cmd_out.extend(['-e', 'REPOSITORY=%s' % gecko_mount_point])
            cmd_out.extend(['-e', 'REVISION=%s' % gecko_revision])
        if local_gaia_repo:
            if not os.path.exists(local_gaia_repo):
                print("Gaia repository path doesn't exist: %s" % local_gaia_repo)
                sys.exit(1)
            cmd_out.extend(['-v', '%s:%s' % (local_gaia_repo, gaia_mount_point)])
            cmd_out.extend(['-e', 'GAIA_REPOSITORY=%s' % gaia_mount_point])
        if mozconfig:
            cmd_out.extend(['-e', 'MOZCONFIG=%s' % mozconfig])
        cmd_out.append(kwargs['image'])
        for cmd_x in kwargs['command']:
            cmd_out.append(cmd_x)
        try:
            subprocess.check_call(cmd_out)
        except subprocess.CalledProcessError:
            sys.stderr.write("Docker run command returned non-zero status. Attempted:\n")
            cmd_line = ''
            for x in cmd_out:
                cmd_line = cmd_line + x + ' '
            sys.stderr.write(cmd_line + '\n')
            sys.exit(1)
