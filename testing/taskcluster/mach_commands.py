# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from collections import defaultdict
import os
import json
import copy
import re
import sys
import time
from collections import namedtuple

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


ROOT = os.path.dirname(os.path.realpath(__file__))
GECKO = os.path.realpath(os.path.join(ROOT, '..', '..'))

# XXX: If/when we have the taskcluster queue use construct url instead
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'

DEFINE_TASK = 'queue:define-task:aws-provisioner-v1/{}'

DEFAULT_TRY = 'try: -b do -p all -u all'
DEFAULT_JOB_PATH = os.path.join(
    ROOT, 'tasks', 'branches', 'base_jobs.yml'
)

# time after which a try build's results will expire
TRY_EXPIRATION = "14 days"

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
        from taskcluster_graph.mach_util import gaia_info
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
class LoadImage(object):
    @Command('taskcluster-load-image', category="ci",
        description="Load a pre-built Docker image")
    @CommandArgument('--task-id',
        help="Load the image at public/image.tar in this task, rather than "
             "searching the index")
    @CommandArgument('image_name', nargs='?',
        help="Load the image of this name based on the current contents of the tree "
             "(as built for mozilla-central or mozilla-inbound)")
    def load_image(self, image_name, task_id):
        from taskcluster_graph.image_builder import (
            task_id_for_image,
            docker_load_from_url
        )

        if not image_name and not task_id:
            print("Specify either IMAGE-NAME or TASK-ID")
            sys.exit(1)

        if not task_id:
            task_id = task_id_for_image({}, 'mozilla-inbound', image_name, create=False)
            if not task_id:
                print("No task found in the TaskCluster index for {}".format(image_name))
                sys.exit(1)

        print("Task ID: {}".format(task_id))

        ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
        image_name = docker_load_from_url(ARTIFACT_URL.format(task_id, 'public/image.tar'))

        print("Loaded image is named {}".format(image_name))


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
    @CommandArgument('--level',
        default="1",
        help='SCM level of this repository')
    @CommandArgument('--extend-graph',
        action="store_true", dest="ci", help='Omit create graph arguments')
    @CommandArgument('--interactive',
        required=False,
        default=False,
        action="store_true",
        dest="interactive",
        help="Run the tasks with the interactive feature enabled")
    @CommandArgument('--print-names-only',
        action='store_true', default=False,
        help="Only print the names of each scheduled task, one per line.")
    @CommandArgument('--dry-run',
        action='store_true', default=False,
        help="Stub out taskIds and date fields from the task definitions.")
    @CommandArgument('--ignore-conditions',
        action='store_true',
        help='Run tasks even if their conditions are not met')
    def create_graph(self, **params):
        from functools import partial

        from mozpack.path import match as mozpackmatch

        from slugid import nice as slugid

        from taskcluster_graph.mach_util import (
            merge_dicts,
            gaia_info,
            configure_dependent_task,
            set_interactive_task,
            remove_caches_from_task,
            query_vcs_info
        )
        import taskcluster_graph.transform.routes as routes_transform
        import taskcluster_graph.transform.treeherder as treeherder_transform
        from taskcluster_graph.commit_parser import parse_commit
        from taskcluster_graph.image_builder import (
            docker_image,
            normalize_image_details,
            task_id_for_image
        )
        from taskcluster_graph.from_now import (
            json_time_from_now,
            current_json_time,
        )
        from taskcluster_graph.templates import Templates
        import taskcluster_graph.build_task

        if params['dry_run']:
            from taskcluster_graph.dry_run import (
                json_time_from_now,
                current_json_time,
                slugid,
            )

        project = params['project']
        message = params.get('message', '') if project == 'try' else DEFAULT_TRY

        templates = Templates(ROOT)

        job_path = os.path.join(ROOT, 'tasks', 'branches', project, 'job_flags.yml')
        job_path = job_path if os.path.exists(job_path) else DEFAULT_JOB_PATH

        jobs = templates.load(job_path, {})

        job_graph, trigger_tests = parse_commit(message, jobs)

        cmdline_interactive = params.get('interactive', False)

        # Default to current time if querying the head rev fails
        pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime())
        vcs_info = query_vcs_info(params['head_repository'], params['head_rev'])
        changed_files = set()
        if vcs_info:
            pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime(vcs_info.pushdate))

            sys.stderr.write('%d commits influencing task scheduling:\n' %
                             len(vcs_info.changesets))
            for c in vcs_info.changesets:
                sys.stderr.write('%s %s\n' % (
                    c['node'][0:12], c['desc'].splitlines()[0].encode('ascii', 'ignore')))

                changed_files |= set(c['files'])

        # Template parameters used when expanding the graph
        seen_images = {}
        parameters = dict(gaia_info().items() + {
            'index': 'index',
            'project': project,
            'pushlog_id': params.get('pushlog_id', 0),
            'docker_image': docker_image,
            'task_id_for_image': partial(task_id_for_image, seen_images, project),
            'base_repository': params['base_repository'] or \
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

        routes_file = os.path.join(ROOT, 'routes.json')
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
            for env in routes_transform.TREEHERDER_ROUTES:
                route = 'queue:route:{}.{}'.format(
                            routes_transform.TREEHERDER_ROUTES[env],
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

            # Command line override to not filter.
            if params['ignore_conditions']:
                return True

            when = task['when']

            # If the task defines file patterns and we have a set of changed
            # files to compare against, only run if a file pattern matches one
            # of the changed files.
            file_patterns = when.get('file_patterns', None)
            if file_patterns and changed_files:
                for pattern in file_patterns:
                    for path in changed_files:
                        if mozpackmatch(path, pattern):
                            sys.stderr.write('scheduling %s because pattern %s '
                                             'matches %s\n' % (task['task'],
                                                               pattern,
                                                               path))
                            return True

                # No file patterns matched. Discard task.
                sys.stderr.write('discarding %s because no relevant files changed\n' %
                                 task['task'])
                return False

            return True

        job_graph = filter(should_run, job_graph)

        all_routes = {}

        for build in job_graph:
            interactive = cmdline_interactive or build["interactive"]
            build_parameters = merge_dicts(parameters, build['additional-parameters']);
            build_parameters['build_slugid'] = slugid()
            build_parameters['source'] = '{repo}file/{rev}/testing/taskcluster/{file}'.format(repo=params['head_repository'], rev=params['head_rev'], file=build['task'])
            build_task = templates.load(build['task'], build_parameters)

            # Copy build_* attributes to expose them to post-build tasks
            # as well as json routes and tests
            task_extra = build_task['task']['extra']
            build_parameters['build_name'] = task_extra['build_name']
            build_parameters['build_type'] = task_extra['build_type']
            build_parameters['build_product'] = task_extra['build_product']

            normalize_image_details(graph,
                                    build_task,
                                    seen_images,
                                    build_parameters,
                                    os.environ.get('TASK_ID', None))
            set_interactive_task(build_task, interactive)

            # try builds don't use cache
            if project == "try":
                remove_caches_from_task(build_task)
                set_expiration(build_task, json_time_from_now(TRY_EXPIRATION))

            if params['revision_hash']:
                treeherder_transform.add_treeherder_revision_info(build_task['task'],
                                                                  params['head_rev'],
                                                                  params['revision_hash'])
                routes_transform.decorate_task_treeherder_routes(build_task['task'],
                                                                 treeherder_route)
                routes_transform.decorate_task_json_routes(build_task['task'],
                                                           json_routes,
                                                           build_parameters)

            # Ensure each build graph is valid after construction.
            taskcluster_graph.build_task.validate(build_task)
            graph['tasks'].append(build_task)

            for location in build_task['task']['extra'].get('locations', {}):
                build_parameters['{}_url'.format(location)] = ARTIFACT_URL.format(
                    build_parameters['build_slugid'],
                    build_task['task']['extra']['locations'][location]
                )

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
                build_treeherder_config['collection'] = { 'opt': True }

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
                                                     slugid(),
                                                     templates,
                                                     build_treeherder_config)
                normalize_image_details(graph,
                                        post_task,
                                        seen_images,
                                        build_parameters,
                                        os.environ.get('TASK_ID', None))
                set_interactive_task(post_task, interactive)
                treeherder_transform.add_treeherder_revision_info(post_task['task'],
                                                                  params['head_rev'],
                                                                  params['revision_hash'])
                if project == "try":
                    set_expiration(post_task, json_time_from_now(TRY_EXPIRATION))
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
                                                         slugid(),
                                                         templates,
                                                         build_treeherder_config)
                    normalize_image_details(graph,
                                            test_task,
                                            seen_images,
                                            build_parameters,
                                            os.environ.get('TASK_ID', None))
                    set_interactive_task(test_task, interactive)

                    if params['revision_hash']:
                        treeherder_transform.add_treeherder_revision_info(test_task['task'],
                                                                          params['head_rev'],
                                                                          params['revision_hash'])
                        routes_transform.decorate_task_treeherder_routes(
                            test_task['task'],
                            treeherder_route
                        )

                    if project == "try":
                        set_expiration(test_task, json_time_from_now(TRY_EXPIRATION))

                    # This will schedule test jobs N times
                    for i in range(0, trigger_tests):
                        graph['tasks'].append(test_task)
                        # If we're scheduling more tasks each have to be unique
                        test_task = copy.deepcopy(test_task)
                        test_task['taskId'] = slugid()

                    define_task = DEFINE_TASK.format(
                        test_task['task']['workerType']
                    )

                    graph['scopes'].add(define_task)
                    graph['scopes'] |= set(test_task['task'].get('scopes', []))

        graph['scopes'] = sorted(graph['scopes'])

        if params['print_names_only']:
            tIDs = defaultdict(list)

            def print_task(task, indent=0):
                print('{}- {}'.format(' ' * indent, task['task']['metadata']['name']))

                for child in tIDs[task['taskId']]:
                    print_task(child, indent=indent+2)

            # build a dependency map
            for task in graph['tasks']:
                if 'requires' in task:
                    for tID in task['requires']:
                        tIDs[tID].append(task)

            # recursively print root tasks
            for task in graph['tasks']:
                if 'requires' not in task:
                    print_task(task)
            return

        # When we are extending the graph remove extra fields...
        if params['ci'] is True:
            graph.pop('scopes', None)
            graph.pop('metadata', None)

        print(json.dumps(graph, indent=4, sort_keys=True))

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
    @CommandArgument('--owner',
        default='foobar@mozilla.com',
        help='email address of who owns this graph')
    @CommandArgument('--level',
        default="1",
        help='SCM level of this repository')
    @CommandArgument('build_task',
        help='path to build task definition')
    @CommandArgument('--interactive',
        required=False,
        default=False,
        action="store_true",
        dest="interactive",
        help="Run the task with the interactive feature enabled")
    def create_ci_build(self, **params):
        from taskcluster_graph.mach_util import (
            gaia_info,
            set_interactive_task,
            query_vcs_info
        )
        from taskcluster_graph.templates import Templates
        from taskcluster_graph.image_builder import docker_image
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

        # Default to current time if querying the head rev fails
        pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime())
        vcs_info = query_vcs_info(params['head_repository'], params['head_rev'])
        if vcs_info:
            pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime(vcs_info.pushdate))

        from taskcluster_graph.from_now import (
            json_time_from_now,
            current_json_time,
        )
        build_parameters = dict(gaia_info().items() + {
            'docker_image': docker_image,
            'owner': params['owner'],
            'level': params['level'],
            'from_now': json_time_from_now,
            'now': current_json_time(),
            'base_repository': params['base_repository'] or head_repository,
            'head_repository': head_repository,
            'head_rev': head_rev,
            'head_ref': head_ref,
            'pushdate': pushdate,
            'pushtime': pushdate[8:],
            'year': pushdate[0:4],
            'month': pushdate[4:6],
            'day': pushdate[6:8],
        }.items())

        try:
            build_task = templates.load(params['build_task'], build_parameters)
            set_interactive_task(build_task, params.get('interactive', False))
        except IOError:
            sys.stderr.write(
                "Could not load build task file.  Ensure path is a relative " \
                "path from testing/taskcluster"
            )
            sys.exit(1)

        taskcluster_graph.build_task.validate(build_task)

        print(json.dumps(build_task['task'], indent=4))
