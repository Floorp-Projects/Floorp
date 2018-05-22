# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import re
import yaml
from slugid import nice as slugid
from types import FunctionType
from collections import namedtuple
from taskgraph import create
from taskgraph.config import load_graph_config
from taskgraph.util import taskcluster
from taskgraph.parameters import Parameters


actions = []
callbacks = {}

Action = namedtuple('Action', ['order', 'action_builder'])


def is_json(data):
    """ Return ``True``, if ``data`` is a JSON serializable data structure. """
    try:
        json.dumps(data)
    except ValueError:
        return False
    return True


def register_callback_action(name, title, symbol, description, order=10000,
                             context=[], available=lambda parameters: True,
                             schema=None, kind='task', generic=True):
    """
    Register an action callback that can be triggered from supporting
    user interfaces, such as Treeherder.

    This function is to be used as a decorator for a callback that takes
    parameters as follows:

    ``parameters``:
        Decision task parameters, see ``taskgraph.parameters.Parameters``.
    ``input``:
        Input matching specified JSON schema, ``None`` if no ``schema``
        parameter is given to ``register_callback_action``.
    ``task_group_id``:
        The id of the task-group this was triggered for.
    ``task_id`` and `task``:
        task identifier and task definition for task the action was triggered
        for, ``None`` if no ``context`` parameters was given to
        ``register_callback_action``.

    Parameters
    ----------
    name : str
        An identifier for this action, used by UIs to find the action.
    title : str
        A human readable title for the action to be used as label on a button
        or text on a link for triggering the action.
    symbol : str
        Treeherder symbol for the action callback, this is the symbol that the
        task calling your callback will be displayed as. This is usually 1-3
        letters abbreviating the action title.
    description : str
        A human readable description of the action in **markdown**.
        This will be display as tooltip and in dialog window when the action
        is triggered. This is a good place to describe how to use the action.
    order : int
        Order of the action in menus, this is relative to the ``order`` of
        other actions declared.
    context : list of dict
        List of tag-sets specifying which tasks the action is can take as input.
        If no tag-sets is specified as input the action is related to the
        entire task-group, and won't be triggered with a given task.

        Otherwise, if ``context = [{'k': 'b', 'p': 'l'}, {'k': 't'}]`` will only
        be displayed in the context menu for tasks that has
        ``task.tags.k == 'b' && task.tags.p = 'l'`` or ``task.tags.k = 't'``.
        Esentially, this allows filtering on ``task.tags``.
    available : function
        An optional function that given decision parameters decides if the
        action is available. Defaults to a function that always returns ``True``.
    schema : dict
        JSON schema specifying input accepted by the action.
        This is optional and can be left ``null`` if no input is taken.
    kind : string
        The action kind to define - must be one of `task` or `hook`.  Only for
        transitional purposes.
    generic : boolean
        For kind=hook, whether this is a generic action or has its own permissions.

    Returns
    -------
    function
        To be used as decorator for the callback function.
    """
    mem = {"registered": False}  # workaround nonlocal missing in 2.x

    assert isinstance(title, basestring), 'title must be a string'
    assert isinstance(description, basestring), 'description must be a string'
    title = title.strip()
    description = description.strip()

    def register_callback(cb):
        assert isinstance(name, basestring), 'name must be a string'
        assert isinstance(order, int), 'order must be an integer'
        assert kind in ('task', 'hook'), 'kind must be task or hook'
        assert callable(schema) or is_json(schema), 'schema must be a JSON compatible object'
        assert isinstance(cb, FunctionType), 'callback must be a function'
        # Allow for json-e > 25 chars in the symbol.
        if '$' not in symbol:
            assert 1 <= len(symbol) <= 25, 'symbol must be between 1 and 25 characters'
        assert isinstance(symbol, basestring), 'symbol must be a string'

        assert not mem['registered'], 'register_callback_action must be used as decorator'
        assert cb.__name__ not in callbacks, 'callback name {} is not unique'.format(cb.__name__)

        def action_builder(parameters, graph_config):
            if not available(parameters):
                return None

            actionPerm = 'generic' if generic else name

            # gather up the common decision-task-supplied data for this action
            repo_param = '{}head_repository'.format(graph_config['project-repo-param-prefix'])
            repository = {
                'url': parameters[repo_param],
                'project': parameters['project'],
                'level': parameters['level'],
            }

            revision = parameters['{}head_rev'.format(graph_config['project-repo-param-prefix'])]
            push = {
                'owner': 'mozilla-taskcluster-maintenance@mozilla.com',
                'pushlog_id': parameters['pushlog_id'],
                'revision': revision,
            }

            task_group_id = os.environ.get('TASK_ID', slugid())
            match = re.match(r'https://(hg.mozilla.org)/(.*?)/?$', parameters[repo_param])
            if not match:
                raise Exception('Unrecognized {}'.format(repo_param))
            action = {
                'name': name,
                'title': title,
                'description': description,
                'taskGroupId': task_group_id,
                'cb_name': cb.__name__,
                'symbol': symbol,
            }

            rv = {
                'name': name,
                'title': title,
                'description': description,
                'context': context,
            }
            if schema:
                rv['schema'] = schema(graph_config=graph_config) if callable(schema) else schema

            # for kind=task, we embed the task from .taskcluster.yml in the action, with
            # suitable context
            if kind == 'task':
                template = graph_config.taskcluster_yml

                # tasks get all of the scopes the original push did, yuck; this is not
                # done with kind = hook.
                repo_scope = 'assume:repo:{}/{}:branch:default'.format(
                    match.group(1), match.group(2))
                action['repo_scope'] = repo_scope

                with open(template, 'r') as f:
                    taskcluster_yml = yaml.safe_load(f)
                    if taskcluster_yml['version'] != 1:
                        raise Exception(
                            'actions.json must be updated to work with .taskcluster.yml')
                    if not isinstance(taskcluster_yml['tasks'], list):
                        raise Exception(
                            '.taskcluster.yml "tasks" must be a list for action tasks')

                rv.update({
                    'kind': 'task',
                    'task': {
                        '$let': {
                            'tasks_for': 'action',
                            'repository': repository,
                            'push': push,
                            'action': action,
                        },
                        'in': taskcluster_yml['tasks'][0],
                    },
                })

            # for kind=hook
            elif kind == 'hook':
                trustDomain = graph_config['trust-domain']
                level = parameters['level']
                rv.update({
                    'kind': 'hook',
                    'hookGroupId': 'project-{}'.format(trustDomain),
                    'hookId': 'in-tree-action-{}-{}'.format(level, actionPerm),
                    'hookPayload': {
                        # provide the decision-task parameters as context for triggerHook
                        "decision": {
                            'action': action,
                            'repository': repository,
                            'push': push,
                            # parameters is long, so fetch it from the actions.json variables
                            'parameters': {'$eval': 'parameters'},
                        },

                        # and pass everything else through from our own context
                        "user": {
                            'input': {'$eval': 'input'},
                            'taskId': {'$eval': 'taskId'},
                            'taskGroupId': {'$eval': 'taskGroupId'},
                        }
                    },
                })

            return rv

        actions.append(Action(order, action_builder))

        mem['registered'] = True
        callbacks[cb.__name__] = cb
    return register_callback


