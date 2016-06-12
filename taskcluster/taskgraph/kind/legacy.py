# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import copy
import json
import logging
import os
import re
import sys
import time
from collections import defaultdict, namedtuple

from . import base
from ..types import Task
from functools import partial
from mozpack.path import match as mozpackmatch
from slugid import nice as slugid
from taskgraph.util.legacy_commit_parser import parse_commit
from taskgraph.util.time import (
    json_time_from_now,
    current_json_time,
)
from taskgraph.util.templates import Templates
from taskgraph.util.docker import docker_image


ROOT = os.path.dirname(os.path.realpath(__file__))
GECKO = os.path.realpath(os.path.join(ROOT, '..', '..', '..'))
# TASKID_PLACEHOLDER is the "internal" form of a taskid; it is substituted with
# actual taskIds at the very last minute, in get_task_definition
TASKID_PLACEHOLDER = 'TaskLabel=={}'

DEFINE_TASK = 'queue:define-task:aws-provisioner-v1/{}'
DEFAULT_TRY = 'try: -b do -p all -u all -t all'
DEFAULT_JOB_PATH = os.path.join(
    'tasks', 'branches', 'base_jobs.yml'
)

TREEHERDER_ROUTE_PREFIX = 'tc-treeherder-stage'
TREEHERDER_ROUTES = {
    'staging': 'tc-treeherder-stage',
    'production': 'tc-treeherder'
}

# time after which a try build's results will expire
TRY_EXPIRATION = "14 days"

logger = logging.getLogger(__name__)

def mklabel():
    return TASKID_PLACEHOLDER.format(slugid())

def merge_dicts(*dicts):
    merged_dict = {}
    for dictionary in dicts:
        merged_dict.update(dictionary)
    return merged_dict

def gaia_info():
    '''Fetch details from in tree gaia.json (which links this version of
    gecko->gaia) and construct the usual base/head/ref/rev pairing...'''
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

def configure_dependent_task(task_path, parameters, taskid, templates, build_treeherder_config):
    """Configure a build dependent task. This is shared between post-build and test tasks.

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

def set_interactive_task(task, interactive):
    r"""Make the task interactive.

    :param task: task definition.
    :param interactive: True if the task should be interactive.
    """
    if not interactive:
        return

    payload = task["task"]["payload"]
    if "features" not in payload:
        payload["features"] = {}
    payload["features"]["interactive"] = True

def remove_caches_from_task(task):
    r"""Remove all caches but tc-vcs from the task.

    :param task: task definition.
    """
    whitelist = [
        re.compile("^level-[123]-.*-tc-vcs(-public-sources)?$"),
        re.compile("^tooltool-cache$"),
    ]
    try:
        caches = task["task"]["payload"]["cache"]
        for cache in caches.keys():
            if not any(pat.match(cache) for pat in whitelist):
                caches.pop(cache)
    except KeyError:
        pass

def query_vcs_info(repository, revision):
    """Query the pushdate and pushid of a repository/revision.

    This is intended to be used on hg.mozilla.org/mozilla-central and
    similar. It may or may not work for other hg repositories.
    """
    if not repository or not revision:
        logger.warning('cannot query vcs info because vcs info not provided')
        return None

    VCSInfo = namedtuple('VCSInfo', ['pushid', 'pushdate', 'changesets'])

    try:
        import requests
        url = '%s/json-automationrelevance/%s' % (repository.rstrip('/'),
                                                  revision)
        logger.debug("Querying version control for metadata: %s", url)
        contents = requests.get(url).json()

        changesets = []
        for c in contents['changesets']:
            changesets.append({k: c[k] for k in ('desc', 'files', 'node')})

        pushid = contents['changesets'][-1]['pushid']
        pushdate = contents['changesets'][-1]['pushdate'][0]

        return VCSInfo(pushid, pushdate, changesets)

    except Exception:
        logger.exception("Error querying VCS info for '%s' revision '%s'",
                repository, revision)
        return None


def set_expiration(task, timestamp):
    task_def = task['task']
    task_def['expires'] = timestamp
    if task_def.get('deadline', timestamp) > timestamp:
        task_def['deadline'] = timestamp

    try:
        artifacts = task_def['payload']['artifacts']
    except KeyError:
        return

    # for docker-worker, artifacts is a dictionary
    # for generic-worker, artifacts is a list
    # for taskcluster-worker, it will depend on what we do in artifacts plugin
    for artifact in artifacts.values() if hasattr(artifacts, "values") else artifacts:
        artifact['expires'] = timestamp

def add_treeherder_revision_info(task, revision, revision_hash):
    # Only add treeherder information if task.extra.treeherder is present
    if 'extra' not in task and 'treeherder' not in task.extra:
        return

    task['extra']['treeherder']['revision'] = revision
    task['extra']['treeherder']['revision_hash'] = revision_hash


def decorate_task_treeherder_routes(task, suffix):
    """Decorate the given task with treeherder routes.

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

