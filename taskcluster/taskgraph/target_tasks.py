# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
from taskgraph import try_option_syntax
from taskgraph.util.attributes import attrmatch

BUILD_AND_TEST_KINDS = set([
    'legacy',  # builds
    'desktop-test',
    'android-test',
])

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
    options = try_option_syntax.TryOptionSyntax(parameters['message'], full_task_graph)
    target_tasks_labels = [t.label for t in full_task_graph.tasks.itervalues()
                           if options.task_matches(t.attributes)]

    # If the developer wants test jobs to be rebuilt N times we add that value here
    if int(options.trigger_tests) > 1:
        for l in target_tasks_labels:
            task = full_task_graph[l]
            if 'unittest_suite' in task.attributes:
                task.attributes['task_duplicates'] = options.trigger_tests

    return target_tasks_labels


@_target_task('all_builds_and_tests')
def target_tasks_all_builds_and_tests(full_task_graph, parameters):
    """Trivially target all build and test tasks.  This is used for
    branches where we want to build "everyting", but "everything"
    does not include uninteresting things like docker images"""
    def filter(task):
        return t.attributes.get('kind') in BUILD_AND_TEST_KINDS
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('ash_tasks')
def target_tasks_ash_tasks(full_task_graph, parameters):
    """Special case for builds on ash."""
    def filter(task):
        # NOTE: on the ash branch, update taskcluster/ci/desktop-test/tests.yml to
        # run the M-dt-e10s tasks
        attrs = t.attributes
        if attrs.get('kind') not in BUILD_AND_TEST_KINDS:
            return False
        if not attrmatch(attrs, build_platform=set([
            'linux64',
            'linux64-asan',
            'linux64-pgo',
        ])):
            return False
        if not attrmatch(attrs, e10s=True):
            return False
        return True
    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t)]


@_target_task('nightly_fennec')
def target_tasks_nightly(full_task_graph, parameters):
    """Select the set of tasks required for a nightly build of fennec. The
    nightly build process involves a pipeline of builds, signing,
    and, eventually, uploading the tasks to balrog."""
    return [t.label for t in full_task_graph.tasks.itervalues()
            if t.attributes.get('kind') in ['nightly-fennec', 'signing']]
