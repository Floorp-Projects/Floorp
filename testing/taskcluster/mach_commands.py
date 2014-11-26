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
            "Error: Could not determine the latest hg revision at {}" \
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
