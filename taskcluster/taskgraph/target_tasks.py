# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

_target_task_methods = {}
def _target_task(name):
    def wrap(func):
        _target_task_methods[name] = func
        return func
    return wrap

def get_method(method):
    """Get a target_task_method to pass to a TaskGraphGenerator."""
    return _target_task_methods[method]

@_target_task('from_parameters')
def target_tasks_from_parameters(full_task_graph, parameters):
    """Get the target task set from parameters['target_tasks'].  This is
    useful for re-running a decision task with the same target set as in an
    earlier run, by copying `target_tasks.json` into `parameters.yml`."""
    return parameters['target_tasks']

@_target_task('try_option_syntax')
def target_tasks_try_option_syntax(full_task_graph, parameters):
    """Generate a list of target tasks based on try syntax in
    parameters['message'] and, for context, the full task graph."""
    from taskgraph.try_option_syntax import TryOptionSyntax
    options = TryOptionSyntax(parameters['message'], full_task_graph)
    return [t.label for t in full_task_graph.tasks.itervalues()
            if options.task_matches(t.attributes)]

@_target_task('all_tasks')
def target_tasks_all_tasks(full_task_graph, parameters):
    """Trivially target all tasks."""
    return full_task_graph.tasks.keys()

