# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import os
from types import FunctionType
from collections import namedtuple


from taskgraph import create
from taskgraph.config import load_graph_config
from taskgraph.util import taskcluster, yaml, hash
from taskgraph.parameters import Parameters
from taskgraph.util.memoize import memoize


actions = []
callbacks = {}

Action = namedtuple("Action", ["order", "cb_name", "generic", "action_builder"])


def is_json(data):
    """Return ``True``, if ``data`` is a JSON serializable data structure."""
    try:
        json.dumps(data)
    except ValueError:
        return False
    return True


@memoize
def read_taskcluster_yml(filename):
    """Load and parse .taskcluster.yml, memoized to save some time"""
    return yaml.load_yaml(filename)


@memoize
def hash_taskcluster_yml(filename):
    """
    Generate a hash of the given .taskcluster.yml.  This is the first 10 digits
    of the sha256 of the file's content, and is used by administrative scripts
    to create a hook based on this content.
    """
    return hash.hash_path(filename)[:10]


def register_callback_action(
    name,
    title,
    symbol,
    description,
    order=10000,
    context=[],
    available=lambda parameters: True,
    schema=None,
    generic=True,
    cb_name=None,
):
    """
    Register an action callback that can be triggered from supporting
    user interfaces, such as Treeherder.

    This function is to be used as a decorator for a callback that takes
    parameters as follows:

    ``parameters``:
        Decision task :class:`parameters <taskgraph.parameters.Parameters>`.
    ``input``:
        Input matching specified JSON schema, ``None`` if no ``schema``
        parameter is given to ``register_callback_action``.
    ``task_group_id``:
        The id of the task-group this was triggered for.
    ``task_id`` and `task``:
        task identifier and task definition for task the action was triggered
        for, ``None`` if no ``context`` parameters was given to
        ``register_callback_action``.

    Args:
        name (str):
            An identifier for this action, used by UIs to find the action.
        title (str):
            A human readable title for the action to be used as label on a button
            or text on a link for triggering the action.
        symbol (str):
            Treeherder symbol for the action callback, this is the symbol that the
            task calling your callback will be displayed as. This is usually 1-3
            letters abbreviating the action title.
        description (str):
            A human readable description of the action in **markdown**.
            This will be display as tooltip and in dialog window when the action
            is triggered. This is a good place to describe how to use the action.
        order (int):
            Order of the action in menus, this is relative to the ``order`` of
            other actions declared.
        context (list of dict):
            List of tag-sets specifying which tasks the action is can take as input.
            If no tag-sets is specified as input the action is related to the
            entire task-group, and won't be triggered with a given task.

            Otherwise, if ``context = [{'k': 'b', 'p': 'l'}, {'k': 't'}]`` will only
            be displayed in the context menu for tasks that has
            ``task.tags.k == 'b' && task.tags.p = 'l'`` or ``task.tags.k = 't'``.
            Esentially, this allows filtering on ``task.tags``.

            If this is a function, it is given the decision parameters and must return
            a value of the form described above.
        available (function):
            An optional function that given decision parameters decides if the
            action is available. Defaults to a function that always returns ``True``.
        schema (dict):
            JSON schema specifying input accepted by the action.
            This is optional and can be left ``null`` if no input is taken.
        generic (bool)
            Whether this is a generic action or has its own permissions.
        cb_name (str):
            The name under which this function should be registered, defaulting to
            `name`.  This is used to generation actionPerm for non-generic hook
            actions, and thus appears in ci-configuration and various role and hook
            names.  Unlike `name`, which can appear multiple times, cb_name must be
            unique among all registered callbacks.

    Returns:
        function: Decorator to be used for the callback function.
    """
    mem = {"registered": False}  # workaround nonlocal missing in 2.x

    assert isinstance(title, str), "title must be a string"
    assert isinstance(description, str), "description must be a string"
    title = title.strip()
    description = description.strip()

    # ensure that context is callable
    if not callable(context):
        context_value = context
        context = lambda params: context_value  # noqa

    def register_callback(cb, cb_name=cb_name):
        assert isinstance(name, str), "name must be a string"
        assert isinstance(order, int), "order must be an integer"
        assert callable(schema) or is_json(
            schema
        ), "schema must be a JSON compatible object"
        assert isinstance(cb, FunctionType), "callback must be a function"
        # Allow for json-e > 25 chars in the symbol.
        if "$" not in symbol:
            assert 1 <= len(symbol) <= 25, "symbol must be between 1 and 25 characters"
        assert isinstance(symbol, str), "symbol must be a string"

        assert not mem[
            "registered"
        ], "register_callback_action must be used as decorator"
        if not cb_name:
            cb_name = name
        assert cb_name not in callbacks, "callback name {} is not unique".format(
            cb_name
        )

        def action_builder(parameters, graph_config, decision_task_id):
            if not available(parameters):
                return None

            actionPerm = "generic" if generic else cb_name

            # gather up the common decision-task-supplied data for this action
            repo_param = "head_repository"
            repository = {
                "url": parameters[repo_param],
                "project": parameters["project"],
                "level": parameters["level"],
            }

            revision = parameters["head_rev"]
            push = {
                "owner": "mozilla-taskcluster-maintenance@mozilla.com",
                "pushlog_id": parameters["pushlog_id"],
                "revision": revision,
            }
            branch = parameters.get("head_ref")
            if branch:
                push["branch"] = branch

            action = {
                "name": name,
                "title": title,
                "description": description,
                # target taskGroupId (the task group this decision task is creating)
                "taskGroupId": decision_task_id,
                "cb_name": cb_name,
                "symbol": symbol,
            }

            rv = {
                "name": name,
                "title": title,
                "description": description,
                "context": context(parameters),
            }
            if schema:
                rv["schema"] = (
                    schema(graph_config=graph_config) if callable(schema) else schema
                )

            trustDomain = graph_config["trust-domain"]
            level = parameters["level"]
            tcyml_hash = hash_taskcluster_yml(graph_config.taskcluster_yml)

            # the tcyml_hash is prefixed with `/` in the hookId, so users will be granted
            # hooks:trigger-hook:project-gecko/in-tree-action-3-myaction/*; if another
            # action was named `myaction/release`, then the `*` in the scope would also
            # match that action.  To prevent such an accident, we prohibit `/` in hook
            # names.
            if "/" in actionPerm:
                raise Exception("`/` is not allowed in action names; use `-`")

            rv.update(
                {
                    "kind": "hook",
                    "hookGroupId": f"project-{trustDomain}",
                    "hookId": "in-tree-action-{}-{}/{}".format(
                        level, actionPerm, tcyml_hash
                    ),
                    "hookPayload": {
                        # provide the decision-task parameters as context for triggerHook
                        "decision": {
                            "action": action,
                            "repository": repository,
                            "push": push,
                        },
                        # and pass everything else through from our own context
                        "user": {
                            "input": {"$eval": "input"},
                            "taskId": {"$eval": "taskId"},  # target taskId (or null)
                            "taskGroupId": {
                                "$eval": "taskGroupId"
                            },  # target task group
                        },
                    },
                    "extra": {
                        "actionPerm": actionPerm,
                    },
                }
            )

            return rv

        actions.append(Action(order, cb_name, generic, action_builder))

        mem["registered"] = True
        callbacks[cb_name] = cb
        return cb

    return register_callback


