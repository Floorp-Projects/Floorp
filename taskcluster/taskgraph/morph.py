# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Graph morphs are modifications to task-graphs that take place *after* the
optimization phase.

These graph morphs are largely invisible to developers running `./mach`
locally, so they should be limited to changes that do not modify the meaning of
the graph.
"""

# Note that the translation of `{'task-reference': '..'}` is handled in the
# optimization phase (since optimization involves dealing with taskIds
# directly).  Similarly, `{'relative-datestamp': '..'}` is handled at the last
# possible moment during task creation.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os
import re

import jsone
import yaml
from slugid import nice as slugid
from .task import Task
from .graph import Graph
from .taskgraph import TaskGraph

here = os.path.abspath(os.path.dirname(__file__))
logger = logging.getLogger(__name__)
MAX_ROUTES = 10


def amend_taskgraph(taskgraph, label_to_taskid, to_add):
    """Add the given tasks to the taskgraph, returning a new taskgraph"""
    new_tasks = taskgraph.tasks.copy()
    new_edges = taskgraph.graph.edges.copy()
    for task in to_add:
        new_tasks[task.task_id] = task
        assert task.label not in label_to_taskid
        label_to_taskid[task.label] = task.task_id
        for depname, dep in task.dependencies.iteritems():
            new_edges.add((task.task_id, dep, depname))

    taskgraph = TaskGraph(new_tasks, Graph(set(new_tasks), new_edges))
    return taskgraph, label_to_taskid


def derive_misc_task(task, purpose, image, taskgraph, label_to_taskid):
    """Create the shell of a task that depends on `task` and on the given docker
    image."""
    label = '{}-{}'.format(purpose, task.label)

    # this is why all docker image tasks are included in the target task graph: we
    # need to find them in label_to_taskid, if if nothing else required them
    image_taskid = label_to_taskid['build-docker-image-' + image]

    task_def = {
        'provisionerId': 'aws-provisioner-v1',
        'workerType': 'gecko-misc',
        'dependencies': [task.task_id, image_taskid],
        'created': {'relative-datestamp': '0 seconds'},
        'deadline': task.task['deadline'],
        # no point existing past the parent task's deadline
        'expires': task.task['deadline'],
        'metadata': {
            'name': label,
            'description': '{} for {}'.format(purpose, task.task['metadata']['description']),
            'owner': task.task['metadata']['owner'],
            'source': task.task['metadata']['source'],
        },
        'scopes': [],
        'payload': {
            'image': {
                'path': 'public/image.tar.zst',
                'taskId': image_taskid,
                'type': 'task-image',
            },
            'features': {
                'taskclusterProxy': True,
            },
            'maxRunTime': 600,
        }
    }

    # only include the docker-image dependency here if it is actually in the
    # taskgraph (has not been optimized).  It is included in
    # task_def['dependencies'] unconditionally.
    dependencies = {'parent': task.task_id}
    if image_taskid in taskgraph.tasks:
        dependencies['docker-image'] = image_taskid

    task = Task(kind='misc', label=label, attributes={}, task=task_def,
                dependencies=dependencies)
    task.task_id = slugid()
    return task


# these regular expressions capture route prefixes for which we have a star
# scope, allowing them to be summarized.  Each should correspond to a star scope
# in each Gecko `assume:repo:hg.mozilla.org/...` role.
SCOPE_SUMMARY_REGEXPS = [
    re.compile(r'(index:insert-task:docker\.images\.v1\.[^.]*\.).*'),
    re.compile(r'(index:insert-task:gecko\.v2\.[^.]*\.).*'),
]


def make_index_task(parent_task, taskgraph, label_to_taskid):
    index_paths = [r.split('.', 1)[1] for r in parent_task.task['routes']
                   if r.startswith('index.')]
    parent_task.task['routes'] = [r for r in parent_task.task['routes']
                                  if not r.startswith('index.')]

    task = derive_misc_task(parent_task, 'index-task', 'index-task',
                            taskgraph, label_to_taskid)

    # we need to "summarize" the scopes, otherwise a particularly
    # namespace-heavy index task might have more scopes than can fit in a
    # temporary credential.
    scopes = set()
    for path in index_paths:
        scope = 'index:insert-task:{}'.format(path)
        for summ_re in SCOPE_SUMMARY_REGEXPS:
            match = summ_re.match(scope)
            if match:
                scope = match.group(1) + '*'
                break
        scopes.add(scope)
    task.task['scopes'] = sorted(scopes)

    task.task['payload']['command'] = ['insert-indexes.js'] + index_paths
    task.task['payload']['env'] = {"TARGET_TASKID": parent_task.task_id}
    return task


def add_index_tasks(taskgraph, label_to_taskid):
    """
    The TaskCluster queue only allows 10 routes on a task, but we have tasks
    with many more routes, for purposes of indexing. This graph morph adds
    "index tasks" that depend on such tasks and do the index insertions
    directly, avoiding the limits on task.routes.
    """
    logger.debug('Morphing: adding index tasks')

    added = []
    for label, task in taskgraph.tasks.iteritems():
        if len(task.task.get('routes', [])) <= MAX_ROUTES:
            continue
        added.append(make_index_task(task, taskgraph, label_to_taskid))

    if added:
        taskgraph, label_to_taskid = amend_taskgraph(
            taskgraph, label_to_taskid, added)
        logger.info('Added {} index tasks'.format(len(added)))

    return taskgraph, label_to_taskid


class apply_jsone_templates(object):
    """Apply a set of JSON-e templates to each task's `task` attribute.

    :param templates: A dict with the template name as the key, and extra context
                      to use (in addition to task.to_json()) as the value.
    """
    template_dir = os.path.join(here, 'templates')

    def __init__(self, try_task_config):
        self.templates = try_task_config.get('templates')
        self.target_tasks = try_task_config.get('tasks')

    def __call__(self, taskgraph, label_to_taskid):
        if not self.templates:
            return taskgraph, label_to_taskid

        for task in taskgraph.tasks.itervalues():
            for template in sorted(self.templates):
                context = {
                    'task': task.task,
                    'taskGroup': None,
                    'taskId': task.task_id,
                    'kind': task.kind,
                    'input': self.templates[template],
                    # The following context differs from action tasks
                    'attributes': task.attributes,
                    'label': task.label,
                    'target_tasks': self.target_tasks,
                }

                template_path = os.path.join(self.template_dir, template + '.yml')
                with open(template_path) as f:
                    template = yaml.safe_load(f)
                result = jsone.render(template, context) or {}
                for attr in ('task', 'attributes'):
                    if attr in result:
                        setattr(task, attr, result[attr])

        return taskgraph, label_to_taskid


def morph(taskgraph, label_to_taskid, parameters):
    """Apply all morphs"""
    morphs = [
        add_index_tasks,
    ]
    if parameters['try_mode'] == 'try_task_config':
        morphs.append(apply_jsone_templates(parameters['try_task_config']))

    for m in morphs:
        taskgraph, label_to_taskid = m(taskgraph, label_to_taskid)
    return taskgraph, label_to_taskid