def render_actions_json(parameters, graph_config):
    """
    Render JSON object for the ``public/actions.json`` artifact.

    Parameters
    ----------
    parameters : taskgraph.parameters.Parameters
        Decision task parameters.

    Returns
    -------
    dict
        JSON object representation of the ``public/actions.json`` artifact.
    """
    assert isinstance(parameters, Parameters), 'requires instance of Parameters'
    actions = []
    for action in sorted(_get_actions(graph_config), key=lambda action: action.order):
        action = action.action_builder(parameters, graph_config)
        if action:
            assert is_json(action), 'action must be a JSON compatible object'
            actions.append(action)
    return {
        'version': 1,
        'variables': {
            'parameters': dict(**parameters),
        },
        'actions': actions,
    }


def trigger_action_callback(task_group_id, task_id, input, callback, parameters, root,
                            test=False):
    """
    Trigger action callback with the given inputs. If `test` is true, then run
    the action callback in testing mode, without actually creating tasks.
    """
    graph_config = load_graph_config(root)
    callbacks = _get_callbacks(graph_config)
    cb = callbacks.get(callback, None)
    if not cb:
        raise Exception('Unknown callback: {}. Known callbacks: {}'.format(
            callback, callbacks))

    if test:
        create.testing = True
        taskcluster.testing = True

    # fetch the task, if taskId was given
    # FIXME: many actions don't need this, so move this fetch into the callbacks
    # that do need it
    if task_id:
        task = taskcluster.get_task_definition(task_id)
    else:
        task = None

    cb(Parameters(**parameters), graph_config, input, task_group_id, task_id, task)


def _load(graph_config):
    # Load all modules from this folder, relying on the side-effects of register_
    # functions to populate the action registry.
    actions_dir = os.path.dirname(__file__)
    for f in os.listdir(actions_dir):
        if f.endswith('.py') and f not in ('__init__.py', 'registry.py', 'util.py'):
            __import__('taskgraph.actions.' + f[:-3])
    return callbacks, actions


def _get_callbacks(graph_config):
    return _load(graph_config)[0]


def _get_actions(graph_config):
    return _load(graph_config)[1]
