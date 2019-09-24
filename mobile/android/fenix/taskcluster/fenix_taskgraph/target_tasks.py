# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re

from taskgraph.target_tasks import _target_task, filter_for_tasks_for


BETA_SEMVER = re.compile(r'^v\d+\.\d+\.\d+-beta\.\d+$')
PRODUCTION_SEMVER = re.compile(r'^v\d+\.\d+\.\d+(-rc\.\d+)?$')


@_target_task('default')
def target_tasks_default(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on this project
    via the `run_on_projects` attributes."""

    filter = filter_for_tasks_for
    if parameters["tasks_for"] == 'github-release':
        # TODO Move GIT_TAG as to a parameter
        git_tag = os.environ['GIT_TAG']
        version = git_tag[1:]  # remove prefixed "v"

        if BETA_SEMVER.match(git_tag):
            def filter(task, params):
                return task.attributes.get("release-type", "") == "beta"
        elif PRODUCTION_SEMVER.match(git_tag):
            def filter(task, params):
                return task.attributes.get("release-type", "") == "production"
        else:
            raise ValueError('Github tag must be in semver format and prefixed with a "v", '
                             'e.g.: "v1.0.0-beta.0" (beta), "v1.0.0-rc.0" (production) or "v1.0.0" (production)')

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task("nightly")
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    """Select the set of tasks required for a nightly build."""

    def filter(task, parameters):
        return task.attributes.get("nightly", False)

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]


@_target_task('raptor')
def target_tasks_raptor(full_task_graph, parameters, graph_config):
    def filter(task, params):
        return task.kind == 'raptor'

    return [l for l, t in full_task_graph.tasks.iteritems() if filter(t, parameters)]
