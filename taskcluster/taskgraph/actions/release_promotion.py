# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from .registry import register_callback_action

from .util import (find_decision_task, find_existing_tasks_from_previous_kinds,
                   find_hg_revision_pushlog_id)
from taskgraph.util.taskcluster import get_artifact
from taskgraph.taskgraph import TaskGraph
from taskgraph.decision import taskgraph_decision
from taskgraph.parameters import Parameters
from taskgraph.util.attributes import RELEASE_PROMOTION_PROJECTS

RELEASE_PROMOTION_CONFIG = {
    'promote_fennec': {
        'target_tasks_method': 'candidates_fennec',
        'previous_graph_kinds': [
            'build', 'build-signing', 'repackage', 'repackage-signing',
            "beetmover", "beetmover-checksums", "checksums-signing",
            "nightly-l10n", "nightly-l10n-signing", "release-bouncer-sub",
            "upload-generated-sources", "upload-symbols",
        ],
        'do_not_optimize': [],
    },
    'publish_fennec': {
        'target_tasks_method': 'publish_fennec',
        'previous_graph_kinds': [
            'build', 'build-signing', 'repackage', 'repackage-signing',
        ],
        'do_not_optimize': [],
    },
    'promote_firefox': {
        'target_tasks_method': '%(project)s_desktop_promotion',
        'previous_graph_kinds': [
            'build', 'build-signing', 'repackage', 'repackage-signing',
        ],
        'do_not_optimize': [],
    },
    'publish_firefox': {
        'target_tasks_method': 'publish_firefox',
        'previous_graph_kinds': [
            'build', 'build-signing', 'repackage', 'repackage-signing',
            'nightly-l10n', 'nightly-l10n-signing', 'repackage-l10n',
        ],
        'do_not_optimize': [],
    },
}


def is_release_promotion_available(parameters):
    return parameters['project'] in RELEASE_PROMOTION_PROJECTS


@register_callback_action(
    name='release-promotion',
    title='Release Promotion',
    symbol='Relpro',
    description="Promote a release.",
    order=10000,
    context=[],
    available=is_release_promotion_available,
    schema={
        'type': 'object',
        'properties': {
            'build_number': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'title': 'The release build number',
                'description': ('The release build number. Starts at 1 per '
                                'release version, and increments on rebuild.'),
            },
            'do_not_optimize': {
                'type': 'array',
                'description': ('Optional: a list of labels to avoid optimizing out '
                                'of the graph (to force a rerun of, say, '
                                'funsize docker-image tasks).'),
                'items': {
                    'type': 'string',
                },
            },
            'revision': {
                'type': 'string',
                'title': 'Optional: revision to promote',
                'description': ('Optional: the revision to promote. If specified, '
                                'and if neither `pushlog_id` nor `previous_graph_kinds` '
                                'is specified, find the `pushlog_id using the '
                                'revision.'),
            },
            'release_promotion_flavor': {
                'type': 'string',
                'description': 'The flavor of release promotion to perform.',
                'enum': sorted(RELEASE_PROMOTION_CONFIG.keys()),
            },
            'target_tasks_method': {
                'type': 'string',
                'title': 'target task method',
                'description': ('Optional: the target task method to use to generate '
                                'the new graph.'),
            },
            'previous_graph_kinds': {
                'type': 'array',
                'description': ('Optional: an array of kinds to use from the previous '
                                'graph(s).'),
                'items': {
                    'type': 'string',
                },
            },
            'previous_graph_ids': {
                'type': 'array',
                'description': ('Optional: an array of taskIds of decision or action '
                                'tasks from the previous graph(s) to use to populate '
                                'our `previous_graph_kinds`.'),
                'items': {
                    'type': 'string',
                },
            },
        },
        "required": ['release_promotion_flavor', 'build_number'],
    }
)
def release_promotion_action(parameters, input, task_group_id, task_id, task):
    os.environ['BUILD_NUMBER'] = str(input['build_number'])
    release_promotion_flavor = input['release_promotion_flavor']
    promotion_config = RELEASE_PROMOTION_CONFIG[release_promotion_flavor]

    target_tasks_method = input.get(
        'target_tasks_method', promotion_config['target_tasks_method']
    )
    previous_graph_kinds = input.get(
        'previous_graph_kinds', promotion_config['previous_graph_kinds']
    )
    do_not_optimize = input.get(
        'do_not_optimize', promotion_config['do_not_optimize']
    )

    # make parameters read-write
    parameters = dict(parameters)
    # Build previous_graph_ids from ``previous_graph_ids``, ``pushlog_id``,
    # or ``revision``.
    previous_graph_ids = input.get('previous_graph_ids')
    if not previous_graph_ids:
        revision = input.get('revision')
        parameters['pushlog_id'] = parameters['pushlog_id'] or \
            find_hg_revision_pushlog_id(parameters, revision)
        previous_graph_ids = [find_decision_task(parameters)]

    # Download parameters and full task graph from the first decision task.
    parameters = get_artifact(previous_graph_ids[0], "public/parameters.yml")
    full_task_graph = get_artifact(previous_graph_ids[0], "public/full-task-graph.json")
    _, full_task_graph = TaskGraph.from_json(full_task_graph)
    parameters['existing_tasks'] = find_existing_tasks_from_previous_kinds(
        full_task_graph, previous_graph_ids, previous_graph_kinds
    )
    parameters['do_not_optimize'] = do_not_optimize
    parameters['target_tasks_method'] = target_tasks_method

    # make parameters read-only
    parameters = Parameters(**parameters)

    taskgraph_decision({}, parameters=parameters)