def decorate_task_json_routes(task, json_routes, parameters):
    """Decorate the given task with routes.json routes.

    :param dict task: task definition.
    :param json_routes: the list of routes to use from routes.json
    :param parameters: dictionary of parameters to use in route templates
    """
    routes = task.get('routes', [])
    for route in json_routes:
        routes.append(route.format(**parameters))

    task['routes'] = routes

class BuildTaskValidationException(Exception):
    pass

def validate_build_task(task):
    '''The build tasks have some required fields in extra this function ensures
    they are there. '''
    if 'task' not in task:
        raise BuildTaskValidationException('must have task field')

    task_def = task['task']

    if 'extra' not in task_def:
        raise BuildTaskValidationException('build task must have task.extra props')

    if 'locations' in task_def['extra']:

        locations = task_def['extra']['locations']

        if 'build' not in locations:
            raise BuildTaskValidationException('task.extra.locations.build missing')

        if 'tests' not in locations and 'test_packages' not in locations:
            raise BuildTaskValidationException('task.extra.locations.tests or '
                                               'task.extra.locations.tests_packages missing')

class LegacyKind(base.Kind):
    """
    This kind generates a full task graph from the old YAML files in
    `testing/taskcluster/tasks`.  The tasks already have dependency links.

    The existing task-graph generation generates slugids for tasks during task
    generation, so this kind labels tasks using those slugids, with a prefix of
    "TaskLabel==".  These labels are unfortunately not stable from run to run.
    """

    def load_tasks(self, params):
        root = os.path.abspath(os.path.join(self.path, self.config['legacy_path']))

        project = params['project']
        # NOTE: message is ignored here; we always use DEFAULT_TRY, then filter the
        # resulting task graph later
        message = DEFAULT_TRY

        templates = Templates(root)

        job_path = os.path.join(root, 'tasks', 'branches', project, 'job_flags.yml')
        job_path = job_path if os.path.exists(job_path) else \
            os.path.join(root, DEFAULT_JOB_PATH)

        jobs = templates.load(job_path, {})

        job_graph, trigger_tests = parse_commit(message, jobs)

        cmdline_interactive = params.get('interactive', False)

        # Default to current time if querying the head rev fails
        pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime())
        vcs_info = query_vcs_info(params['head_repository'], params['head_rev'])
        changed_files = set()
        if vcs_info:
            pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime(vcs_info.pushdate))

            logger.debug('{} commits influencing task scheduling:'.format(len(vcs_info.changesets)))
            for c in vcs_info.changesets:
                logger.debug("{cset} {desc}".format(
                    cset=c['node'][0:12],
                    desc=c['desc'].splitlines()[0].encode('ascii', 'ignore')))
                changed_files |= set(c['files'])

        # Template parameters used when expanding the graph
        parameters = dict(gaia_info().items() + {
            'index': 'index',
            'project': project,
            'pushlog_id': params.get('pushlog_id', 0),
            'docker_image': docker_image,
            'base_repository': params['base_repository'] or
            params['head_repository'],
            'head_repository': params['head_repository'],
            'head_ref': params['head_ref'] or params['head_rev'],
            'head_rev': params['head_rev'],
            'pushdate': pushdate,
            'pushtime': pushdate[8:],
            'year': pushdate[0:4],
            'month': pushdate[4:6],
            'day': pushdate[6:8],
            'owner': params['owner'],
            'level': params['level'],
            'from_now': json_time_from_now,
            'now': current_json_time(),
            'revision_hash': params['revision_hash']
        }.items())

        treeherder_route = '{}.{}'.format(
            params['project'],
            params.get('revision_hash', '')
        )

        routes_file = os.path.join(root, 'routes.json')
        with open(routes_file) as f:
            contents = json.load(f)
            json_routes = contents['routes']
            # TODO: Nightly and/or l10n routes

        # Task graph we are generating for taskcluster...
        graph = {
            'tasks': [],
            'scopes': set(),
        }

        if params['revision_hash']:
            for env in TREEHERDER_ROUTES:
                route = 'queue:route:{}.{}'.format(
                    TREEHERDER_ROUTES[env],
                    treeherder_route)
                graph['scopes'].add(route)

        graph['metadata'] = {
            'source': '{repo}file/{rev}/testing/taskcluster/mach_commands.py'.format(repo=params['head_repository'], rev=params['head_rev']),
            'owner': params['owner'],
            # TODO: Add full mach commands to this example?
            'description': 'Task graph generated via ./mach taskcluster-graph',
            'name': 'task graph local'
        }

        # Filter the job graph according to conditions met by this invocation run.
        def should_run(task):
            # Old style build or test task that doesn't define conditions. Always runs.
            if 'when' not in task:
                return True

            when = task['when']

            # If the task defines file patterns and we have a set of changed
            # files to compare against, only run if a file pattern matches one
            # of the changed files.
            file_patterns = when.get('file_patterns', None)
            if file_patterns and changed_files:
                # Always consider changes to the task definition itself
                file_patterns.append('testing/taskcluster/{task}'.format(task=task['task']))
                for pattern in file_patterns:
                    for path in changed_files:
                        if mozpackmatch(path, pattern):
                            logger.debug('scheduling {task} because pattern {pattern} '
                                         'matches {path}'.format(
                                             task=task['task'],
                                             pattern=pattern,
                                             path=path,
                            ))
                            return True

                # No file patterns matched. Discard task.
                logger.debug('discarding {task} because no relevant files changed'.format(
                    task=task['task'],
                    pattern=pattern,
                    path=path))
                return False

            return True

        job_graph = filter(should_run, job_graph)

        all_routes = {}

        for build in job_graph:
            logging.debug("loading build task {}".format(build['task']))
            interactive = cmdline_interactive or build["interactive"]
            build_parameters = merge_dicts(parameters, build['additional-parameters'])
            build_parameters['build_slugid'] = mklabel()
            build_parameters['source'] = '{repo}file/{rev}/testing/taskcluster/{file}'.format(repo=params['head_repository'], rev=params['head_rev'], file=build['task'])
            build_task = templates.load(build['task'], build_parameters)

            # Copy build_* attributes to expose them to post-build tasks
            # as well as json routes and tests
            task_extra = build_task['task']['extra']
            build_parameters['build_name'] = task_extra['build_name']
            build_parameters['build_type'] = task_extra['build_type']
            build_parameters['build_product'] = task_extra['build_product']

            set_interactive_task(build_task, interactive)

            # try builds don't use cache
            if project == "try":
                remove_caches_from_task(build_task)
                set_expiration(build_task, json_time_from_now(TRY_EXPIRATION))

            if params['revision_hash']:
                add_treeherder_revision_info(build_task['task'],
                                             params['head_rev'],
                                             params['revision_hash'])
                decorate_task_treeherder_routes(build_task['task'],
                                                treeherder_route)
                decorate_task_json_routes(build_task['task'],
                                          json_routes,
                                          build_parameters)

            # Ensure each build graph is valid after construction.
            validate_build_task(build_task)
            attributes = build_task['attributes'] = {'kind':'legacy', 'legacy_kind': 'build'}
            if 'build_name' in build:
                attributes['build_platform'] = build['build_name']
            if 'build_type' in task_extra:
                attributes['build_type'] = {'dbg': 'debug'}.get(task_extra['build_type'],
                                                                task_extra['build_type'])
            if build.get('is_job'):
                attributes['job'] = build['build_name']
                attributes['legacy_kind'] = 'job'
            graph['tasks'].append(build_task)

            for location in build_task['task']['extra'].get('locations', {}):
                build_parameters['{}_location'.format(location)] = build_task['task']['extra']['locations'][location]

            for url in build_task['task']['extra'].get('url', {}):
                build_parameters['{}_url'.format(url)] = \
                    build_task['task']['extra']['url'][url]

            define_task = DEFINE_TASK.format(build_task['task']['workerType'])

            for route in build_task['task'].get('routes', []):
                if route.startswith('index.gecko.v2') and route in all_routes:
                    raise Exception("Error: route '%s' is in use by multiple tasks: '%s' and '%s'" % (
                        route,
                        build_task['task']['metadata']['name'],
                        all_routes[route],
                    ))
                all_routes[route] = build_task['task']['metadata']['name']

            graph['scopes'].add(define_task)
            graph['scopes'] |= set(build_task['task'].get('scopes', []))
            route_scopes = map(lambda route: 'queue:route:' + route, build_task['task'].get('routes', []))
            graph['scopes'] |= set(route_scopes)

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
                build_treeherder_config['collection'] = {'opt': True}

            if len(build_treeherder_config['collection'].keys()) != 1:
                message = '({}), extra.treeherder.collection must contain one type'
                raise ValueError(message.fomrat(build['task']))

            for post_build in build['post-build']:
                # copy over the old parameters to update the template
                # TODO additional-parameters is currently not an option, only
                # enabled for build tasks
                post_parameters = merge_dicts(build_parameters,
                                              post_build.get('additional-parameters', {}))
                post_task = configure_dependent_task(post_build['task'],
                                                     post_parameters,
                                                     mklabel(),
                                                     templates,
                                                     build_treeherder_config)
                set_interactive_task(post_task, interactive)
                add_treeherder_revision_info(post_task['task'],
                                             params['head_rev'],
                                             params['revision_hash'])

                if project == "try":
                    set_expiration(post_task, json_time_from_now(TRY_EXPIRATION))

                post_task['attributes'] = attributes.copy()
                post_task['attributes']['legacy_kind'] = 'post_build'
                post_task['attributes']['post_build'] = post_build['job_flag']
                graph['tasks'].append(post_task)

            for test in build['dependents']:
                test = test['allowed_build_tasks'][build['task']]
                # TODO additional-parameters is currently not an option, only
                # enabled for build tasks
                test_parameters = merge_dicts(build_parameters,
                                              test.get('additional-parameters', {}))
                test_parameters = copy.copy(build_parameters)

                test_definition = templates.load(test['task'], {})['task']
                chunk_config = test_definition['extra'].get('chunks', {})

                # Allow branch configs to override task level chunking...
                if 'chunks' in test:
                    chunk_config['total'] = test['chunks']

                chunked = 'total' in chunk_config
                if chunked:
                    test_parameters['total_chunks'] = chunk_config['total']

                if 'suite' in test_definition['extra']:
                    suite_config = test_definition['extra']['suite']
                    test_parameters['suite'] = suite_config['name']
                    test_parameters['flavor'] = suite_config.get('flavor', '')

                for chunk in range(1, chunk_config.get('total', 1) + 1):
                    if 'only_chunks' in test and chunked and \
                            chunk not in test['only_chunks']:
                        continue

                    if chunked:
                        test_parameters['chunk'] = chunk
                    test_task = configure_dependent_task(test['task'],
                                                         test_parameters,
                                                         mklabel(),
                                                         templates,
                                                         build_treeherder_config)
                    set_interactive_task(test_task, interactive)

                    if params['revision_hash']:
                        add_treeherder_revision_info(test_task['task'],
                                                     params['head_rev'],
                                                     params['revision_hash'])
                        decorate_task_treeherder_routes(
                            test_task['task'],
                            treeherder_route
                        )

                    if project == "try":
                        set_expiration(test_task, json_time_from_now(TRY_EXPIRATION))

                    test_task['attributes'] = attributes.copy()
                    test_task['attributes']['legacy_kind'] = 'unittest'
                    test_task['attributes']['test_platform'] = attributes['build_platform']
                    test_task['attributes']['unittest_try_name'] = test['unittest_try_name']
                    for param, attr in [
                            ('suite', 'unittest_suite'),
                            ('flavor', 'unittest_flavor'),
                            ('chunk', 'test_chunk')]:
                        if param in test_parameters:
                            test_task['attributes'][attr] = str(test_parameters[param])

                    # This will schedule test jobs N times
                    for i in range(0, trigger_tests):
                        graph['tasks'].append(test_task)
                        # If we're scheduling more tasks each have to be unique
                        test_task = copy.deepcopy(test_task)
                        test_task['taskId'] = mklabel()

                    define_task = DEFINE_TASK.format(
                        test_task['task']['workerType']
                    )

                    graph['scopes'].add(define_task)
                    graph['scopes'] |= set(test_task['task'].get('scopes', []))

        graph['scopes'] = sorted(graph['scopes'])

        # save the graph for later, when taskgraph asks for additional information
        # such as dependencies
        self.graph = graph
        self.tasks_by_label = {t['taskId']: t for t in self.graph['tasks']}

        # Convert to a dictionary of tasks.  The process above has invented a
        # taskId for each task, and we use those as the *labels* for the tasks;
        # taskgraph will later assign them new taskIds.
        return [Task(self, t['taskId'], task=t['task'], attributes=t['attributes'])
                for t in self.graph['tasks']]

    def get_task_dependencies(self, task, taskgraph):
        # fetch dependency information from the cached graph
        taskdict = self.tasks_by_label[task.label]
        deps = [(label, label) for label in taskdict.get('requires', [])]

        # add a dependency on an image task, if needed
        if 'docker-image' in taskdict:
            deps.append(('build-docker-image-{docker-image}'.format(**taskdict), 'docker-image'))

        return deps

    def optimize_task(self, task, taskgraph):
        # no optimization for the moment
        return False, None