def render_actions_json(parameters, graph_config, decision_task_id):
    """
    Render JSON object for the ``public/actions.json`` artifact.

    Args:
        parameters (:class:`~taskgraph.parameters.Parameters`):
            Decision task parameters.

    Returns:
        dict:
            JSON object representation of the ``public/actions.json``
            artifact.
    """
    assert isinstance(parameters, Parameters), "requires instance of Parameters"
    actions = []
    for action in sorted(_get_actions(graph_config), key=lambda action: action.order):
        action = action.action_builder(parameters, graph_config, decision_task_id)
        if action:
            assert is_json(action), "action must be a JSON compatible object"
            actions.append(action)
    return {
        "version": 1,
        "variables": {},
        "actions": actions,
    }


def sanity_check_task_scope(callback, parameters, graph_config):
    """
    If this action is not generic, then verify that this task has the necessary
    scope to run the action. This serves as a backstop preventing abuse by
    running non-generic actions using generic hooks. While scopes should
    prevent serious damage from such abuse, it's never a valid thing to do.
    """
    for action in _get_actions(graph_config):
        if action.cb_name == callback:
            break
    else:
        raise Exception(f"No action with cb_name {callback}")

    actionPerm = "generic" if action.generic else action.cb_name

    repo_param = "head_repository"
    head_repository = parameters[repo_param]
    if not head_repository.startswith(("https://hg.mozilla.org", "https://github.com")):
        raise Exception(
            "{} is not either https://hg.mozilla.org or https://github.com !"
        )

    expected_scope = f"assume:repo:{head_repository[8:]}:action:{actionPerm}"

    # the scope should appear literally; no need for a satisfaction check. The use of
    # get_current_scopes here calls the auth service through the Taskcluster Proxy, giving
    # the precise scopes available to this task.
    if expected_scope not in taskcluster.get_current_scopes():
        raise Exception(f"Expected task scope {expected_scope} for this action")


def trigger_action_callback(
    task_group_id, task_id, input, callback, parameters, root, test=False
):
    """
    Trigger action callback with the given inputs. If `test` is true, then run
    the action callback in testing mode, without actually creating tasks.
    """
    graph_config = load_graph_config(root)
    graph_config.register()
    callbacks = _get_callbacks(graph_config)
    cb = callbacks.get(callback, None)
    if not cb:
        raise Exception(
            "Unknown callback: {}. Known callbacks: {}".format(
                callback, ", ".join(callbacks)
            )
        )

    if test:
        create.testing = True
        taskcluster.testing = True

    if not test:
        sanity_check_task_scope(callback, parameters, graph_config)

    cb(Parameters(**parameters), graph_config, input, task_group_id, task_id)


def _load(graph_config):
    # Load all modules from this folder, relying on the side-effects of register_
    # functions to populate the action registry.
    actions_dir = os.path.dirname(__file__)
    for f in os.listdir(actions_dir):
        if f.endswith(".py") and f not in ("__init__.py", "registry.py", "util.py"):
            __import__("taskgraph.actions." + f[:-3])
    return callbacks, actions


def _get_callbacks(graph_config):
    return _load(graph_config)[0]


def _get_actions(graph_config):
    return _load(graph_config)[1]
