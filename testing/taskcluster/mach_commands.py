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

import pystache
import yaml

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from taskcluster_graph.commit_parser import parse_commit
from taskcluster_graph.slugid import slugid
from taskcluster_graph.from_now import json_time_from_now, current_json_time

import taskcluster_graph.build_task

ROOT = os.path.dirname(os.path.realpath(__file__))
DOCKER_ROOT = os.path.join(ROOT, '..', 'docker')
LOCAL_WORKER_TYPES = ['b2gtest', 'b2gbuild']

# XXX: If/when we have the taskcluster queue use construct url instead
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
REGISTRY = open(os.path.join(DOCKER_ROOT, 'REGISTRY')).read().strip()

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

def import_yaml(path, variables=None):
    ''' Load a yml file relative to the root of this file'''
    content = open(os.path.join(ROOT, path)).read()
    if variables is not None:
        content = pystache.render(content, variables)
    task = yaml.load(content)
    return task

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

@CommandProvider
class TryGraph(object):
    @Command('trygraph', category="ci",
        description="Create taskcluster try server graph")
    @CommandArgument('--revision',
        help='revision in gecko to use in sub tasks')
    @CommandArgument('--message',
        required=True,
        help='Commit message to be parsed')
    @CommandArgument('--repository',
        help='full path to hg repository to use in sub tasks')
    @CommandArgument('--owner',
        help='email address of who owns this graph')
    @CommandArgument('--extend-graph',
        action="store_true", dest="ci", help='Omit create graph arguments')
    def create_graph(self, revision="", message="", repository="", owner="",
            ci=False):
        """ Create the taskcluster graph from the try commit message.

        :param args: commit message (ex: "– try: -b o -p linux64_gecko -u gaia-unit -t none")
        """
        jobs = import_yaml('job_flags.yml')
        job_graph = parse_commit(message, jobs)

        # Template parameters used when expanding the graph
        parameters = {
            'docker_image': docker_image,
            'repository': repository,
            'revision': revision,
            'owner': owner,
            'from_now': json_time_from_now,
            'now': datetime.datetime.now().isoformat()
        }

        # Task graph we are generating for taskcluster...
        graph = {
            'tasks': []
        }

        if ci is False:
            # XXX: We need to figure out a less ugly way to store these for
            # local testing.
            graph['scopes'] = [
                "docker-worker:cache:sources-mozilla-central",
                "docker-worker:cache:sources-gaia",
                "docker-worker:cache:build-b2g-desktop-objects"
            ]

            # XXX: This is a hack figure out how to do this correctly or sanely
            # at least so we don't need to keep track of all worker types in
            # existence.
            for worker_type in LOCAL_WORKER_TYPES:
                graph['scopes'].append(
                    'queue:define-task:{}/{}'.format('aws-provisioner',
                        worker_type)
                )

                graph['scopes'].append(
                    'queue:create-task:{}/{}'.format('aws-provisioner',
                        worker_type)
                )

            graph['metadata'] = {
                'source': 'http://todo.com/what/goes/here',
                'owner': owner,
                # TODO: Add full mach commands to this example?
                'description': 'Try task graph generated via ./mach trygraph',
                'name': 'trygraph local'
            }

        for build in job_graph:
            build_parameters = copy.copy(parameters)
            build_parameters['build_slugid'] = slugid()
            build_task = import_yaml(build['task'], build_parameters)

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

            for test in build['dependents']:
                test_parameters = copy.copy(build_parameters)
                test_parameters['build_url'] = build_url
                test_parameters['tests_url'] = tests_url
                test_parameters['total_chunks'] = 1

                if 'chunks' in test:
                    test_parameters['total_chunks'] = test['chunks']

                for chunk in range(1, test_parameters['total_chunks'] + 1):
                    test_parameters['chunk'] = chunk
                    test_task = import_yaml(test['task'], test_parameters)
                    test_task['taskId'] = slugid()

                    if 'requires' not in test_task:
                        test_task['requires'] = []

                    test_task['requires'].append(test_parameters['build_slugid'])
                    graph['tasks'].append(test_task)

        print(json.dumps(graph, indent=4))

@CommandProvider
class CIBuild(object):
    @Command('ci-build', category='ci',
        description="Create taskcluster try server build task")
    @CommandArgument('--revision',
        help='revision in gecko to use in sub tasks')
    @CommandArgument('--repository',
        help='full path to hg repository to use in sub tasks')
    @CommandArgument('--owner',
        help='email address of who owns this graph')
    @CommandArgument('build_task',
        help='path to build task definition')
    def create_ci_build(self, build_task, revision="", repository="", owner=""):
        # TODO handle git repos
        if not repository:
            repository = get_hg_url()

        if not revision:
            revision = get_latest_hg_revision(repository)

        build_parameters = {
            'docker_image': docker_image,
            'repository': repository,
            'revision': revision,
            'owner': owner,
            'from_now': json_time_from_now,
            'now': current_json_time()
        }

        try:
            build_task = import_yaml(build_task, build_parameters)
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
    @Command('ci-test', category='ci',
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
    @Command('ci-docker-run', category='ci',
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
